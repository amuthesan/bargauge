# Changelog

All notable changes to this project will be documented in this file.

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
