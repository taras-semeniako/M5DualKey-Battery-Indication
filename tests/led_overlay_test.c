#include "bsp/led_overlay.h"
#include "led_strip_interface.h"
#include "freertos/semphr.h"

#define CHECK(condition)                   \
    do {                                   \
        if (!(condition)) return __LINE__; \
    } while (0)

static uint32_t pixels[2];
static int refreshes;
static bool fail_refresh;
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return (void *)1;
}
int xSemaphoreTake(SemaphoreHandle_t mutex, unsigned ticks)
{
    (void)mutex;
    (void)ticks;
    return 1;
}
int xSemaphoreGive(SemaphoreHandle_t mutex)
{
    (void)mutex;
    return 1;
}

static esp_err_t set_pixel(led_strip_t *strip, uint32_t index, uint32_t r, uint32_t g, uint32_t b)
{
    (void)strip;
    pixels[index] = (r << 16) | (g << 8) | b;
    return ESP_OK;
}
static esp_err_t refresh(led_strip_t *strip)
{
    (void)strip;
    ++refreshes;
    return fail_refresh ? ESP_FAIL : ESP_OK;
}
esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index, uint32_t r, uint32_t g, uint32_t b)
{
    return strip->set_pixel(strip, index, r, g, b);
}
esp_err_t led_strip_refresh(led_strip_handle_t strip)
{
    return strip->refresh(strip);
}

int main(void)
{
    led_strip_t hardware     = {.set_pixel = set_pixel, .refresh = refresh};
    led_strip_handle_t proxy = 0;
    CHECK(bsp_led_overlay_wrap(&hardware, &proxy) == ESP_OK);
    CHECK(proxy != &hardware);
    CHECK(led_strip_set_pixel(proxy, 0, 0x12, 0x34, 0x56) == ESP_OK);
    CHECK(led_strip_set_pixel(proxy, 1, 0x65, 0x43, 0x21) == ESP_OK);
    CHECK(led_strip_refresh(proxy) == ESP_OK);
    CHECK(pixels[0] == 0x123456 && pixels[1] == 0x654321);
    CHECK(bsp_led_overlay_set(true, 1, 0xff0000) == ESP_OK);
    CHECK(pixels[0] == 0x123456 && pixels[1] == 0xff0000);
    CHECK(led_strip_set_pixel(proxy, 1, 0xab, 0xcd, 0xef) == ESP_OK);
    CHECK(led_strip_refresh(proxy) == ESP_OK);
    CHECK(pixels[1] == 0xff0000);
    CHECK(bsp_led_overlay_set(false, 1, 0) == ESP_OK);
    CHECK(pixels[0] == 0x123456 && pixels[1] == 0xabcdef);
    CHECK(bsp_led_overlay_set(true, 0, 0x00ff00) == ESP_OK);
    CHECK(pixels[0] == 0x00ff00 && pixels[1] == 0xabcdef);
    CHECK(bsp_led_overlay_suppress(true) == ESP_OK);
    CHECK(!bsp_led_overlay_allowed());
    CHECK(pixels[0] == 0x123456 && pixels[1] == 0xabcdef);
    CHECK(bsp_led_overlay_set(true, 0, 0xff0000) == ESP_OK);
    CHECK(led_strip_refresh(proxy) == ESP_OK);
    CHECK(pixels[0] == 0x123456);
    CHECK(bsp_led_overlay_suppress(false) == ESP_OK);
    CHECK(bsp_led_overlay_allowed());
    CHECK(pixels[0] == 0x123456);
    CHECK(bsp_led_overlay_set(true, 1, 0xff0000) == ESP_OK);
    CHECK(proxy->clear(proxy) == ESP_OK);
    CHECK(pixels[0] == 0 && pixels[1] == 0xff0000);
    CHECK(bsp_led_overlay_set(false, 1, 0) == ESP_OK);
    CHECK(pixels[0] == 0 && pixels[1] == 0);
    CHECK(led_strip_set_pixel(proxy, 2, 1, 2, 3) == ESP_ERR_INVALID_ARG);
    CHECK(bsp_led_overlay_set(true, 2, 0) == ESP_ERR_INVALID_ARG);
    CHECK(bsp_led_overlay_set(true, 1, 0x1000000) == ESP_ERR_INVALID_ARG);
    fail_refresh = true;
    CHECK(bsp_led_overlay_set(true, 0, 0xff0000) == ESP_FAIL);
    int attempts = refreshes;
    fail_refresh = false;
    CHECK(bsp_led_overlay_set(true, 0, 0xff0000) == ESP_OK);
    CHECK(refreshes == attempts + 1);
    return 0;
}
