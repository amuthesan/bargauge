# v3.7.9 Release Notes

## Logic Fixes
- **Robust Warning Suppression**: Improved the suppression logic to prevent "timer extension" from a held button or noisy alarm signal. 
    - The 60-second timer now starts exactly once upon acknowledgement and cannot be pushed further into the future by holding the button.
    - This ensures the Warning Screen reappears reliably after 60 seconds if the alarm persists.

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
