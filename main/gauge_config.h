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

#endif // GAUGE_CONFIG_H
