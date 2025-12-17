# v3.7.5 Release Notes

## Logic Fixes
- **Warning Screen Latching**: The Warning/Alert screen now remains visible even if the alarm condition clears automatically. It will only disappear when the **Alarm Acknowledge Button** (Button 2) is pressed.

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
