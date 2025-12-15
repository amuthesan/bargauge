#ifndef GAUGE_CONFIG_H
#define GAUGE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// --- Configuration Structure ---
typedef struct {
    char name[32];    // Gauge Name
    char unit[16];    // Gauge Unit (e.g. "PPM")
    int min_val;
    int max_val;
    int blue_limit;   // 0 to blue_limit (Cyan)
    int yellow_limit; // blue_limit to yellow_limit (Yellow)
    int red_limit;    // yellow_limit to red_limit (Warning?? No, usually Red starts here)
    int threshold;    // Future use
    int analog_min;   // Raw Analog Input Min (e.g. 4000)
    int analog_max;   // Raw Analog Input Max (e.g. 20000)
    int trigger_relay_index; // 0: None, 1-16: Relay to trigger on alarm (Independent)
} GasGaugeConfig;

typedef struct {
    int siren_relay_index;  // 0: None, 1-16: Relay Index
    bool siren_invert;      // false: Active=ON, true: Active=OFF
    int strobe_relay_index; // 0: None, 1-16: Relay Index
    bool strobe_invert;     // false: Active=ON, true: Active=OFF
} SafetyConfig;

typedef struct {
    // Calibration parameters
    uint16_t cal_year;
    uint8_t  cal_month; // 1-12
    uint8_t  cal_day;   // 1-31
    
    // 0: 6 months, 1: 1 year
    uint8_t  expiry_period; 
    
    // Acknowledgement tracking
    uint16_t last_ack_year;
    uint8_t  last_ack_month;
    uint8_t  last_ack_day;

    // History (Last 6 Calibrations)
    // Index 0 is the most recent (previous) calibration
    uint16_t history_year[6];
    uint8_t  history_month[6];
    uint8_t  history_day[6];
} ServiceConfig;

#endif // GAUGE_CONFIG_H
