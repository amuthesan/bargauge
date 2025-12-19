# v3.7.6 Release Notes

## Logic Fixes
- **Warning Screen Suppression**: After acknowledging an alarm (pressing Button 2), the **Warning Screen** will be inhibited (blocked from appearing) for **60 seconds**, even if the alarm condition persists or re-triggers. This allows the user time to inspect the main dashboard without constant interruptions.

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
