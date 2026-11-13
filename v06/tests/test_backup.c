#include "ou_backup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int permission(void *context){return *(int *)context;}
static void sample(ou_backup_snapshot *backup){
    ou_config_model_init(&backup->configuration);
    assert(ou_config_model_set(&backup->configuration,
           "System.Network.HostName","h910p-lab")==0);
    assert(ou_config_model_set(&backup->configuration,
           "System.Network.DHCP","true")==0);
    assert(ou_config_model_set(&backup->configuration,
           "System.Contact","Network operations")==0);
    ou_shutdown_policy_init(&backup->shutdown);
    backup->shutdown.runtime_limit_seconds=600U;
    backup->shutdown.capacity_limit_percent=20U;
    backup->shutdown.segments[0].capacity_limit_percent=15U;
    ou_weekly_schedule_init(&backup->weekly);
    backup->weekly.off[0]=8*3600;
    backup->weekly.on[0]=18*3600;
    backup->weekly.off[6]=22*3600+30*60;
    assert(ou_config_model_validate(&backup->configuration)==0);
    assert(ou_shutdown_policy_validate(&backup->shutdown)==0);
    assert(ou_weekly_schedule_validate(&backup->weekly)==0);
}
static void rehash(uint8_t *bytes,size_t length){
    ou_sha256_ctx sha;uint8_t digest[32];
    ou_sha256_init(&sha);
    assert(ou_sha256_update(&sha,bytes,16U)==0);
    assert(ou_sha256_update(&sha,bytes+OU_BACKUP_HEADER_BYTES,
                            length-OU_BACKUP_HEADER_BYTES)==0);
    assert(ou_sha256_final(&sha,digest)==0);
    memcpy(bytes+16U,digest,32U);
}
static void corruption_and_atomic_decode(void){
    ou_backup_snapshot original,loaded,before;
    uint8_t bytes[OU_BACKUP_MAX_BYTES],backup[OU_BACKUP_MAX_BYTES];
    size_t length=0U;
    int authorized=1;
    sample(&original);
    memset(bytes,0xAB,sizeof(bytes));
    assert(ou_backup_export(&original,permission,&authorized,
                            bytes,sizeof(bytes),&length)==0);
    assert(length>=OU_BACKUP_HEADER_BYTES+OU_SHUTDOWN_RECORD_BYTES+
            OU_WEEK_RECORD_BYTES);
    assert(length<OU_BACKUP_MAX_BYTES);
    assert(bytes[0]=='O'&&bytes[1]=='6'&&bytes[2]=='B'&&bytes[3]=='K');
    assert(bytes[4]==1U&&bytes[5]==3U);
    assert((unsigned)bytes[8]*256U+(unsigned)bytes[9]==length);
    memset(&loaded,0x75,sizeof(loaded));
    assert(ou_backup_preview(&loaded,bytes,length)==0);
    assert(memcmp(&loaded.configuration,&original.configuration,
                  sizeof(original.configuration))==0);
    assert(memcmp(&loaded.shutdown,&original.shutdown,
                  sizeof(original.shutdown))==0);
    assert(memcmp(&loaded.weekly,&original.weekly,
                  sizeof(original.weekly))==0);
    before=loaded;
    memcpy(backup,bytes,length);
    bytes[length-1U]^=0x80U;
    assert(ou_backup_preview(&loaded,bytes,length)==-1);
    assert(memcmp(&loaded,&before,sizeof(loaded))==0);
    memcpy(bytes,backup,length);
    bytes[4]=2U;rehash(bytes,length);
    assert(ou_backup_preview(&loaded,bytes,length)==-1);
    bytes[4]=1U;bytes[5]=2U;rehash(bytes,length);
    assert(ou_backup_preview(&loaded,bytes,length)==-1);
    memcpy(bytes,backup,length);
    bytes[6]=1U;rehash(bytes,length);
    assert(ou_backup_preview(&loaded,bytes,length)==-1);
    memcpy(bytes,backup,length);
    bytes[10]=0U;bytes[11]=1U;rehash(bytes,length);
    assert(ou_backup_preview(&loaded,bytes,length)==-1);
    memcpy(bytes,backup,length);
    bytes[12]=0U;bytes[13]=63U;rehash(bytes,length);
    assert(ou_backup_preview(&loaded,bytes,length)==-1);
    memcpy(bytes,backup,length);
    bytes[14]=0U;bytes[15]=63U;rehash(bytes,length);
    assert(ou_backup_preview(&loaded,bytes,length)==-1);
    memcpy(bytes,backup,length);
    assert(ou_backup_preview(&loaded,bytes,length-1U)==-1);
    assert(ou_backup_preview(&loaded,bytes,OU_BACKUP_MAX_BYTES+1U)==-1);
    /* Attacker can rehash a corrupted inner section, but the format's
       independent version/semantic decoder must still reject it. */
    memcpy(bytes,backup,length);
    {
        size_t config_length=((size_t)bytes[10]<<8U)|bytes[11];
        size_t shutdown_offset=OU_BACKUP_HEADER_BYTES+config_length;
        bytes[shutdown_offset+4U]=77U;
        rehash(bytes,length);
        assert(ou_backup_preview(&loaded,bytes,length)==-1);
    }
    assert(memcmp(&loaded,&before,sizeof(loaded))==0);
}
static void authorization_and_all_or_nothing(void){
    ou_backup_snapshot original,invalid;
    uint8_t bytes[OU_BACKUP_MAX_BYTES];
    size_t written=1234U;
    int authorized=0;
    sample(&original);
    memset(bytes,0xAA,sizeof(bytes));
    assert(ou_backup_export(&original,permission,&authorized,bytes,
                            sizeof(bytes),&written)==-1);
    assert(written==0U&&bytes[0]==0xAAU);
    authorized=1;
    assert(ou_backup_export(&original,permission,&authorized,bytes,
                            32U,&written)==-1);
    assert(written==0U&&bytes[0]==0xAAU);
    invalid=original;
    invalid.weekly.off[0]=17U;
    assert(ou_backup_export(&invalid,permission,&authorized,bytes,
                            sizeof(bytes),&written)==-1);
    invalid=original;
    invalid.shutdown.capacity_limit_percent=150U;
    assert(ou_backup_export(&invalid,permission,&authorized,bytes,
                            sizeof(bytes),&written)==-1);
    invalid=original;
    assert(ou_config_model_set(&invalid.configuration,
                               "System.Network.DHCP","false")==0);
    assert(ou_backup_export(&invalid,permission,&authorized,bytes,
                            sizeof(bytes),&written)==-1);
    assert(ou_backup_export(&original,NULL,&authorized,bytes,
                            sizeof(bytes),&written)==-1);
    assert(ou_backup_export(&original,permission,&authorized,NULL,
                            sizeof(bytes),&written)==-1);
    assert(ou_backup_preview(NULL,bytes,0U)==-1);
    assert(ou_backup_preview(&invalid,NULL,0U)==-1);
}
static void deterministic_reexport(void){
    ou_backup_snapshot model,loaded;
    uint8_t a[OU_BACKUP_MAX_BYTES],b[OU_BACKUP_MAX_BYTES];
    size_t la=0U,lb=0U;
    int yes=1;
    sample(&model);
    assert(ou_backup_export(&model,permission,&yes,a,sizeof(a),&la)==0);
    assert(ou_backup_preview(&loaded,a,la)==0);
    assert(ou_backup_export(&loaded,permission,&yes,b,sizeof(b),&lb)==0);
    assert(la==lb&&memcmp(a,b,la)==0);
}
int main(void){
    corruption_and_atomic_decode();
    authorization_and_all_or_nothing();
    deterministic_reexport();
    puts("PASS: portable network/shutdown/weekly backup, authorization, SHA integrity, non-destructive atomic preview and deterministic re-export");
    return 0;
}
