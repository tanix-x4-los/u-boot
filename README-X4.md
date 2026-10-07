# Tanix X4 U-Boot

X4 is a separate SC2 board target. Existing board configurations remain
available. Recovery reboot support is adapted from LineageOS commit
5fce2c5b8dd1e4f81f3f6ffe244a359dddbde010, preserving Bruno Martins's authorship.

## Boot behavior

X4 selects boot, vendor_boot and recovery partitions using the same board
logic as sc2_vontarx4. Storage initialization and A/B commands use the
existing vendor implementation. There are no X4 overrides for calibration
writes, reported capacity, factory provisioning or HDMI environment saves.
USB-loaded U-Boot retains the vendor write behavior and normal boot flow.

Automatic GPT conversion is in board/amlogic/sc2_x4/gpt.c under bl33/v2019.
It leaves valid GPT unchanged, requires an eMMC hardware boot for conversion,
checks partition extents and validates GPT readback. USB boots skip this
conversion. There is no backup GPT at the userdata tail; this retains the
factory userdata extent. GPT overwrites the user-area bootloader prefix,
so the installed bootloader must boot from an eMMC hardware boot area.

## Build

Run bash tools/build-x4.sh. X4_WORKSPACE_ROOT can select the workspace
containing toolchains. CROSS_COMPILE overrides the AArch64 compiler prefix;
RISCV_TOOLCHAIN_BIN overrides the RISC-V toolchain directory.
The default toolchain paths match this workspace. Firmware packaging uses
the existing vendor stages and development signing keys. The build log is
build-x4.log.

This revision has not been hardware-tested or flashed. AV-port button
support and Android build integration remain deferred. Earlier images and
the old production tree remain available outside this checkout.
