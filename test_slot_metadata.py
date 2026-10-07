#!/usr/bin/env python3
"""Host regression checks for the actual X4 slot functions, with fake storage.

Fixtures are serialized independently as Android boot-control/AVB metadata.
No USB connection or device writes are used by these tests.
"""
import ctypes
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / 'bl33/v2019/cmd/amlogic/cmd_bootctl_vab.c'


def function(source, name):
    start = source.index(name)
    start = source.rfind('\n', 0, start) + 1
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def metadata(a=(7, 7, 1, 0), b=(7, 7, 1, 0), active='a'):
    data = bytearray(32)
    data[:2] = ('_' + active).encode()
    struct.pack_into('<IBB', data, 4, 0x42414342, 1, 2)
    for i, (priority, tries, successful, corrupted) in enumerate((a, b)):
        data[12 + 2 * i] = priority | (tries << 4) | (successful << 7)
        data[13 + 2 * i] = corrupted
    struct.pack_into('<I', data, 28, zlib.crc32(data[:28]))
    return bytes(data)


def legacy(a=(7, 7, 1), b=(15, 3, 0)):
    data = bytearray(32)
    data[:4] = b'\0AB0'
    data[4] = 1
    for i, values in enumerate((a, b)):
        data[8 + 4 * i:11 + 4 * i] = bytes(values)
    struct.pack_into('>I', data, 28, zlib.crc32(data[:28]))
    return bytes(data)


def main():
    source = SOURCE.read_text()
    types = source[source.index('typedef struct slot_metadata'):
                   source.index('bool boot_info_validate(')]
    start = source.index('static int x4_load_boot_control(')
    helpers = source[start:source.index('\n#endif', start)]
    prelude = r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#define BOOT_CTRL_MAGIC 0x42414342
#define BOOT_CTRL_VERSION 1
#define MISCBUF_SIZE 2080
#define AB_METADATA_MISC_PARTITION_OFFSET 2048
#define CMD_RET_FAILURE 1
#define le32_to_cpu(x) (x)
#define cpu_to_le32(x) (x)
#define be32_to_cpu(x) __builtin_bswap32(x)
static unsigned char disk[MISCBUF_SIZE];
static int writes, read_error, write_error;
static int has_boot_slot = 1, dynamic_partition = 1;
static int gpt_partition, vendor_boot_partition = 1;
static char active[16];
static const char *env_get(const char *key) {
    return strcmp(key, "active_slot") ? NULL : active;
}
static int env_set(const char *key, const char *value) {
    if (!strcmp(key, "active_slot")) snprintf(active, sizeof(active), "%s", value);
    return 0;
}
static int store_read(const char *part, int offset, int size, void *buffer) {
    if (read_error) return read_error;
    memcpy(buffer, disk + offset, size); return 0;
}
static int store_write(const char *part, int offset, int size, const void *buffer) {
    writes++;
    if (write_error) return write_error;
    memcpy(disk + offset, buffer, size); return 0;
}
'''
    wrappers = r'''
int test_select(const unsigned char *data, int read_failure) {
    memset(disk, 0x5a, sizeof(disk));
    memcpy(disk + 2048, data, 32);
    writes = 0; read_error = read_failure; write_error = 0;
    active[0] = 0;
    if (x4_get_valid_slot()) return -1;
    return active[1] - 'a';
}
int test_retry(int slot, int write_failure) {
    snprintf(active, sizeof(active), "_%c", 'a' + slot);
    write_error = write_failure;
    return x4_update_tries();
}
int test_activate(const char *name, int write_failure) {
    write_error = write_failure;
    return x4_set_active_slot(name);
}
int test_writes(void) { return writes; }
void test_disk(unsigned char *output) { memcpy(output, disk, sizeof(disk)); }
int test_abi(void) { return sizeof(bootloader_control) == 32 &&
    offsetof(bootloader_control, slot_info) == 12 &&
    offsetof(bootloader_control, crc32_le) == 28; }
'''
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        cfile = directory / 'test.c'
        cfile.write_text(prelude + types + '\n' + function(source, 'vab_crc32(') +
                         '\n' + helpers + wrappers)
        library = directory / 'test.so'
        subprocess.run(['cc', '-std=gnu99', '-shared', '-fPIC', '-O2',
                        str(cfile), '-o', str(library)], check=True)
        lib = ctypes.CDLL(str(library))
        assert lib.test_abi() == 1

        def select(data, expected, error=0):
            buffer = ctypes.create_string_buffer(data)
            assert lib.test_select(buffer, error) == expected
            assert lib.test_writes() == 0, 'Slot lookup wrote persistent data'

        def disk():
            buffer = ctypes.create_string_buffer(2080)
            lib.test_disk(buffer)
            return buffer.raw

        select(metadata(), 0)
        select(metadata(active='b'), 1)
        select(metadata(a=(15, 0, 1, 0), b=(14, 7, 0, 0)), 0)
        assert lib.test_retry(0, 0) == 0 and lib.test_writes() == 0
        select(metadata(a=(15, 0, 0, 0), b=(14, 4, 0, 0)), 1)
        select(metadata(a=(15, 7, 0, 1), b=(14, 4, 0, 0)), 1)
        select(metadata(a=(0, 7, 1, 0), b=(0, 7, 1, 0)), -1)
        select(metadata(), -1, error=1)
        bad_crc = bytearray(metadata(active='b'))
        bad_crc[-1] ^= 1
        select(bytes(bad_crc), 0)
        select(legacy(), 1)

        select(metadata(a=(7, 7, 1, 0), b=(15, 3, 0, 0)), 1)
        assert lib.test_retry(1, 0) == 0 and lib.test_writes() == 1
        result = disk()
        assert result[:2048] == b'\x5a' * 2048, 'BCB was overwritten'
        ctrl = result[2048:2080]
        assert (ctrl[14] >> 4) & 7 == 2
        assert struct.unpack_from('<I', ctrl, 28)[0] == zlib.crc32(ctrl[:28])
        select(metadata(a=(7, 7, 1, 0), b=(15, 3, 0, 0)), 1)
        assert lib.test_retry(1, 1) != 0, 'Write failure was swallowed'
        assert disk()[2048:2080] == metadata(a=(7, 7, 1, 0), b=(15, 3, 0, 0))

        select(metadata(a=(15, 0, 1, 0), b=(0, 0, 0, 1)), 0)
        assert lib.test_activate(b'b', 0) == 0 and lib.test_writes() == 1
        result = disk()
        assert result[:2048] == b'\x5a' * 2048
        ctrl = result[2048:2080]
        assert ctrl[:2] == b'_b' and ctrl[14:16] == b'\x7f\0'
        assert ctrl[12] & 15 == 14
        assert struct.unpack_from('<I', ctrl, 28)[0] == zlib.crc32(ctrl[:28])
        select(metadata(), 0)
        assert lib.test_activate(b'b', 1) != 0
        assert lib.test_activate(b'c', 0) != 0
    print('PASS: metadata ABI, CRC validation, A/B selection and fallback, legacy AVB,')
    print('successful-slot retries, attempt decrement, BCB preservation, explicit slot activation,')
    print('and storage read/write error propagation. All storage was mocked on the host.')


if __name__ == '__main__':
    main()
