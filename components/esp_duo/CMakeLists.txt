idf_component_register(SRCS "esp_duo.c" "led_overlay.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_adc led_strip)
