#include "battery_indicator.h"

#define CHECK(condition)                   \
    do {                                   \
        if (!(condition)) return __LINE__; \
    } while (0)

int main(void)
{
    battery_indicator_config_t config = {
        .enabled    = true,
        .led_index  = 1,
        .rule_count = 3,
        .rules      = {{76, 100, 0x00ff00, 30}, {30, 75, 0xffff00, 60}, {0, 19, 0xff0000, 10}},
    };
    CHECK(battery_indicator_validate(&config) == BATTERY_INDICATOR_VALID);
    CHECK(battery_indicator_find_rule(&config, 100) == 0);
    CHECK(battery_indicator_find_rule(&config, 76) == 0);
    CHECK(battery_indicator_find_rule(&config, 75) == 1);
    CHECK(battery_indicator_find_rule(&config, 30) == 1);
    CHECK(battery_indicator_find_rule(&config, 29) == -1);
    CHECK(battery_indicator_find_rule(&config, 20) == -1);
    CHECK(battery_indicator_find_rule(&config, 19) == 2);
    CHECK(battery_indicator_find_rule(&config, 0) == 2);
    CHECK(battery_indicator_find_rule(&config, -1) == -1);
    CHECK(battery_indicator_find_rule(&config, 101) == -1);

    battery_indicator_config_t bad = config;
    bad.rules[1].max_percent       = 76;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_OVERLAP);
    bad                           = config;
    bad.rules[0].interval_seconds = 0;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_INVALID_RULE);
    bad.rules[0].interval_seconds = 86401;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_INVALID_RULE);
    bad.rules[0].interval_seconds = 86400;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_VALID);
    bad.rules[0].min_percent = 101;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_INVALID_RULE);
    bad                = config;
    bad.rules[0].color = 0x1000000;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_INVALID_RULE);
    bad           = config;
    bad.led_index = 2;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_INVALID_CONFIG);
    bad            = config;
    bad.rule_count = 9;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_INVALID_CONFIG);
    bad.rule_count = 0;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_INVALID_CONFIG);
    bad.enabled = false;
    CHECK(battery_indicator_validate(&bad) == BATTERY_INDICATOR_VALID);

    battery_indicator_state_t state = {0};
    uint32_t color                  = 0;
    CHECK(!battery_indicator_step(&config, &state, 80, false, 1000, &color));
    CHECK(!battery_indicator_step(&config, &state, 80, false, 30999, &color));
    CHECK(battery_indicator_step(&config, &state, 80, false, 31000, &color));
    CHECK(color == 0x00ff00);
    CHECK(battery_indicator_step(&config, &state, 80, false, 31299, &color));
    CHECK(!battery_indicator_step(&config, &state, 80, false, 31300, &color));
    CHECK(!battery_indicator_step(&config, &state, 75, false, 32000, &color));
    CHECK(!battery_indicator_step(&config, &state, 76, false, 33000, &color));
    CHECK(!battery_indicator_step(&config, &state, 75, false, 34000, &color));
    CHECK(!battery_indicator_step(&config, &state, 76, false, 60999, &color));
    CHECK(battery_indicator_step(&config, &state, 76, false, 61000, &color));
    CHECK(!battery_indicator_step(&config, &state, 76, true, 61100, &color));
    CHECK(!battery_indicator_step(&config, &state, 76, false, 70000, &color));
    CHECK(!battery_indicator_step(&config, &state, 76, false, 99999, &color));
    CHECK(battery_indicator_step(&config, &state, 76, false, 100000, &color));
    CHECK(!battery_indicator_step(&config, &state, 25, false, 100100, &color));
    CHECK(!battery_indicator_step(&config, &state, 76, false, 100200, &color));
    CHECK(!battery_indicator_step(&config, &state, 10, false, 109999, &color));
    CHECK(battery_indicator_step(&config, &state, 10, false, 110000, &color));
    CHECK(color == 0xff0000);
    config.enabled = false;
    CHECK(!battery_indicator_step(&config, &state, 10, false, 110100, &color));
    config.enabled = true;
    CHECK(!battery_indicator_step(&config, &state, 10, false, 120000, &color));
    CHECK(battery_indicator_step(&config, &state, 10, false, 130000, &color));

    return 0;
}
