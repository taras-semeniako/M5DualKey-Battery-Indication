#pragma once

#include "esp_http_server.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t battery_indicator_service_init(void);
void battery_indicator_service_tick(void);
esp_err_t battery_indicator_handle_request(httpd_req_t *req, const char *type, const cJSON *json);

#ifdef __cplusplus
}
#endif
