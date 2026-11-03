#pragma once

#include <stdbool.h>
#include "led_strip.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_led_overlay_wrap(led_strip_handle_t hardware, led_strip_handle_t *handle);
esp_err_t bsp_led_overlay_set(bool active, uint8_t index, uint32_t color);
esp_err_t bsp_led_overlay_suppress(bool suppressed);
bool bsp_led_overlay_allowed(void);

#ifdef __cplusplus
}
#endif
