# v3.7.10 Release Notes

## Critical Fixes
- **Stuck Button Logic Fix**: Changed the Acknowledge Button (Button 2) logic from **Level-Triggered** to **Edge-Triggered**.
    - **Issue**: Previously, if the button was held down, stuck, or electrically noisy (High), the system would continuously "re-acknowledge" every 100ms. This caused the 60-second suppression timer to restart infinitely, effectively disabling the Warning Screen re-appearance.
    - **Fix**: The system now only registers an acknowledgement on the specific moment the button is pressed (Rising Edge). Holding the button has no effect on the timer.
    - **Result**: The Warning Screen is guaranteed to reappear after 60 seconds if the alarm persists, regardless of button state.

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
