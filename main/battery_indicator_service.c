#include "battery_indicator.h"
#include "cJSON.h"
#include "battery_indicator_service.h"
#include "adc_detect.h"
#include "bsp/esp_duo.h"
#include "bsp/led_overlay.h"
#include "esp_crc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "rgb_matrix.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "battery_indicator";
static SemaphoreHandle_t config_mutex;
static battery_indicator_config_t current_config = {
    .enabled    = false,
    .led_index  = 1,
    .rule_count = 3,
    .rules      = {{76, 100, 0x00ff00, 30}, {30, 75, 0xffff00, 60}, {0, 19, 0xff0000, 10}},
};
static battery_indicator_state_t scheduler;
static bool config_load_failed;

typedef struct {
    uint32_t min_percent;
    uint32_t max_percent;
    uint32_t color;
    uint32_t interval_seconds;
} saved_rule_t;

typedef struct {
    uint32_t version;
    uint32_t enabled;
    uint32_t led_index;
    uint32_t rule_count;
    saved_rule_t rules[BATTERY_INDICATOR_MAX_RULES];
    uint32_t crc;
} saved_config_t;

static esp_err_t save_config(const battery_indicator_config_t *config)
{
    saved_config_t saved = {0};
    saved.version        = 1;
    saved.enabled        = config->enabled;
    saved.led_index      = config->led_index;
    saved.rule_count     = config->rule_count;
    for (unsigned i = 0; i < config->rule_count; ++i) {
        saved.rules[i] = (saved_rule_t){config->rules[i].min_percent, config->rules[i].max_percent,
                                        config->rules[i].color, config->rules[i].interval_seconds};
    }
    saved.crc = esp_crc32_le(0, (const uint8_t *)&saved, sizeof(saved) - sizeof(saved.crc));
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("battery_ind", NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_blob(handle, "config", &saved, sizeof(saved));
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

static esp_err_t load_config(battery_indicator_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("battery_ind", NVS_READONLY, &handle);
    if (ret != ESP_OK) return ret;
    saved_config_t saved = {0};
    size_t size          = sizeof(saved);
    ret                  = nvs_get_blob(handle, "config", &saved, &size);
    nvs_close(handle);
    if (ret != ESP_OK) return ret;
    if (size != sizeof(saved) || saved.version != 1 || saved.enabled > 1 || saved.led_index > 1 ||
        saved.rule_count > BATTERY_INDICATOR_MAX_RULES)
        return ESP_ERR_INVALID_ARG;
    uint32_t crc = esp_crc32_le(0, (const uint8_t *)&saved, sizeof(saved) - sizeof(saved.crc));
    if (crc != saved.crc) return ESP_ERR_INVALID_CRC;
    *config            = (battery_indicator_config_t){0};
    config->enabled    = saved.enabled != 0;
    config->led_index  = (uint8_t)saved.led_index;
    config->rule_count = (uint8_t)saved.rule_count;
    for (unsigned i = 0; i < config->rule_count; ++i) {
        if (saved.rules[i].min_percent > 100 || saved.rules[i].max_percent > 100) return ESP_ERR_INVALID_ARG;
        config->rules[i] =
            (battery_indicator_rule_t){(uint8_t)saved.rules[i].min_percent, (uint8_t)saved.rules[i].max_percent,
                                       saved.rules[i].color, saved.rules[i].interval_seconds};
    }
    return battery_indicator_validate(config) == BATTERY_INDICATOR_VALID ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t battery_indicator_service_init(void)
{
    config_mutex = xSemaphoreCreateMutex();
    if (!config_mutex) return ESP_ERR_NO_MEM;
    battery_indicator_config_t loaded = {0};
    esp_err_t ret                     = load_config(&loaded);
    if (ret == ESP_OK) {
        current_config = loaded;
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved battery indicator configuration; disabled by default");
    } else {
        config_load_failed = true;
        ESP_LOGE(TAG, "Battery indicator configuration could not be loaded: %s; indicator disabled",
                 esp_err_to_name(ret));
    }
    return ESP_OK;
}

void battery_indicator_service_tick(void)
{
    if (!config_mutex) return;
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    uint32_t color = 0;
    bool suppressed =
        g_usb_connected || !bsp_ws2812_is_enable() || !rgb_matrix_is_enabled() || !bsp_led_overlay_allowed();
    bool active   = battery_indicator_step(&current_config, &scheduler, g_battery_percentage, suppressed,
                                           (uint64_t)esp_timer_get_time() / 1000, &color);
    esp_err_t ret = bsp_led_overlay_set(active, current_config.led_index, color);
    xSemaphoreGive(config_mutex);
    static esp_err_t previous_error = ESP_OK;
    if (ret != ESP_OK && ret != previous_error) ESP_LOGE(TAG, "LED output failed: %s", esp_err_to_name(ret));
    previous_error = ret;
}

static bool read_integer(const cJSON *object, const char *name, uint32_t min, uint32_t max, uint32_t *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(item) || !(item->valuedouble >= min && item->valuedouble <= max)) return false;
    uint32_t integer = (uint32_t)item->valuedouble;
    if (item->valuedouble != integer) return false;
    *value = integer;
    return true;
}

static bool read_color(const cJSON *object, uint32_t *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, "color");
    if (!cJSON_IsString(item) || strlen(item->valuestring) != 7 || item->valuestring[0] != '#') return false;
    uint32_t color = 0;
    for (unsigned i = 1; i < 7; ++i) {
        char digit = item->valuestring[i];
        uint32_t part;
        if (digit >= '0' && digit <= '9')
            part = digit - '0';
        else if (digit >= 'a' && digit <= 'f')
            part = digit - 'a' + 10;
        else if (digit >= 'A' && digit <= 'F')
            part = digit - 'A' + 10;
        else
            return false;
        color = (color << 4) | part;
    }
    *value = color;
    return true;
}

static const char *parse_config(const cJSON *json, battery_indicator_config_t *config)
{
    const cJSON *object  = cJSON_GetObjectItemCaseSensitive(json, "config");
    const cJSON *enabled = cJSON_GetObjectItemCaseSensitive(object, "enabled");
    const cJSON *key     = cJSON_GetObjectItemCaseSensitive(object, "key");
    const cJSON *rules   = cJSON_GetObjectItemCaseSensitive(object, "rules");
    if (!cJSON_IsObject(object) || !cJSON_IsBool(enabled) || !cJSON_IsString(key) ||
        (strcmp(key->valuestring, "left") != 0 && strcmp(key->valuestring, "right") != 0) || !cJSON_IsArray(rules) ||
        cJSON_GetArraySize(rules) > BATTERY_INDICATOR_MAX_RULES) {
        return "batteryIndicatorInvalidConfig";
    }
    *config            = (battery_indicator_config_t){0};
    config->enabled    = cJSON_IsTrue(enabled);
    config->led_index  = strcmp(key->valuestring, "left") == 0 ? 1 : 0;
    config->rule_count = (uint8_t)cJSON_GetArraySize(rules);
    for (unsigned i = 0; i < config->rule_count; ++i) {
        const cJSON *rule = cJSON_GetArrayItem(rules, (int)i);
        uint32_t min, max, interval, color;
        if (!cJSON_IsObject(rule) || !read_integer(rule, "min_percent", 0, 100, &min) ||
            !read_integer(rule, "max_percent", 0, 100, &max) ||
            !read_integer(rule, "interval_seconds", 1, BATTERY_INDICATOR_MAX_INTERVAL_SECONDS, &interval) ||
            !read_color(rule, &color))
            return "batteryIndicatorInvalidRule";
        config->rules[i] = (battery_indicator_rule_t){(uint8_t)min, (uint8_t)max, color, interval};
    }
    switch (battery_indicator_validate(config)) {
        case BATTERY_INDICATOR_VALID:
            return NULL;
        case BATTERY_INDICATOR_OVERLAP:
            return "batteryIndicatorOverlap";
        case BATTERY_INDICATOR_INVALID_RULE:
            return "batteryIndicatorInvalidRule";
        default:
            return "batteryIndicatorInvalidConfig";
    }
}

static cJSON *config_to_json(const battery_indicator_config_t *config)
{
    cJSON *object = cJSON_CreateObject();
    if (!object) return NULL;
    cJSON *rules = cJSON_AddArrayToObject(object, "rules");
    if (!rules || !cJSON_AddBoolToObject(object, "enabled", config->enabled) ||
        !cJSON_AddStringToObject(object, "key", config->led_index == 1 ? "left" : "right"))
        goto fail;
    for (unsigned i = 0; i < config->rule_count; ++i) {
        cJSON *rule = cJSON_CreateObject();
        if (!rule) goto fail;
        if (!cJSON_AddItemToArray(rules, rule)) {
            cJSON_Delete(rule);
            goto fail;
        }
        char color[8];
        snprintf(color, sizeof(color), "#%06lx", (unsigned long)config->rules[i].color);
        if (!cJSON_AddNumberToObject(rule, "min_percent", config->rules[i].min_percent) ||
            !cJSON_AddNumberToObject(rule, "max_percent", config->rules[i].max_percent) ||
            !cJSON_AddStringToObject(rule, "color", color) ||
            !cJSON_AddNumberToObject(rule, "interval_seconds", config->rules[i].interval_seconds))
            goto fail;
    }
    return object;
fail:
    cJSON_Delete(object);
    return NULL;
}

esp_err_t battery_indicator_handle_request(httpd_req_t *req, const char *type, const cJSON *json)
{
    if (!config_mutex) {
        ESP_LOGE(TAG, "Battery indicator service is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    battery_indicator_config_t config = {0};
    const char *error                 = NULL;
    bool saving                       = strcmp(type, "set_battery_indicator_config") == 0;
    if (saving) error = parse_config(json, &config);
    if (saving && !error) {
        esp_err_t ret = save_config(&config);
        if (ret == ESP_OK) {
            xSemaphoreTake(config_mutex, portMAX_DELAY);
            current_config     = config;
            scheduler          = (battery_indicator_state_t){0};
            config_load_failed = false;
            xSemaphoreGive(config_mutex);
        } else {
            ESP_LOGE(TAG, "Could not save battery indicator configuration: %s", esp_err_to_name(ret));
            error = "batteryIndicatorSaveFailed";
        }
    }
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    if (!saving && config_load_failed) error = "batteryIndicatorLoadFailed";
    config = current_config;
    xSemaphoreGive(config_mutex);
    if (error) ESP_LOGW(TAG, "Battery indicator request rejected: %s", error);

    cJSON *response = cJSON_CreateObject();
    cJSON *settings = config_to_json(&config);
    if (!response || !settings) {
        cJSON_Delete(response);
        cJSON_Delete(settings);
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddItemToObject(response, "config", settings)) {
        cJSON_Delete(settings);
        cJSON_Delete(response);
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddStringToObject(response, "type", "battery_indicator_config") ||
        !cJSON_AddStringToObject(response, "action", saving ? "save" : "load") ||
        !cJSON_AddBoolToObject(response, "success", error == NULL) ||
        (error && !cJSON_AddStringToObject(response, "error", error))) {
        cJSON_Delete(response);
        return ESP_ERR_NO_MEM;
    }
    const cJSON *request_id = cJSON_GetObjectItemCaseSensitive(json, "request_id");
    if (cJSON_IsString(request_id) && strlen(request_id->valuestring) <= 64 &&
        !cJSON_AddStringToObject(response, "request_id", request_id->valuestring)) {
        cJSON_Delete(response);
        return ESP_ERR_NO_MEM;
    }
    char *payload = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    if (!payload) return ESP_ERR_NO_MEM;
    httpd_ws_frame_t frame = {.type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)payload, .len = strlen(payload)};
    esp_err_t ret          = httpd_ws_send_frame(req, &frame);
    free(payload);
    if (ret != ESP_OK) ESP_LOGE(TAG, "Could not send configuration response: %s", esp_err_to_name(ret));
    return ret;
}
