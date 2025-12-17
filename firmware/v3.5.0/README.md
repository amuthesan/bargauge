# Firmware v3.5.0 Release Notes

## 🚀 Significant Performance Improvements

This release focuses on eliminating touch latency and optimizing UI performance for the ESP32-P4.

### ⚡ Touch Latency Optimization
- **Core Affinity**: Moved LVGL (Display Task) to **Core 1** to isolate it from system/WiFi interrupts on Core 0.
- **Task Priority**: Increased LVGL Task Priority to **6** (from 4) for immediate input handling.
- **I2C Speed**: Confirmed and locked Touch Controller I2C bus to **400kHz**.

### 🎨 UI Efficiency
- **Smart Redraws**: Optimized the main gauge loop to only redraw "SAFE" / "WARNING" status labels when the state *actually changes*.
  - Previously, these were redrawn every 100ms for all 16 gauges, wasting CPU cycles.
  - This change significantly reduces overhead on the UI thread.

### 📦 Binaries
- `bar_gauge_demo.bin`: Main Firmware
- `bootloader.bin`: Bootloader
- `partition-table.bin`: Partition Table

---
**Flashing Instructions:**
```bash
idf.py -p PORT flash monitor
```
Or using the provided binaries:
```bash
esptool.py -p PORT -b 460800 --before default_reset --after hard_reset --chip esp32p4  write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0000 bootloader.bin 0x8000 partition-table.bin 0x10000 bar_gauge_demo.bin
```
