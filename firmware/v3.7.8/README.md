# v3.7.8 Release Notes

## Logic Fixes
- **Warning Screen Re-Appearance**: Fixed an issue where the Warning Screen would not reappear after the 60-second suppression period if the alarm was still active.
    - Now, if the suppression timer expires and the alarm is still present, the screen will automatically pop up again.
    - **Siren Behavior**: The Siren remains **Silent** (Acknowledged) during this re-appearance, ensuring purely visual feedback for persistent alarms.

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
