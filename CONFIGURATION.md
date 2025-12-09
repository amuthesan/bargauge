# Bar Gauge Project Configuration

## Overview
**Version**: 0.1.0 (Initial Release)
**Target**: ESP32-P4 Function EV Board
**Display**: 800x1280 MIPI-DSI (St7701/JD9365)
**Orientation**: Landscape (1280x800)

## Hardware Configuration

### Display & Graphics
- **Controller**: JD9365 (MIPI-DSI 2-lane)
- **Resolution**: 800x1280 (Physical), 1280x800 (Logical Landscape)
- **Rotation**: 90 degrees via PPA (Hardware Acceleration)
- **Buffering**: Triple Buffer (MODE=2) in PSRAM
- **Backlight**: GPIO 23 (LEDC PWM)

### Memory (PSRAM)
- **Type**: 32MB Octal-SPI @ 200MHz
- **Usage**:
  - LVGL Frame Buffers: Allocated in PSRAM
  - Heap: Available for application
  - Cache: 256KB L2 Cache, 128B Line Size

### Connectivity
- **WiFi**: ESP-Hosted (SDIO) using ESP32-C6 Slave
  - **Interface**: SDIO (Slot 1)
  - **Correction**: GPIO 54 (Reset)
  - **Status**: Integrated and Functional
- **RTC**: RX8025T (I2C @ 0x32)
  - **Driver**: Custom `rx8025t.c/h`
  - **Sync**: NTP -> RTC on connection
- **Touch**: GSL3680 (I2C @ 0x40)

## Software Configuration

### LVGL (v9.2)
- **Refresh Rate**: 15ms (~66 FPS target)
- **Task Priority**: High (4)
- **Core Affinity**: Core 0
- **Direct Mode**: Enabled (Avoid Tear Mode 2)
- **Fonts**: Montserrat (12, 14, 16, 18, 20, 22, 24, 26, 32)

### Partition Table
- **Scheme**: Custom (`partitions.csv`)
- **Factory App**: 8MB
- **Flash Size**: 16MB

## Build Instructions
```bash
idf.py build
idf.py -p /dev/cu.usbserial-xxxxxx flash monitor
```

## Known Constraints
- **Flash Size**: Minimum 16MB required for 8MB partition table.
- **WiFi**: Requires ESP32-C6 co-processor flashed with ESP-Hosted firmware.
