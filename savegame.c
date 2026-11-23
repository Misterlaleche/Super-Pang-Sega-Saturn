#include <jo/jo.h>
#include <string.h>
#include "include/savegame.h"
#include "include/boot_diag.h"
#include "frontend/frontend.h"
#include "frontend/save_codec.h"
#include "frontend/settings.h"

volatile savegame_diagnostics savegame_status __attribute__((used,externally_visible));
static uint8_t last_records[200];
static char * const names[2]={"SPANG_REC_A","SPANG_REC_B"};
static int read_slot(unsigned slot,uint8_t records[200],uint32_t *generation)
{
 unsigned length=0,blocks=0;uint8_t *file;int valid=0;
 boot_diag_mark(14+slot);
 if(!jo_backup_get_file_size(JoInternalMemoryBackup,names[slot],&length,&blocks)||(length!=SPANG_SAVE_BYTES&&length!=208))return 0;
 file=jo_backup_load_file_contents(JoInternalMemoryBackup,names[slot],&length);
 if(file){valid=spang_save_decode(file,length,records,generation);jo_free(file);}
 if(valid)for(unsigned row=0;row<12;row++)if(records[row*16+5])return 0;
 return valid&&spang_settings_valid(records+192);
}
void savegame_load(void)
{
 uint8_t candidate[200];uint32_t generation=0;
 memset((void *)&savegame_status,0,sizeof savegame_status);
 if(jo_backup_mount_existing(JoInternalMemoryBackup)){
  for(unsigned slot=0;slot<2;slot++)if(read_slot(slot,candidate,&generation)&&
    (!savegame_status.loaded||spang_save_newer(generation,savegame_status.generation))&&frontend_records_import(candidate)&&spang_settings_import(candidate+192)){
   savegame_status.loaded=1;savegame_status.generation=generation;savegame_status.slot=slot;
  }
  boot_diag_mark(16);
  jo_backup_unmount(JoInternalMemoryBackup);
 }else savegame_status.error=-1;
 (void)frontend_records_export(last_records);spang_settings_export(last_records+192);
 savegame_status.attempted_revision=frontend_records_revision()+spang_settings_revision();
}
void savegame_flush(void)
{
 uint8_t records[200],file[216],verified[200];uint32_t generation;
 unsigned revision=frontend_records_revision()+spang_settings_revision(),slot=savegame_status.loaded?(savegame_status.slot^1u):0;
 if(revision==savegame_status.attempted_revision||!frontend_records_export(records))return;
 savegame_status.attempted_revision=revision;spang_settings_export(records+192);
 if(!memcmp(records,last_records,200))return;
 if(!jo_backup_mount_existing(JoInternalMemoryBackup)){savegame_status.error=-1;return;}
 generation=savegame_status.generation+1;spang_save_encode(file,records,generation);
 if(!jo_backup_save_file_contents(JoInternalMemoryBackup,names[slot],"PANG SCORE",file,sizeof file))savegame_status.error=-2;
 else{
  uint32_t checked=0;
  if(!read_slot(slot,verified,&checked)||checked!=generation||memcmp(verified,records,200))savegame_status.error=-3;
  else{
   savegame_status.loaded=1;savegame_status.generation=generation;savegame_status.slot=slot;
   savegame_status.error=0;++savegame_status.writes;memcpy(last_records,records,200);
  }
 }
 jo_backup_unmount(JoInternalMemoryBackup);
}
