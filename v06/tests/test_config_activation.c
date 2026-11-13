#include "ou_config_activation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SLOT_BYTES 8192U
static struct {
    uint8_t slots[2U][SLOT_BYTES];
    int fail_after;
    int fail_read_after_marker;
    int marker_written;
    unsigned writes,erases;
} chip;
static struct {
    ou_config_model network;
    unsigned applies,restores;
    int authorized,probe,fail_apply,fail_restore;
} physical;
static int read_slot(void *ctx,unsigned slot,uint32_t offset,uint8_t *out,size_t len){
    (void)ctx;
    if(slot>1U||offset>SLOT_BYTES||len>SLOT_BYTES-offset)return -1;
    if(chip.marker_written){
        chip.marker_written=0;
        if(chip.fail_read_after_marker){
            chip.fail_read_after_marker=0;
            return -1;
        }
    }
    memcpy(out,chip.slots[slot]+offset,len);
    return 0;
}
static int program_slot(void *ctx,unsigned slot,uint32_t offset,
                        const uint8_t *input,size_t len){
    size_t i;(void)ctx;
    if(slot>1U||offset>SLOT_BYTES||len>SLOT_BYTES-offset)return -1;
    for(i=0U;i<len;++i){
        if(chip.fail_after==0)return -1;
        if(((uint8_t)~chip.slots[slot][offset+i]&input[i])!=0U)return -1;
        chip.slots[slot][offset+i]&=input[i];
        if(chip.fail_after>0)--chip.fail_after;
        ++chip.writes;
    }
    if(offset==20U && len==4U)chip.marker_written=1;
    return 0;
}
static int erase_slot(void *ctx,unsigned slot){
    (void)ctx;
    if(slot>1U)return -1;
    memset(chip.slots[slot],0xff,SLOT_BYTES);
    ++chip.erases;return 0;
}
static ou_journal_io storage(void){
    ou_journal_io io={0};
    io.read=read_slot;io.program=program_slot;io.erase=erase_slot;
    io.slot_bytes=SLOT_BYTES;io.erase_block_bytes=SLOT_BYTES;
    io.geometry_verified=1U;io.dedicated_slots_verified=1U;
    io.writes_enabled=1U;return io;
}
static int authorization(void *ctx){
    (void)ctx;return physical.authorized;
}
static int apply(void *ctx,const ou_config_model *m){
    (void)ctx;++physical.applies;
    physical.network=*m;
    return physical.fail_apply?-1:0;
}
static int restore(void *ctx,const ou_config_model *m){
    (void)ctx;++physical.restores;
    if(physical.fail_restore)return -1;
    physical.network=*m;return 0;
}
static int probe(void *ctx){(void)ctx;return physical.probe;}
static ou_config_activation_adapter adapter(void){
    ou_config_activation_adapter a={0};
    a.apply=apply;a.restore=restore;a.probe=probe;
    a.authorize=authorization;
    a.physical_network_verified=1U;
    a.rollback_channel_verified=1U;
    a.authenticated_session_verified=1U;
    return a;
}
static void seed(ou_config_model *current,ou_journal_io *io){
    ou_journal_record meta;
    memset(&chip,0,sizeof(chip));
    memset(chip.slots,0xff,sizeof(chip.slots));
    chip.fail_after=-1;
    memset(&physical,0,sizeof(physical));
    physical.authorized=1;
    physical.probe=1;
    *io=storage();
    ou_config_model_init(current);
    assert(ou_config_model_set(current,"System.Network.HostName","h910p-lab")==0);
    assert(ou_config_model_set(current,"System.Network.DHCP","true")==0);
    assert(ou_config_model_set(current,"System.Contact","Operations")==0);
    assert(ou_config_model_validate(current)==0);
    assert(ou_config_model_save(current,io,authorization,0,&meta)==0);
    physical.network=*current;
}
static ou_config_model updated(const ou_config_model *base) {
    ou_config_model m=*base;
    assert(ou_config_model_set(&m,"System.Network.DHCP","false")==0);
    assert(ou_config_model_set(&m,"System.Network.IPAddress","10.8.1.25")==0);
    assert(ou_config_model_set(&m,"System.Network.IPMask","255.255.255.0")==0);
    assert(ou_config_model_set(&m,"System.Network.IPGateway","10.8.1.1")==0);
    assert(ou_config_model_validate(&m)==0);
    return m;
}
static void check_disk(const ou_journal_io *io,
                       const ou_config_model *expect){
    ou_config_model loaded;
    ou_journal_record meta;
    ou_config_model_init(&loaded);
    assert(ou_config_model_load(&loaded,io,&meta)==0);
    assert(memcmp(&loaded,expect,sizeof(loaded))==0);
}
static void safe_two_phase_and_reentry(void){
    ou_config_activation session={0};
    ou_config_model original,target;
    ou_journal_io io;
    ou_config_activation_adapter a;
    seed(&original,&io);target=updated(&original);a=adapter();
    assert(ou_config_activation_init(&session,&io,&a)==0);
    assert(ou_config_activation_begin(&session,&target,1000U,2000U)==-1);
    assert(ou_config_activation_begin(&session,&target,1000U,60000U)==0);
    assert(session.state==OU_CONFIG_ACT_PENDING);
    assert(physical.applies==1U);
    assert(memcmp(&physical.network,&target,sizeof(target))==0);
    check_disk(&io,&original);
    assert(ou_config_activation_begin(&session,&target,2000U,60000U)==-1);
    physical.probe=0;
    assert(ou_config_activation_step(&session,2000U)==0);
    assert(ou_config_activation_confirm(&session,2001U)==-1);
    check_disk(&io,&original);
    physical.probe=1;physical.authorized=0;
    assert(ou_config_activation_confirm(&session,3000U)==-1);
    check_disk(&io,&original);
    physical.authorized=1;
    assert(ou_config_activation_confirm(&session,4000U)==0);
    assert(session.state==OU_CONFIG_ACT_CONFIRMED);
    assert(session.confirmed_changes==1U);
    check_disk(&io,&target);
    assert(ou_config_activation_confirm(&session,5000U)==-1);
    assert(ou_config_activation_begin(&session,&target,6000U,60000U)==-1);
    assert(physical.restores==0U);
}
static void timeouts_and_manual_cancel(void){
    ou_config_activation session={0};
    ou_config_model original,target;
    ou_journal_io io;
    ou_config_activation_adapter a;
    seed(&original,&io);target=updated(&original);a=adapter();
    assert(ou_config_activation_init(&session,&io,&a)==0);
    assert(ou_config_activation_begin(&session,&target,UINT32_MAX-5000U,10000U)==0);
    physical.probe=0;
    assert(ou_config_activation_step(&session,UINT32_MAX-1000U)==0);
    assert(ou_config_activation_step(&session,6000U)==1);
    assert(session.state==OU_CONFIG_ACT_ROLLED_BACK);
    assert(session.last_error==OU_CONFIG_ACT_ERR_TIMEOUT);
    assert(memcmp(&physical.network,&original,sizeof(original))==0);
    check_disk(&io,&original);
    physical.probe=1;
    assert(ou_config_activation_begin(&session,&target,1000U,15000U)==0);
    assert(ou_config_activation_cancel(&session)==1);
    assert(session.rolled_back_changes==2U);
    assert(session.state==OU_CONFIG_ACT_ROLLED_BACK);
    assert(ou_config_activation_cancel(&session)==-1);
    check_disk(&io,&original);
}
static void authorization_and_hardware_qualification(void){
    ou_config_activation session={0};
    ou_config_model original,target;
    ou_journal_io io;
    ou_config_activation_adapter a;
    unsigned calls;
    seed(&original,&io);target=updated(&original);a=adapter();
    a.rollback_channel_verified=0U;
    assert(ou_config_activation_init(&session,&io,&a)==-1);
    a.rollback_channel_verified=1U;
    assert(ou_config_activation_init(&session,&io,&a)==0);
    calls=physical.applies;
    physical.authorized=0;
    assert(ou_config_activation_begin(&session,&target,0U,30000U)==-1);
    assert(physical.applies==calls);
    physical.authorized=1;physical.probe=0;
    assert(ou_config_activation_begin(&session,&target,0U,30000U)==-1);
    assert(physical.applies==calls);
    physical.probe=1;
    io.writes_enabled=0U;
    /* Refuse before applying any change if persistence is unavailable. */
    assert(ou_config_activation_begin(&session,&target,0U,30000U)==-1);
    assert(session.state==OU_CONFIG_ACT_IDLE);
    assert(physical.applies==calls);
    check_disk(&io,&original);
    io.writes_enabled=1U;
    physical.fail_apply=1;
    assert(ou_config_activation_begin(&session,&target,0U,30000U)==-1);
    assert(session.last_error==OU_CONFIG_ACT_ERR_APPLY);
    assert(session.state==OU_CONFIG_ACT_ROLLED_BACK);
    assert(memcmp(&physical.network,&original,sizeof(original))==0);
}
static void failed_storage_and_ambiguous_commit(void){
    ou_config_activation session={0};
    ou_config_model original,target;
    ou_journal_io io;
    ou_config_activation_adapter a;
    seed(&original,&io);target=updated(&original);a=adapter();
    assert(ou_config_activation_init(&session,&io,&a)==0);
    assert(ou_config_activation_begin(&session,&target,1000U,30000U)==0);
    chip.fail_after=40;
    assert(ou_config_activation_confirm(&session,2000U)==-1);
    chip.fail_after=-1;
    assert(session.state==OU_CONFIG_ACT_ROLLED_BACK);
    assert(session.last_error==OU_CONFIG_ACT_ERR_STORAGE);
    check_disk(&io,&original);
    assert(memcmp(&physical.network,&original,sizeof(original))==0);
    assert(ou_config_activation_begin(&session,&target,3000U,30000U)==0);
    chip.fail_read_after_marker=1;
    assert(ou_config_activation_confirm(&session,4000U)==-1);
    assert(session.state==OU_CONFIG_ACT_ROLLED_BACK);
    check_disk(&io,&original);
    assert(memcmp(&physical.network,&original,sizeof(original))==0);
}
static void failed_restoration_is_fatal(void){
    ou_config_activation session={0};
    ou_config_model original,target;
    ou_journal_io io;
    ou_config_activation_adapter a;
    seed(&original,&io);target=updated(&original);a=adapter();
    assert(ou_config_activation_init(&session,&io,&a)==0);
    assert(ou_config_activation_begin(&session,&target,1000U,30000U)==0);
    physical.fail_restore=1;
    physical.probe=-1;
    assert(ou_config_activation_step(&session,1200U)==-1);
    assert(session.state==OU_CONFIG_ACT_FAILED_ROLLBACK);
    assert(session.last_error==OU_CONFIG_ACT_ERR_RESTORE);
    assert(ou_config_activation_begin(&session,&target,2000U,30000U)==-1);
}
int main(void){
    safe_two_phase_and_reentry();
    timeouts_and_manual_cancel();
    authorization_and_hardware_qualification();
    failed_storage_and_ambiguous_commit();
    failed_restoration_is_fatal();
    puts("PASS: qualified config apply/confirm/rollback, storage atomicity, timeout wrap, stale defense and fatal restoration");
    return 0;
}
