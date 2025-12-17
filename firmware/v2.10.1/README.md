# BarGauge Firmware v2.10.1

This folder contains the compiled firmware binaries for the BarGauge ESP32-P4 device.

## Flash Offsets

| Binary File           | Address  |
|-----------------------|----------|
| `bootloader.bin`      | `0x2000` |
| `partition-table.bin` | `0x8000` |
| `bar_gauge_demo.bin`  | `0x10000`|

## Flash Command (`esptool.py`)

Run the following command in this directory to flash the device:

```bash
esptool.py -p /dev/cu.usbserial-212430 -b 460800 --before default_reset --after hard_reset --chip esp32p4 write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x2000 bootloader.bin 0x8000 partition-table.bin 0x10000 bar_gauge_demo.bin
```

*Note: Replace `/dev/cu.usbserial-212430` with your actual serial port if different.*
