#include "bsp/led_overlay.h"
#include "led_strip_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static led_strip_handle_t hardware_strip;
static SemaphoreHandle_t output_mutex;
static uint32_t normal_colors[2];
static bool overlay_active;
static bool overlay_suppressed;
static bool output_dirty;
static uint8_t overlay_index;
static uint32_t overlay_color;

static esp_err_t render_locked(void)
{
    output_dirty = true;
    for (unsigned i = 0; i < 2; ++i) {
        uint32_t color = overlay_active && !overlay_suppressed && i == overlay_index ? overlay_color : normal_colors[i];
        esp_err_t ret = led_strip_set_pixel(hardware_strip, i, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
        if (ret != ESP_OK) return ret;
    }
    esp_err_t ret = led_strip_refresh(hardware_strip);
    if (ret == ESP_OK) output_dirty = false;
    return ret;
}

static esp_err_t proxy_set_pixel(led_strip_t *strip, uint32_t index, uint32_t r, uint32_t g, uint32_t b)
{
    (void)strip;
    if (index >= 2 || r > 255 || g > 255 || b > 255) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(output_mutex, portMAX_DELAY);
    normal_colors[index] = (r << 16) | (g << 8) | b;
    xSemaphoreGive(output_mutex);
    return ESP_OK;
}

static esp_err_t proxy_refresh(led_strip_t *strip)
{
    (void)strip;
    xSemaphoreTake(output_mutex, portMAX_DELAY);
    esp_err_t ret = render_locked();
    xSemaphoreGive(output_mutex);
    return ret;
}

static esp_err_t proxy_clear(led_strip_t *strip)
{
    (void)strip;
    xSemaphoreTake(output_mutex, portMAX_DELAY);
    normal_colors[0] = normal_colors[1] = 0;
    esp_err_t ret                       = render_locked();
    xSemaphoreGive(output_mutex);
    return ret;
}

static esp_err_t proxy_delete(led_strip_t *strip)
{
    (void)strip;
    return ESP_ERR_NOT_SUPPORTED;
}

static led_strip_t proxy_strip = {
    .set_pixel = proxy_set_pixel,
    .refresh   = proxy_refresh,
    .clear     = proxy_clear,
    .del       = proxy_delete,
};

esp_err_t bsp_led_overlay_wrap(led_strip_handle_t hardware, led_strip_handle_t *handle)
{
    if (!hardware || !handle) return ESP_ERR_INVALID_ARG;
    if (hardware_strip) return ESP_ERR_INVALID_STATE;
    output_mutex = xSemaphoreCreateMutex();
    if (!output_mutex) return ESP_ERR_NO_MEM;
    hardware_strip = hardware;
    *handle        = &proxy_strip;
    return ESP_OK;
}

esp_err_t bsp_led_overlay_set(bool active, uint8_t index, uint32_t color)
{
    if (index > 1 || color > 0xffffff) return ESP_ERR_INVALID_ARG;
    if (!hardware_strip) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(output_mutex, portMAX_DELAY);
    active         = active && !overlay_suppressed;
    bool changed   = active != overlay_active || (active && (index != overlay_index || color != overlay_color));
    overlay_active = active;
    overlay_index  = index;
    overlay_color  = color;
    esp_err_t ret  = ESP_OK;
    if (changed || output_dirty) {
        output_dirty = true;
        ret          = render_locked();
    }
    xSemaphoreGive(output_mutex);
    return ret;
}

esp_err_t bsp_led_overlay_suppress(bool suppressed)
{
    if (!hardware_strip) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(output_mutex, portMAX_DELAY);
    overlay_suppressed = suppressed;
    bool was_active    = overlay_active;
    overlay_active     = false;
    esp_err_t ret      = ESP_OK;
    if (was_active || output_dirty) {
        output_dirty = true;
        ret          = render_locked();
    }
    xSemaphoreGive(output_mutex);
    return ret;
}

bool bsp_led_overlay_allowed(void)
{
    if (!hardware_strip) return false;
    xSemaphoreTake(output_mutex, portMAX_DELAY);
    bool allowed = !overlay_suppressed;
    xSemaphoreGive(output_mutex);
    return allowed;
}
