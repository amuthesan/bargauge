# v3.7.0 Release Notes

## Critical Fixes
- **Trending Live View**: Fixed a critical bug where the trending chart would freeze or fail to update in Live Mode.
   - Resolved incorrect pointer referencing for the `trending_mode_sw`.
   - Fixed condition logic that was preventing updates.
   - Ensured chart data is properly cleared when switching modes to prevent "ghosting" of 24h data.

## features
- **Optimized Update Rate**: Live trending chart now updates effectively at 1Hz (1 data point per second), providing a smooth yet performant visualization.
- **Stability**: Removed unused variables and cleaned up debug artifacts for a cleaner build.

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
