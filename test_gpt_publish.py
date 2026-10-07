#!/usr/bin/env python3
"""Compile actual GPT map conversion and boot-source gate with mocked storage."""
from pathlib import Path
import subprocess
import tempfile
s = (Path(__file__).parent / 'bl33/v2019/board/amlogic/sc2_x4/sc2_x4.c').read_text()
def function(start):
    pos=s.index(start); opening=s.index('{',pos); end=opening+1; depth=1
    while depth:
        depth += (s[end]=='{')-(s[end]=='}'); end+=1
    return s[pos:end]
code = r"""
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
typedef uint32_t u32; typedef uint64_t lbaint_t; typedef int cmd_tbl_t;
#define CMD_RET_FAILURE 1
#define CMD_RET_SUCCESS 0
#define CMD_RET_USAGE 2
#define BOOT_ID_EMMC 1
#define SYSCTRL_SEC_STATUS_REG2 0
#define CONFIG_FASTBOOT_FLASH_MMC_DEV 1
#define GPT_PRIMARY_PARTITION_TABLE_LBA 1
#define GPT_ENTRY_NUMBERS 128
#define CONFIG_IS_ENABLED(x) 1
#define ALLOC_CACHE_ALIGN_BUFFER_PAD(t,n,a,b) t n[a]
typedef int gpt_header; typedef int gpt_entry;
struct mmc {int dummy;};
struct blk_desc {uint64_t lba; unsigned blksz,hwpart;};
struct partitions {char name[16]; uint64_t offset,size;};
typedef struct {uint64_t start,size; unsigned blksz; char name[32],uuid[40];} disk_partition_t;
static struct mmc mmc;
static struct blk_desc disk;
static struct partitions map[4];
static int count,valid,writes,nparts;
static u32 status;
static disk_partition_t published[4];
static u32 readl(int x) {return status;}
static struct mmc *find_mmc_device(int n) {return &mmc;}
static int mmc_init(struct mmc *m) {return 0;}
static struct blk_desc *mmc_get_blk_desc(struct mmc *m) {return &disk;}
static int is_gpt_valid(struct blk_desc *d,int l,gpt_header *h,gpt_entry **e) {return valid;}
static struct partitions *aml_ept_table(int *n) {*n=count;return map;}
static int x4_write_primary_gpt(struct blk_desc *d,disk_partition_t *p,int n) {
 writes++; nparts=n; memcpy(published,p,n*sizeof(*p)); return 0;
}
"""
code += function('static void x4_part_uuid(')+'\n'
code += function('static int x4_publish_gpt(')+'\n'
code += function('static int do_x4_gpt(')+'\n'
code += r"""
static char *args[]={"x4_gpt","publish"};
static void reset(void) {
 memset(map,0,sizeof(map)); count=3; valid=writes=0;
 disk=(struct blk_desc){61079552,512,0}; status=0x1011;
 strcpy(map[0].name,"bootloader"); map[0].size=4*1024*1024;
 strcpy(map[1].name,"misc"); map[1].offset=64*1024*1024; map[1].size=1024*1024;
 strcpy(map[2].name,"userdata"); map[2].offset=128*1024*1024; map[2].size=UINT64_MAX;
}
static void rejected(void) {assert(do_x4_gpt(0,0,2,args)==1);assert(writes==0);}
int main(void) {
 reset(); assert(do_x4_gpt(0,0,2,args)==0); assert(writes==1 && nparts==2);
 assert(published[0].start==131072 && published[0].size==2048);
 assert(published[1].start+published[1].size==disk.lba);
 reset(); status=0x1010; rejected(); /* current user-area boot */
 reset(); status=0x850; rejected(); /* USB RAM boot */
 reset(); status=0x1012; assert(do_x4_gpt(0,0,2,args)==0);
 reset(); disk.hwpart=1; rejected();
 reset(); valid=1; assert(do_x4_gpt(0,0,2,args)==0 && writes==0);
 reset(); map[1].size=0; assert(do_x4_gpt(0,0,2,args)==0 && nparts==1);
 reset(); map[1].offset++; rejected();
 reset(); map[1].size++; rejected();
 reset(); map[2].offset=map[1].offset; rejected();
 reset(); strcpy(map[2].name,"misc"); rejected();
 reset(); memset(map[1].name,'x',sizeof(map[1].name)); rejected();
 reset(); map[2].offset=(disk.lba+1)*512; rejected();
 reset(); map[2].size=UINT64_MAX-511; rejected();
 reset(); strcpy(map[0].name,"misc"); rejected();
 reset(); count=129; rejected();
 puts("PASS: GPT map validation, preserved extents, existing GPT and hardware boot gate");
}
"""
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c'; c.write_text(code); exe=Path(d)/'test'
    subprocess.run(['cc','-std=c99',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
