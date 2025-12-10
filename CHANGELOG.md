# Changelog

All notable changes to this project will be documented in this file.

## [v0.5.0] - 2025-12-10
### Fixed
- **Modbus Connectivity**: Fixed `mdd:err` by swapping UART pins (TX: GPIO 38, RX: GPIO 37).
- **Serial Interference**: Enforced strict silence on UART0 (Console Logging Disabled) to prevent RS485 bus corruption.
- **Flow Control**: Removed hardware RTS control (DE/RE not required).

## [v0.4.3] - 2025-12-10
### Changed
- **Modbus Hardware**: Moved Modbus UART pins to **GPIO 37 (TX) / 38 (RX)** to share the port with the flashing interface.
- **Console**: Disabled Bootloader and Application logging on the default UART to ensure a clean line for Modbus RS485 communication.

## [v0.4.2] - 2025-12-10
### Added
- **Safety Settings Tab**: New configuration tab for configuring system-wide safety logic.
- **Master Relay Logic**: Ability to assign any of the 16 relays as a "Master Warning" output.
- **Invert Mode**: Toggle switch for NO/NC (Active Low/High) relay logic.
- **Threshold Monitoring**: Automatic scanning of all 16 gauges; triggers Master Relay if any gauge exceeds its "Yellow" limit.
- **NVS Persistence**: Safety settings (Master Index, Invert Mode) are saved and loaded on boot.

### Changed
- **Modbus Control**: Implemented `modbus_set_relay` to allow write operations to relay coils.
- **Code Restoration**: Reconstructed `lvgl_sw_rotation.c` to eliminate massive code duplication and restore compilation stability.

## v0.3.8
- **Bug Fix:** System Info tab now correctly auto-updates WiFi Status and IP Address in real-time.
- **Rollback:** Removed Boot Splash Screen feature to resolve WDT stability issues.
- **Maintenance:** Cleaned up project configuration and dependencies.

## v0.3.7
- **Settings Tabs:** Organized configuration into "Gauge Config" and "System Info" tabs.
- **System Info:** added tab displaying Firmware Version, WiFi Status, and IP Address.
- **Refactoring:** Improved settings screen structure using `lv_tabview`.

## v0.3.6
- **16-Gauge Expansion:** Support for 16 gauges across 2 pages.
- **Paging Support:** Navigation buttons (Next/Prev) with automatic rendering optimization.
- **NVS Robustness:** Added sanity checks to startup to fix invalid 0-0 ranges from legacy saves.
- **UI Refinement:** Value alignment centered (-3px correction) and animation flicker fixed on page switch.
- **Settings:** Configuration dropdown supports all 16 gauges.
## [v0.4.0] - 2025-12-10
### Added
- **Modbus Integration:** Control of external IO modules via RS485 (UART2).
- **Relay Monitor Screen:** New Page 3 displaying status of 16 Relays and 4 Digital Input Buttons.
- **Gauge Mapping:** Mapped 16 Analog Input channels to the main dashboard gauges.
- **Navigation:** Updated touch navigation to cycle through 3 pages.

## [v0.3.5] - 2025-12-09
### Added
- **NVS Persistence**: Gauge settings (Name, Unit, Limits) are now automatically saved to NVS and loaded on boot.
- **Improved Settings UI**: Compacted row height and text area size to ensure on-screen keyboard visibility without scrolling.

## [v0.3.4] - 2025-12-09
### Added
- **8-Gauge Grid**: Implemented 4x2 grid layout supporting 8 simultaneous gauges.
- **Layout Optimization**: Resized gauges to 260x330px to fit 1280x800 screen without scrolling.
- **Side Margins**: Added reserved space on left/right for future UI expansion.
- **WiFi Status**: Dynamic Green/Red status icon added to top bar.
- **Multi-Gauge Config**: Added dropdown in Settings to switch between Gauge 1-8 configuration contexts.

### Fixed
- **Settings Crash**: Fixed invalid pointer dereference when opening Settings.
- **Back Button Crash**: Fixed unsafe object access when returning from Settings to Main Screen.

## [v0.3.3] - 2025-12-09
### Fixed
- **Stability**: Validated and restored smooth gauge animation.
- **Logging**: Confirmed PSRAM logging is active and performant.
- **rollback**: Reverted performance hacks that caused stuttering.

## [v0.3.2] - 2025-12-09
### Added
- **Configurable Units**: Added "Gauge Unit" field to settings (e.g. PPM, %, Volts).
- **Dynamic Labels**: Main screen unit label updates immediately upon configuration change.

## [v0.3.1] - 2025-12-09
### Added
- **Settings UI Polish**: Replaced +/- spinboxes with modern `lv_textarea` input fields.
- **On-Screen Keyboard**: Automatic virtual keyboard for entering names and numbers.
- **Gauge Naming**: Added "Gauge Name" field to rename the sensor (e.g. "Methane").
- **Layout Refinements**: Widen settings rows and increased font sizes for better readability.

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
