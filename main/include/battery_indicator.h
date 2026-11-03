#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BATTERY_INDICATOR_MAX_RULES            8
#define BATTERY_INDICATOR_MAX_INTERVAL_SECONDS 86400
#define BATTERY_INDICATOR_PULSE_MS             300

typedef struct {
    uint8_t min_percent;
    uint8_t max_percent;
    uint32_t color;
    uint32_t interval_seconds;
} battery_indicator_rule_t;

typedef struct {
    bool enabled;
    uint8_t led_index;
    uint8_t rule_count;
    battery_indicator_rule_t rules[BATTERY_INDICATOR_MAX_RULES];
} battery_indicator_config_t;

typedef enum {
    BATTERY_INDICATOR_VALID,
    BATTERY_INDICATOR_INVALID_CONFIG,
    BATTERY_INDICATOR_INVALID_RULE,
    BATTERY_INDICATOR_OVERLAP,
} battery_indicator_validation_t;

typedef struct {
    bool started;
    uint64_t last_flash_ms;
    uint64_t pulse_until_ms;
    uint32_t pulse_color;
} battery_indicator_state_t;

battery_indicator_validation_t battery_indicator_validate(const battery_indicator_config_t *config);
int battery_indicator_find_rule(const battery_indicator_config_t *config, int percentage);
bool battery_indicator_step(const battery_indicator_config_t *config, battery_indicator_state_t *state, int percentage,
                            bool suppressed, uint64_t now_ms, uint32_t *color);

#ifdef __cplusplus
}
#endif
