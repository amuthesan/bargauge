# v3.7.11 Release Notes

## Architecture Fixes
- **Robust Input Logic**: Decoupled the Acknowledge Button processing from the Suppression State.
    - **Previously**: The system stopped tracking the button state while the screen was suppressed/acknowledged. If the user released the button during this time, the system missed it, leading to "stuck" logic later.
    - **Now**: The button input is processed continuously every cycle, regardless of the alarm state. This guarantees that "Rising Edge" detection is 100% accurate, solving the issue where the screen failed to reappear.

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
