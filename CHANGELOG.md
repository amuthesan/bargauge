# Changelog

## [v1.0.3] - 2025-12-16
### Added
- **Dashboard Layout**: Moved "MB: Status" to bottom. Added Title Header.

## [v1.0.4] - 2025-12-16
### Added
- **Dashboard Icon**: Added high-resolution Unisem logo (ARGB8888 transparent) to the top-right corner.
- **UI Layout**: Moved Time and WiFi status to the top-left to accommodate the new logo.
## [v1.0.5] - 2025-12-16
### Added
- **Service Page Redesign**:
    - Full-screen layout for "Calibration Setup" (Removed panels).
    - Moved "PREVIEW" and "SAVE & EXIT" buttons to bottom corners for ergonomics.
    - Centered input fields and History table.
- **Service Reminder**:
    - Integrated large "Exentec" logo (200% scaling).
    - Bold, large "SERVICE REMINDER" title (Font 48).
    - Improved layout for contact information.
- **Service Page Features**:
    - Added "Preview Reminder" button.
    - Renamed "Expiry" to "Remind In".

## [v1.0.6] - 2025-12-16
### Fixed
- **UI Spacing**: Increased spacing between Time Label and WiFi Icon in the dashboard header to prevent overlapping.

## [v1.0.2] - 2025-12-16
### Added
- **Gauge Status Labels**:
    - Added "SAFE" (Green) / "WARNING" (Red) status box above Trending button.
    - Status updates dynamically based on configured threshold.
    - Large customizable font and box styling.

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
