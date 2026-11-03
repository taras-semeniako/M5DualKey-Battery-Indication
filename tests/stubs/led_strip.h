#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef struct led_strip_t *led_strip_handle_t;
esp_err_t led_strip_set_pixel(led_strip_handle_t, uint32_t, uint32_t, uint32_t, uint32_t);
esp_err_t led_strip_refresh(led_strip_handle_t);
