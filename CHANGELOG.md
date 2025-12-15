# Changelog

## [v1.0.1] - 2025-12-16
### Added
- **Calibration Setup**:
    - Implemented PIN protection (Default: `8888`).
    - Added Calibration History Table showing the last 6 calibration dates.
    - Replaced manual text inputs with convenient Dropdown menus for Day, Month, and Year.
    - Year selection range: 2025 - 2035.
- **UI Enhancements**:
    - Improved layout of the Calibration/Service screen with a two-panel design.

### Fixed
- Fixed date input validation and persistence issues.
- Resolved compilation errors related to `esp_lcd_jd9165` component visibility.
- Fixed duplicate variable definitions in `lvgl_sw_rotation.c`.
