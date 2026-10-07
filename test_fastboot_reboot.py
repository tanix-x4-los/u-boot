#!/usr/bin/env python3
"""Compile the actual command table, dispatcher and recovery completion."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parent / 'bl33/v2019'
s = (root/'drivers/fastboot/fb_command.c').read_text()
h = (root/'include/fastboot.h').read_text()
g = (root/'drivers/usb/gadget/f_fastboot.c').read_text()
def function(source, start):
    pos = source.index(start)
    opening = source.index('{', pos)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[pos:end]
enum = h[h.index('enum {'):h.index('};', h.index('enum {'))+2]
table_start = s.index('static const struct {')
table = s[table_start:s.index('\n};', table_start)+3]
code = r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_SC2_X4 1
#define CONFIG_IS_ENABLED(x) 0
static int nullable_strcmp(const char *a, const char *b) {
 if (!a || !b) return a != b;
 return strcmp(a,b);
}
#define strcmp nullable_strcmp
#define pr_err(...) ((void)0)
static void okay(char *p,char *r) { strcpy(r,"OKAY"); }
static void getvar(char *p,char *r) { okay(p,r); }
static void reboot_bootloader(char *p,char *r) { okay(p,r); }
static void reboot_fastboot(char *p,char *r) { okay(p,r); }
static void fastboot_fail(char *p,char *r) { strcpy(r,"FAIL"); }
struct usb_ep {}; struct usb_request {};
static int disconnects;
static char last_command[64];
static void f_dwc_otg_pullup(int v) { assert(v == 0); disconnects++; }
static int run_command(const char *c,int v) { strcpy(last_command,c); return 0; }
'''
code += enum + '\n' + table + '\n'
code += function(s,'static int strcmp_l1(') + '\n'
code += function(s,'int fastboot_handle_command(') + '\n'
code += function(g,'static void compl_do_reboot_recovery(') + '\n'
code += function(g,'static void compl_do_reset(') + '\n'
code += r'''
static void check(const char *input, int id, const char *expected) {
 char command[65], response[65]; strcpy(command,input);
 assert(fastboot_handle_command(command,response) == id);
 assert(!strcmp(response,expected));
}
int main(void) {
 check("reboot:recovery",FASTBOOT_COMMAND_REBOOT_RECOVERY,"OKAY");
 check("reboot-recovery",FASTBOOT_COMMAND_REBOOT_RECOVERY,"OKAY");
 check("reboot",FASTBOOT_COMMAND_REBOOT,"OKAY");
 check("reboot-bootloader",FASTBOOT_COMMAND_REBOOT_BOOTLOADER,"OKAY");
 check("reboot-fastboot",FASTBOOT_COMMAND_REBOOT_FASTBOOT,"OKAY");
 check("reboot:unknown",-1,"FAIL");
 compl_do_reboot_recovery(NULL,NULL);
 assert(disconnects == 1 && !strcmp(last_command,"reboot recovery"));
 compl_do_reset(NULL,NULL);
 assert(disconnects == 2 && !strcmp(last_command,"reboot normal"));
 puts("PASS: recovery target dispatch and Amlogic reboot completion; normal reset clears reboot reason; bootloader/fastboot dispatch unchanged; unknown target rejected");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/'reboot.c'; c.write_text(code)
    subprocess.run(['cc','-std=gnu11','-Wno-nonnull',str(c),'-o',str(Path(tmp)/'reboot')],check=True)
    subprocess.run([str(Path(tmp)/'reboot')],check=True)
