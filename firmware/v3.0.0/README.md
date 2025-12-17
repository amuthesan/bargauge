# v3.0.0 Release Notes

## Critical Fixes
- **Config Data Integrity**: Fixed a regression where loading configuration would truncate strings (e.g., Gauge Name) to a single character.
- **Auto-Load Prevention**: The Gauge Config screen now initializes with "---" to strictly indicate that no data has been loaded yet, preventing accidental overwrites of Gauge 1 data on boot.
- **Feedback Loop Protection**: Implemented `is_programmatic_update` flag to prevent UI value change callbacks from firing during programmatic UI updates, which was corrupting configuration data.

## Features
- **Manual Load Config**: Explicit "LOAD CONFIG" button required to fetch data from memory vs UI.
- **Status Feedback**: Button changes to "LOADED" upon success.

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Flash at address `0x10000`.
