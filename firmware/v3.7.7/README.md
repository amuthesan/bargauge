# v3.7.7 Release Notes

## Stable Release
This version consolidates the latest logic improvements for site deployment.

### Features
1.  **Warning Screen Suppression**: 
    - Acknowledging an alarm (Button 2) inhibits the warning screen for **60 seconds**.
    - If the alarm persists after 60s, the screen reappears.
2.  **Latching Logic**:
    - **Siren**: Latches ON until Acknowledged (Button 2).
    - **Strobe & Independent Relays**: Latch ON until Reset (Button 1) AND Safe Condition.
3.  **UI Updates**:
    - Larger Service & Settings Icons (60x60).

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
