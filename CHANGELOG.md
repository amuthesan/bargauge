# Changelog

All notable changes to this project will be documented in this file.

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
