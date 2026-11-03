#include "battery_indicator.h"

battery_indicator_validation_t battery_indicator_validate(const battery_indicator_config_t *config)
{
    if (!config || config->led_index > 1 || config->rule_count > BATTERY_INDICATOR_MAX_RULES ||
        (config->enabled && config->rule_count == 0)) {
        return BATTERY_INDICATOR_INVALID_CONFIG;
    }
    for (unsigned i = 0; i < config->rule_count; ++i) {
        const battery_indicator_rule_t *rule = &config->rules[i];
        if (rule->min_percent > rule->max_percent || rule->max_percent > 100 || rule->color > 0xffffff ||
            rule->interval_seconds < 1 || rule->interval_seconds > BATTERY_INDICATOR_MAX_INTERVAL_SECONDS) {
            return BATTERY_INDICATOR_INVALID_RULE;
        }
        for (unsigned j = 0; j < i; ++j) {
            if (rule->min_percent <= config->rules[j].max_percent &&
                rule->max_percent >= config->rules[j].min_percent) {
                return BATTERY_INDICATOR_OVERLAP;
            }
        }
    }
    return BATTERY_INDICATOR_VALID;
}

int battery_indicator_find_rule(const battery_indicator_config_t *config, int percentage)
{
    if (percentage < 0 || percentage > 100) return -1;
    for (unsigned i = 0; i < config->rule_count; ++i) {
        if (percentage >= config->rules[i].min_percent && percentage <= config->rules[i].max_percent) return (int)i;
    }
    return -1;
}

bool battery_indicator_step(const battery_indicator_config_t *config, battery_indicator_state_t *state, int percentage,
                            bool suppressed, uint64_t now_ms, uint32_t *color)
{
    if (!config->enabled || suppressed) {
        *state = (battery_indicator_state_t){0};
        return false;
    }
    if (!state->started) {
        state->started       = true;
        state->last_flash_ms = now_ms;
    }
    int rule_index = battery_indicator_find_rule(config, percentage);
    if (rule_index < 0) {
        state->pulse_until_ms = 0;
        return false;
    }
    if (now_ms < state->pulse_until_ms) {
        *color = state->pulse_color;
        return true;
    }
    const battery_indicator_rule_t *rule = &config->rules[rule_index];
    // Keep the last flash across range transitions so boundary noise cannot retrigger a pulse.
    if (now_ms - state->last_flash_ms >= (uint64_t)rule->interval_seconds * 1000) {
        state->last_flash_ms  = now_ms;
        state->pulse_until_ms = now_ms + BATTERY_INDICATOR_PULSE_MS;
        state->pulse_color    = rule->color;
        *color                = rule->color;
        return true;
    }
    return false;
}
