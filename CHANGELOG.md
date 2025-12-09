# Changelog

All notable changes to this project will be documented in this file.

## [v0.3.0] - 2025-12-09
### Added
- **VolosR Style Redesign**: Complete UI overhaul with dark theme and large arc gauge.
- **Trending Graph**: New screen with live-updating line chart (cyan/dark theme).
- **Settings Screen**: Configuration interface for Min/Max values and Color Zones (Blue/Yellow/Red limits).
- **Dynamic Configuration**: Gauge range and colors now respond immediately to settings changes.

### Changed
- Replaced card-style widget with transparent/black container layout.
- Updated main screen background to black (`#000000`).

## [0.2.0] - 2025-12-09
### Added
- **Gas Widget UI**: Replaced simple bar gauge with a "Methane PPM" gas widget.
    - Semi-circular gauge with Red/Yellow/Green zones.
    - Dynamic status bar (SAFE/WARNING/DANGER).
    - Simulated real-time updates.
- **Trending Screen**: Added a secondary screen accessible via "Trending" button.
- **Dependencies**: Added `esp_driver_ppa`, `esp_mm`, `usb` (drivers) to CMakeLists.

### Fixed
- **Build System**: Restored missing `main/CMakeLists.txt` and fixed `undefined reference to app_main`.
- **Refactoring**: Cleaned up `lvgl_sw_rotation.c` structure (helper functions vs `app_main`).

## [v0.1.1] - 2025-12-09

### Added
- **RTC Integration**: Added RX8025T driver for persistent timekeeping.
- **Time Display**: Digital clock added to the top center of the main screen.
- **Timezone**: configured for Kuala Lumpur (GMT+8).
- **Fonts**: Enabled Montserrat font family (sizes 12-32) in SDK config.
- **NTP Sync**: Implemented automatic RTC update from NTP time.

### Fixed
- **Build Error**: Fixed missing font declaration `lv_font_montserrat_24`.

## [v0.1.0] - 2025-12-09

### Added
- **WiFi Integration**: Added ESP-Hosted support for ESP32-C6 slave.
- **NTP Sync**: Automatic time synchronization with `pool.ntp.org`.
- **Landscape Mode**: Fully working 1280x800 landscape orientation using PPA acceleration.
- **PSRAM Support**: Enabled 32MB Octal-SPI PSRAM for smooth rendering.
- **Project Structure**: Cleaned up CMakeLists and component dependencies.
- **Partition Table**: Custom 8MB app partition to support larger binary size.

### Fixed
- **LVGL Conflict**: Resolved API conflict between custom `lvgl_port_v9` and official `esp_lvgl_port` component.
- **Build Size**: Fixed "app partition too small" error by increasing partition size.
- **Credentials**: Updated with correct WiFi credentials.

### Configuration
- **SDKConfig**: Optimized for Performance (O2), PSRAM enabled, MODE=2 display buffering.
