# v3.7.6 Release Notes

## Logic Fixes
- **Warning Screen Suppression**: After acknowledging an alarm (pressing Button 2), the **Warning Screen** will be inhibited (blocked from appearing) for **60 seconds**, even if the alarm condition persists or re-triggers. This allows the user time to inspect the main dashboard without constant interruptions.

## Binaries
- `bar_gauge_demo.bin`: Application Firmware.
- `bootloader.bin`: Bootloader.
- `partition-table.bin`: Partition Table.

## Flashing Instructions
Use `esptool.py` or the Espressif Download Tool.

| Binary File | Offset Address |
| :--- | :--- |
| `bootloader.bin` | `0x2000` |
| `partition-table.bin` | `0x8000` |
| `bar_gauge_demo.bin` | `0x10000` |

### Command Line
```bash
esptool.py -p PORT -b 460800 --before default_reset --after hard_reset --chip esp32p4 write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x2000 bootloader.bin 0x8000 partition-table.bin 0x10000 bar_gauge_demo.bin
```
