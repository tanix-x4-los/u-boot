# Tanix X4 U-Boot

This branch adds Tanix X4 support to the imported Vontar-capable SC2
baseline. Vontar board files, configuration and runtime behavior remain
intact; shared runtime changes are conditional on CONFIG_SC2_X4.

## Source organization

- bl33/v2019/board/amlogic/sc2_x4: X4 board initialization and stock DDR3
  timing data. gpt.c contains the GPT conversion, separate from board init.
- bl33/v2019/board/amlogic/configs/sc2_x4.h: X4 boot environment.
- bl33/v2019/board/amlogic/defconfigs/sc2_x4_defconfig: X4 build selection.
- Shared storage/slot/fastboot patches: X4-only behavior, with host tests.

## Automatic migration

Startup attempts migration before slot selection, using the existing
partition map and preserving filesystem extents. Valid primary GPT is
left unchanged. Conversion requires an eMMC hardware boot copy (1 or 2);
USB RAM sessions skip it and still reject MMC writes/erases.

Existing invalid GPT headers with EFI PART signatures are not overwritten.
Maps with overlapping, duplicate, unaligned or out-of-range entries are
rejected. Successful conversion requires GPT CRC readback validation.
Failure selects fastboot instead of normal autoboot.

As in the Vontar layout, there is no backup GPT at the end of userdata:
the preserved factory userdata extent uses the disk tail. GPT replaces
the user-area bootloader prefix; the hardware boot areas must supply
the installed bootloader. The explicit x4_gpt publish command is retained
for diagnosis and uses the same guards.

## Build and checks

Run bash build-production.sh from this directory. It uses the workspace's
existing cross toolchains, firmware stages and development signing keys;
it is not part of the Android build. Run test_slot_metadata.py,
test_mmc_guard.py, test_fastboot_reboot.py and test_gpt_publish.py with
python3 for the host regression checks.

On 2026-10-07 the clean X4 build and all four host checks passed, including
non-X4 fastboot behavior. Stock DDR-table byte checks passed for the
storage, USB and SD signed images. Build artifacts are archived outside
the checkout at ../uboot-x4-clean-artifacts/20261007.

This new build has not been hardware-tested or flashed. The old production
checkout and installed firmware are preserved. AV-port button support and
Android build integration remain deferred.
