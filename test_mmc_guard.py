#!/usr/bin/env python3
"""Compile the actual MMC dispatcher against a fake controller; no device IO."""
import ctypes
from pathlib import Path
import re
import subprocess
import tempfile
from test_slot_metadata import function

root = Path(__file__).resolve().parent / 'bl33/v2019'
headers = (root / 'include/mmc.h').read_text()
defines = '\n'.join(re.findall(r'^#define\s+(?:MMC_DATA_WRITE|MMC_DATA_READ|MMC_CMD_ERASE)\s+[^\n]+',
                               headers, re.MULTILINE))
dispatcher = function((root / 'drivers/mmc/mmc-uclass.c').read_text(), 'dm_mmc_send_cmd(')
fake_controller = r'''
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#define CONFIG_SC2_X4 1
struct udevice { int unused; };
struct mmc { int unused; };
struct mmc_cmd { unsigned cmdidx; };
struct mmc_data { unsigned flags; };
struct dm_mmc_ops {
    int (*send_cmd)(struct udevice *, struct mmc_cmd *, struct mmc_data *);
};
static int usb_boot, calls;
static struct mmc card;
static bool x4_usb_ram_boot(void) { return usb_boot; }
static void mmmc_trace_before_send(struct mmc *m, struct mmc_cmd *c) {}
static void mmmc_trace_after_send(struct mmc *m, struct mmc_cmd *c, int r) {}
static int send(struct udevice *d, struct mmc_cmd *c, struct mmc_data *x) {
    calls++; return 0;
}
static struct dm_mmc_ops ops = {send};
static struct mmc *mmc_get_mmc_dev(struct udevice *d) { return &card; }
static struct dm_mmc_ops *mmc_get_ops(struct udevice *d) { return &ops; }
'''
wrappers = r'''
int test_dispatch(int usb, int operation) {
    struct udevice dev;
    struct mmc_cmd cmd = {17};
    struct mmc_data data = {MMC_DATA_READ};
    usb_boot = usb; calls = 0;
    if (operation == 1) { cmd.cmdidx = 24; data.flags = MMC_DATA_WRITE; }
    if (operation == 2) cmd.cmdidx = MMC_CMD_ERASE;
    return dm_mmc_send_cmd(&dev, &cmd, operation == 2 ? NULL : &data);
}
int test_calls(void) { return calls; }
'''
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    source = directory / 'test.c'
    source.write_text(defines + '\n' + fake_controller + dispatcher + wrappers)
    library = directory / 'test.so'
    subprocess.run(['cc', '-shared', '-fPIC', str(source), '-o', str(library)], check=True)
    lib = ctypes.CDLL(str(library))
    for usb in (0, 1):
        for operation in (0, 1, 2):
            blocked = usb and operation != 0
            assert lib.test_dispatch(usb, operation) == (-30 if blocked else 0)
            assert lib.test_calls() == (0 if blocked else 1)
print('PASS: actual dispatcher permits storage-boot writes/erase and USB reads;')
print('USB RAM writes/erase return EROFS without reaching the fake controller.')
