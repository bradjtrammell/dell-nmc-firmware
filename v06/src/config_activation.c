#include "ou_config_activation.h"

static int same_model(const ou_config_model *a,const ou_config_model *b){
    unsigned i;
    size_t j;
    if(!a||!b)return 0;
    for(i=0U;i<OU_CONFIG_FIELD_COUNT;++i){
        if(a->present[i]!=b->present[i])return 0;
        for(j=0U;j<OU_CONFIG_VALUE_BYTES;++j)
            if(a->values[i][j]!=b->values[i][j])return 0;
    }
    return 1;
}
static int qualified(const ou_config_activation *o){
    const ou_config_activation_adapter *a;
    if(!o||!o->config_partition)return 0;
    a=&o->adapter;
    return o->config_partition->geometry_verified==1U &&
           o->config_partition->dedicated_slots_verified==1U &&
           o->config_partition->writes_enabled==1U &&
           o->config_partition->read &&
           o->config_partition->program &&
           o->config_partition->erase &&
           a->physical_network_verified==1U &&
           a->rollback_channel_verified==1U &&
           a->authenticated_session_verified==1U &&
           a->apply && a->restore && a->probe && a->authorize;
}
static int authorized(ou_config_activation *o){
    return qualified(o) && o->adapter.authorize(o->adapter.context)==1;
}
static int elapsed(uint32_t now,uint32_t then,uint32_t *delta){
    *delta=now-then;
    return *delta<UINT32_C(0x80000000);
}
static int disk_is_original(ou_config_activation *o) {
    ou_config_model persisted;
    ou_journal_record current;
    if(ou_config_model_load(&persisted,o->config_partition,&current)!=0)
        return 0;
    return same_model(&persisted,&o->active) &&
           current.valid &&
           current.generation==o->active_record.generation &&
           current.slot==o->active_record.slot;
}
/* Commit can fail after its final marker was written: re-read both
 * journal slots rather than assuming that a failed return left the disk
 * untouched. Restore previously persisted settings when necessary. */
static int ensure_previous_persisted(ou_config_activation *o){
    ou_config_model persisted;
    ou_journal_record current,restored;
    if(ou_config_model_load(&persisted,o->config_partition,&current)!=0)
        return -1;
    if(same_model(&persisted,&o->active))return 0;
    /* Only a transaction WE initiated may have written candidate.
     * Refuse to overwrite an unrelated later configuration. */
    if(!same_model(&persisted,&o->candidate))return -1;
    if(!authorized(o) ||
       ou_config_model_save(&o->active,o->config_partition,
                            o->adapter.authorize,o->adapter.context,
                            &restored)!=0)return -1;
    if(ou_config_model_load(&persisted,o->config_partition,&current)!=0 ||
       !same_model(&persisted,&o->active))return -1;
    o->active_record=current;
    return 0;
}
static int rollback(ou_config_activation *o,uint8_t cause){
    int hardware,storage;
    hardware=o->adapter.restore(o->adapter.context,&o->active);
    storage=ensure_previous_persisted(o);
    if(hardware!=0 || storage!=0){
        o->state=OU_CONFIG_ACT_FAILED_ROLLBACK;
        o->last_error=OU_CONFIG_ACT_ERR_RESTORE;
        return -1;
    }
    o->state=OU_CONFIG_ACT_ROLLED_BACK;
    o->last_error=cause;
    if(o->rolled_back_changes!=UINT32_MAX)
        ++o->rolled_back_changes;
    return 1;
}
int ou_config_activation_init(ou_config_activation *o,
                              const ou_journal_io *io,
                              const ou_config_activation_adapter *adapter) {
    ou_config_activation draft={0};
    if(!o||!io||!adapter||!adapter->apply||!adapter->restore||
       !adapter->probe||!adapter->authorize||
       adapter->physical_network_verified!=1U||
       adapter->rollback_channel_verified!=1U||
       adapter->authenticated_session_verified!=1U)
        return -1;
    draft.config_partition=io;
    draft.adapter=*adapter;
    if(ou_config_model_load(&draft.active,io,&draft.active_record)!=0 ||
       ou_config_model_validate(&draft.active)!=0)return -1;
    draft.state=OU_CONFIG_ACT_IDLE;
    *o=draft;
    return 0;
}
int ou_config_activation_begin(ou_config_activation *o,
                               const ou_config_model *candidate,
                               uint32_t now_ms,uint32_t timeout_ms){
    uint32_t delta;
    if(!o||!candidate || !qualified(o) ||
       (o->state!=OU_CONFIG_ACT_IDLE &&
        o->state!=OU_CONFIG_ACT_CONFIRMED &&
        o->state!=OU_CONFIG_ACT_ROLLED_BACK) ||
       timeout_ms<OU_CONFIG_CONFIRM_MIN_MS ||
       timeout_ms>OU_CONFIG_CONFIRM_MAX_MS ||
       ou_config_model_validate(candidate)!=0||
       same_model(&o->active,candidate)) {
        if(o)o->last_error=OU_CONFIG_ACT_ERR_INVALID;
        return -1;
    }
    /* A prior lease and known active journal are prerequisites.
     * Prevent replaying changes against a concurrently modified config. */
    if(!disk_is_original(o)){
        o->last_error=OU_CONFIG_ACT_ERR_STALE;
        return -1;
    }
    if(!authorized(o)){
        o->last_error=OU_CONFIG_ACT_ERR_UNAUTHORIZED;
        return -1;
    }
    if(o->adapter.probe(o->adapter.context)!=1){
        o->last_error=OU_CONFIG_ACT_ERR_CONNECTIVITY;
        return -1;
    }
    (void)delta;
    o->candidate=*candidate;
    o->started_ms=now_ms;
    o->timeout_ms=timeout_ms;
    o->last_error=OU_CONFIG_ACT_ERR_NONE;
    /* State is pending BEFORE callback execution: a partially applied
     * hardware change must be rolled back, never treated as idle. */
    o->state=OU_CONFIG_ACT_PENDING;
    if(o->adapter.apply(o->adapter.context,&o->candidate)!=0){
        (void)rollback(o,OU_CONFIG_ACT_ERR_APPLY);
        return -1;
    }
    return 0;
}
int ou_config_activation_step(ou_config_activation *o,uint32_t now_ms){
    uint32_t delta;
    int probe;
    if(!o||o->state!=OU_CONFIG_ACT_PENDING)return -1;
    if(!elapsed(now_ms,o->started_ms,&delta) || delta>=o->timeout_ms)
        return rollback(o,OU_CONFIG_ACT_ERR_TIMEOUT);
    probe=o->adapter.probe(o->adapter.context);
    if(probe<0)return rollback(o,OU_CONFIG_ACT_ERR_CONNECTIVITY);
    return 0; /* Offline reconnect can occur during the grace interval. */
}
int ou_config_activation_confirm(ou_config_activation *o,uint32_t now_ms){
    ou_journal_record new_record;
    uint32_t delta;
    int state;
    if(!o||o->state!=OU_CONFIG_ACT_PENDING)return -1;
    if(!elapsed(now_ms,o->started_ms,&delta)||delta>=o->timeout_ms){
        (void)rollback(o,OU_CONFIG_ACT_ERR_TIMEOUT);
        return -1;
    }
    if(!authorized(o)){
        o->last_error=OU_CONFIG_ACT_ERR_UNAUTHORIZED;
        return -1;
    }
    state=o->adapter.probe(o->adapter.context);
    if(state!=1){
        if(state<0)(void)rollback(o,OU_CONFIG_ACT_ERR_CONNECTIVITY);
        else o->last_error=OU_CONFIG_ACT_ERR_CONNECTIVITY;
        return -1;
    }
    if(!disk_is_original(o)){
        (void)rollback(o,OU_CONFIG_ACT_ERR_STALE);
        return -1;
    }
    if(ou_config_model_save(&o->candidate,o->config_partition,
                            o->adapter.authorize,o->adapter.context,
                            &new_record)!=0){
        (void)rollback(o,OU_CONFIG_ACT_ERR_STORAGE);
        return -1;
    }
    o->active=o->candidate;
    o->active_record=new_record;
    o->state=OU_CONFIG_ACT_CONFIRMED;
    o->last_error=OU_CONFIG_ACT_ERR_NONE;
    if(o->confirmed_changes!=UINT32_MAX)++o->confirmed_changes;
    return 0;
}
int ou_config_activation_cancel(ou_config_activation *o){
    if(!o||o->state!=OU_CONFIG_ACT_PENDING)return -1;
    return rollback(o,OU_CONFIG_ACT_ERR_NONE);
}
