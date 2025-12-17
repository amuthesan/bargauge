# v3.7.4 Release Notes

## Logic Fixes
- **Independent Relay Reset**: Corrected the latch reset button. Relays now reset when the **Monitor Reset Button** (Button 1) is pressed, matching the behavior of the Strobe light. (Previously mapped to Ack Button).

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
