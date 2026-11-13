#include "ou_hid_report.h"

typedef struct {
    uint32_t page;
    int32_t logical_min,logical_max;
    uint16_t report_size,report_count;
    uint8_t report_id;
    uint8_t have_min,have_max;
} globals;
typedef struct {
    uint32_t usage[16];
    uint32_t minimum,maximum;
    uint8_t used,have_min,have_max;
} locals;
static uint32_t readle(const uint8_t *p,size_t n){
    size_t i;uint32_t v=0U;
    for(i=0U;i<n;++i)v|=(uint32_t)p[i]<<(8U*i);
    return v;
}
static int32_t signed_item(uint32_t raw,size_t n){
    if(n==1U)return (int8_t)raw;
    if(n==2U)return (int16_t)raw;
    return (int32_t)raw;
}
static uint32_t usage_at(const locals *l,uint16_t index){
    if(l->used)return l->usage[index<l->used?index:l->used-1U];
    if(l->have_min&&l->have_max) {
        uint32_t u=l->minimum+(uint32_t)index;
        return u<=l->maximum?u:l->maximum;
    }
    return 0U;
}
static int feature(ou_hid_feature_map *out,const globals *g,
                   const locals *l,uint32_t flags){
    uint32_t bits=(uint32_t)g->report_size*g->report_count;
    uint32_t start=out->feature_bits[g->report_id];
    uint16_t i;
    if(!g->report_size||g->report_size>32U||!g->report_count||
       g->report_count>256U||bits>OU_HID_REPORT_BITS_CAP||
       start>OU_HID_REPORT_BITS_CAP-bits)return -1;
    if((flags&3U)==2U) {
        for(i=0U;i<g->report_count;++i) {
            uint32_t usage=usage_at(l,i);
            ou_hid_feature_field *field;
            if(usage==0U || (g->page!=0x84U && g->page!=0x85U))continue;
            if(usage>0xffffU || !g->have_min || !g->have_max ||
               g->logical_min>g->logical_max ||
               out->field_count>=OU_HID_FIELDS_CAP)return -1;
            field=&out->fields[out->field_count++];
            field->usage_page=(uint16_t)g->page;
            field->usage=(uint16_t)usage;
            field->bit_offset=(uint16_t)(start+(uint32_t)i*g->report_size);
            field->bit_size=(uint8_t)g->report_size;
            field->report_id=g->report_id;
            field->signed_value=(uint8_t)(g->logical_min<0);
            field->logical_min=g->logical_min;
            field->logical_max=g->logical_max;
        }
    }
    out->feature_bits[g->report_id]=(uint16_t)(start+bits);
    return 0;
}
int ou_hid_feature_parse(const uint8_t *data,size_t length,
                         ou_hid_feature_map *out){
    globals g={0},stack[4];locals l={0};
    size_t pos=0U;uint8_t sp=0U,depth=0U,saw_id0=0U;
    if(!out)return -1;
    *out=(ou_hid_feature_map){0};
    if(!data||!length||length>OU_HID_DESCRIPTOR_CAP)return -1;
    while(pos<length) {
        uint8_t prefix=data[pos++],kind,tag;
        size_t n;uint32_t raw;
        if(prefix==0xfeU)return -1; /* Unknown long items, fail closed. */
        n=(size_t)(prefix&3U);if(n==3U)n=4U;
        if(n>length-pos)return -1;
        raw=readle(data+pos,n);pos+=n;
        kind=(uint8_t)((prefix>>2U)&3U);
        tag=(uint8_t)(prefix>>4U);
        if(kind==1U){
            switch(tag) {
            case 0: g.page=raw;break;
            case 1: g.logical_min=signed_item(raw,n);g.have_min=1U;break;
            case 2:
                if(!g.have_min)return -1;
                if(g.logical_min<0)g.logical_max=signed_item(raw,n);
                else if(raw>INT32_MAX)return -1;
                else g.logical_max=(int32_t)raw;
                g.have_max=1U;break;
            case 7: g.report_size=(uint16_t)raw;if(raw>32U)return -1;break;
            case 8:
                if(raw==0U||raw>255U||out->feature_bits[0]!=0U||saw_id0)
                    return -1;
                g.report_id=(uint8_t)raw;out->has_report_ids=1U;break;
            case 9: g.report_count=(uint16_t)raw;if(raw>256U)return -1;break;
            case 10: if(sp==4U)return -1;stack[sp++]=g;break;
            case 11: if(!sp)return -1;g=stack[--sp];break;
            default: break; /* Unit data is NOT silently applied to values. */
            }
        } else if(kind==2U){
            if(tag==0U || tag==1U || tag==2U) {
                if(n==0U||n==4U||raw>0xffffU)return -1;
                if(tag==0U) {
                    if(l.used>=16U)return -1;
                    l.usage[l.used++]=raw;
                } else if(tag==1U) {l.minimum=raw;l.have_min=1U;}
                else {l.maximum=raw;l.have_max=1U;}
            }
        } else if(kind==0U){
            if(tag==11U) {
                if(!g.report_id)saw_id0=1U;
                if(feature(out,&g,&l,raw)!=0)return -1;
            } else if(tag==10U) {
                if(++depth>8U)return -1;
            } else if(tag==12U) {
                if(depth==0U)return -1;
                --depth;
            }
            l=(locals){0}; /* Local items scope to next main item. */
        } else return -1;
    }
    if(depth!=0U||sp!=0U||!out->field_count)return -1;
    return 0;
}
int ou_hid_feature_decode(const ou_hid_feature_map *map,
                          const uint8_t *report,size_t length,
                          ou_hid_feature_value *values,size_t capacity,
                          size_t *written){
    const uint8_t *data;
    size_t bytes,at,count=0U,i;
    ou_hid_feature_value staged[OU_HID_VALUES_CAP];
    uint8_t id=0U;
    if(!written)return -1;
    *written=0U;
    if(!map||!report||!values||!length||map->field_count>OU_HID_FIELDS_CAP)
        return -1;
    if(map->has_report_ids) {id=report[0];data=report+1U;bytes=length-1U;}
    else {data=report;bytes=length;}
    /* The descriptor defines the entire feature report length, including
     * constant fields. Refuse truncation and trailing spoofed bytes alike. */
    if(bytes>OU_HID_REPORT_BITS_CAP/8U ||
       map->feature_bits[id]==0U ||
       bytes!=((size_t)map->feature_bits[id]+7U)/8U)return -1;
    for(i=0U;i<map->field_count;++i){
        const ou_hid_feature_field *f=&map->fields[i];
        uint32_t raw=0U;
        int32_t decoded;
        size_t b;
        if(f->report_id!=id)continue;
        if(!f->bit_size||f->bit_size>32U||
           (uint32_t)f->bit_offset+f->bit_size>bytes*8U||
           count>=capacity || count>=OU_HID_VALUES_CAP)return -1;
        for(b=0U;b<f->bit_size;++b){
            at=(size_t)f->bit_offset+b;
            if((data[at/8U]>>(at%8U))&1U)raw|=UINT32_C(1)<<b;
        }
        if(f->signed_value && f->bit_size<32U &&
           (raw&(UINT32_C(1)<<(f->bit_size-1U))))
            raw|=UINT32_MAX<<f->bit_size;
        if(!f->signed_value&&raw>INT32_MAX)return -1;
        decoded=(int32_t)raw;
        if(decoded<f->logical_min||decoded>f->logical_max)return -1;
        staged[count].usage_page=f->usage_page;
        staged[count].usage=f->usage;
        staged[count].report_id=id;
        staged[count].value=decoded;
        staged[count].telemetry_verified=0U;
        ++count;
    }
    if(count==0U)return -1;
    /* Nothing reaches the caller until every advertised field validates. */
    for(i=0U;i<count;++i)values[i]=staged[i];
    *written=count;
    return 0;
}
