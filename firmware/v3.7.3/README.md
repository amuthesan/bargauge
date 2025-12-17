# v3.7.3 Release Notes

## Logic Updates
- **Independent Relay Latching**: Relays triggered by gauge thresholds now behave like the main strobe/siren. 
    - **Latch ON**: When the value exceeds the threshold.
    - **Stay ON**: Even if the value drops back below the threshold.
    - **Reset**: Only turns OFF when the **Alarm Acknowledge Button** (Button 2) is pressed (and the condition is clear).

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
