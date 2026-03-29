#include "http_server.h"

#include "app_control.h"
#include "app_state.h"
#include "cJSON.h"
#include "config.h"
#include "esp_log.h"
#include "rgb_led.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "HTTP_SERVER";
static sensor_data_t *g_sensor_data = NULL;

// 引用嵌入的HTML文件
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static esp_err_t send_json_status(httpd_req_t *req, const char *status, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON_AddBoolToObject(root, "ok", false);
    cJSON_AddStringToObject(root, "message", message);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ret;
}

static esp_err_t send_ok(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static esp_err_t recv_request_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    if (req->content_len <= 0) {
        return send_json_status(req, "400 Bad Request", "empty body");
    }

    if ((size_t)req->content_len >= buf_size) {
        return send_json_status(req, "400 Bad Request", "body too large");
    }

    int total = 0;
    while (total < req->content_len) {
        int received = httpd_req_recv(req, buf + total, req->content_len - total);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return send_json_status(req, "400 Bad Request", "failed to read body");
        }
        total += received;
    }

    buf[total] = '\0';
    return ESP_OK;
}

static esp_err_t require_sensor_data(httpd_req_t *req)
{
    if (g_sensor_data == NULL) {
        return send_json_status(req, "500 Internal Server Error", "sensor data unavailable");
    }
    return ESP_OK;
}

static esp_err_t lock_state_or_503(httpd_req_t *req)
{
    if (app_state_lock() != ESP_OK) {
        return send_json_status(req, "503 Service Unavailable", "state lock timeout");
    }
    return ESP_OK;
}

static bool query_get_int(httpd_req_t *req, const char *key, int *out)
{
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }

    char param[32];
    if (httpd_query_key_value(query, key, param, sizeof(param)) != ESP_OK) {
        return false;
    }

    *out = atoi(param);
    return true;
}

static esp_err_t register_uri_handler_checked(httpd_handle_t server, const httpd_uri_t *uri)
{
    esp_err_t ret = httpd_register_uri_handler(server, uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register URI %s (%d)", uri->uri, ret);
    }
    return ret;
}

// 主页处理
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    size_t html_len = index_html_end - index_html_start;
    return httpd_resp_send(req, (const char *)index_html_start, html_len);
}

// API数据处理
static esp_err_t api_data_handler(httpd_req_t *req)
{
    if (require_sensor_data(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (lock_state_or_503(req) != ESP_OK) {
        return ESP_FAIL;
    }

    sensor_data_t snapshot = *g_sensor_data;
    app_state_unlock();

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON_AddNumberToObject(root, "temperature", snapshot.temperature);
    cJSON_AddNumberToObject(root, "humidity", snapshot.humidity);
    cJSON_AddNumberToObject(root, "light", snapshot.light);
    cJSON_AddNumberToObject(root, "smoke", snapshot.smoke);
    cJSON_AddNumberToObject(root, "smoke_threshold", snapshot.smoke_threshold);
    cJSON_AddNumberToObject(root, "led_state", snapshot.led_state);
    cJSON_AddNumberToObject(root, "led_brightness", snapshot.led_brightness);
    cJSON_AddNumberToObject(root, "fan_state", snapshot.fan_state);
    cJSON_AddNumberToObject(root, "fan_speed", snapshot.fan_speed);
    cJSON_AddNumberToObject(root, "curtain_state", snapshot.curtain_state);
    cJSON_AddNumberToObject(root, "control_mode", snapshot.control_mode);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ret;
}

static esp_err_t api_led_toggle_handler(httpd_req_t *req)
{
    if (require_sensor_data(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (lock_state_or_503(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (g_sensor_data->led_state == 0) {
        g_sensor_data->led_state = 1;
        if (g_sensor_data->led_brightness == 0) {
            g_sensor_data->led_brightness = 255;
        }
    } else {
        g_sensor_data->led_state = 0;
        g_sensor_data->led_brightness = 0;
    }
    app_state_unlock();

    return send_ok(req);
}

static esp_err_t api_fan_toggle_handler(httpd_req_t *req)
{
    if (require_sensor_data(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (lock_state_or_503(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (g_sensor_data->fan_state == 0) {
        g_sensor_data->fan_state = 1;
        if (g_sensor_data->fan_speed == 0) {
            g_sensor_data->fan_speed = 255;
        }
    } else {
        g_sensor_data->fan_state = 0;
        g_sensor_data->fan_speed = 0;
    }
    app_state_unlock();

    return send_ok(req);
}

static esp_err_t api_curtain_toggle_handler(httpd_req_t *req)
{
    if (require_sensor_data(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (lock_state_or_503(req) != ESP_OK) {
        return ESP_FAIL;
    }

    g_sensor_data->curtain_state = !g_sensor_data->curtain_state;
    app_state_unlock();

    return send_ok(req);
}

static esp_err_t api_led_brightness_handler(httpd_req_t *req)
{
    if (require_sensor_data(req) != ESP_OK) {
        return ESP_FAIL;
    }

    int value = 0;
    if (!query_get_int(req, "value", &value)) {
        return send_json_status(req, "400 Bad Request", "missing value");
    }

    if (value < 0) value = 0;
    if (value > 255) value = 255;

    if (lock_state_or_503(req) != ESP_OK) {
        return ESP_FAIL;
    }

    g_sensor_data->led_brightness = (uint8_t)value;
    g_sensor_data->led_state = (value > 0) ? 1 : 0;
    app_state_unlock();

    return send_ok(req);
}

static esp_err_t api_fan_speed_handler(httpd_req_t *req)
{
    if (require_sensor_data(req) != ESP_OK) {
        return ESP_FAIL;
    }

    int value = 0;
    if (!query_get_int(req, "value", &value)) {
        return send_json_status(req, "400 Bad Request", "missing value");
    }

    if (value < 0) value = 0;
    if (value > 255) value = 255;

    if (lock_state_or_503(req) != ESP_OK) {
        return ESP_FAIL;
    }

    g_sensor_data->fan_speed = (uint8_t)value;
    g_sensor_data->fan_state = (g_sensor_data->fan_speed > 0) ? 1 : 0;
    app_state_unlock();

    return send_ok(req);
}

static esp_err_t api_mode_toggle_handler(httpd_req_t *req)
{
    if (require_sensor_data(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (lock_state_or_503(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (g_sensor_data->control_mode == CONTROL_MODE_AUTO) {
        app_control_set_mode(g_sensor_data, CONTROL_MODE_MANUAL);
    } else {
        app_control_set_mode(g_sensor_data, CONTROL_MODE_AUTO);
    }
    app_state_unlock();

    return send_ok(req);
}

static esp_err_t api_smoke_threshold_handler(httpd_req_t *req)
{
    if (require_sensor_data(req) != ESP_OK) {
        return ESP_FAIL;
    }

    char body[128];
    if (recv_request_body(req, body, sizeof(body)) != ESP_OK) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json_status(req, "400 Bad Request", "invalid json");
    }

    cJSON *threshold = cJSON_GetObjectItemCaseSensitive(root, "threshold");
    if (!cJSON_IsNumber(threshold)) {
        cJSON_Delete(root);
        return send_json_status(req, "400 Bad Request", "threshold must be a number");
    }

    int value = threshold->valueint;
    cJSON_Delete(root);

    if (value < 100 || value > 4095) {
        return send_json_status(req, "400 Bad Request", "threshold out of range");
    }

    if (lock_state_or_503(req) != ESP_OK) {
        return ESP_FAIL;
    }

    g_sensor_data->smoke_threshold = (uint32_t)value;
    app_state_unlock();

    ESP_LOGI(TAG, "Smoke threshold updated to %d", value);
    return send_ok(req);
}

static esp_err_t api_rgb_color_handler(httpd_req_t *req)
{
    int r = 0;
    int g = 255;
    int b = 0;

    query_get_int(req, "r", &r);
    query_get_int(req, "g", &g);
    query_get_int(req, "b", &b);

    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;

    rgb_led_set_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
    ESP_LOGI(TAG, "RGB set to: R=%d G=%d B=%d", r, g, b);

    return send_ok(req);
}

static esp_err_t api_rgb_preset_handler(httpd_req_t *req)
{
    char query[32];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char color_str[16];
        if (httpd_query_key_value(query, "c", color_str, sizeof(color_str)) == ESP_OK) {
            rgb_color_t color = RGB_COLOR_GREEN;
            if (strcmp(color_str, "red") == 0) color = RGB_COLOR_RED;
            else if (strcmp(color_str, "green") == 0) color = RGB_COLOR_GREEN;
            else if (strcmp(color_str, "blue") == 0) color = RGB_COLOR_BLUE;
            else if (strcmp(color_str, "yellow") == 0) color = RGB_COLOR_YELLOW;
            else if (strcmp(color_str, "cyan") == 0) color = RGB_COLOR_CYAN;
            else if (strcmp(color_str, "magenta") == 0) color = RGB_COLOR_MAGENTA;
            else if (strcmp(color_str, "white") == 0) color = RGB_COLOR_WHITE;
            else if (strcmp(color_str, "orange") == 0) color = RGB_COLOR_ORANGE;
            else if (strcmp(color_str, "purple") == 0) color = RGB_COLOR_PURPLE;

            rgb_led_set_color(color);
            ESP_LOGI(TAG, "RGB preset: %s", color_str);
        }
    }

    return send_ok(req);
}

httpd_handle_t http_server_start(sensor_data_t *sensor_data)
{
    g_sensor_data = sensor_data;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_uri_handlers = 16;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return NULL;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_data_uri = {
        .uri = "/api/data",
        .method = HTTP_GET,
        .handler = api_data_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_led_toggle_uri = {
        .uri = "/api/led/toggle",
        .method = HTTP_POST,
        .handler = api_led_toggle_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_fan_toggle_uri = {
        .uri = "/api/fan/toggle",
        .method = HTTP_POST,
        .handler = api_fan_toggle_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_fan_speed_uri = {
        .uri = "/api/fan/speed",
        .method = HTTP_POST,
        .handler = api_fan_speed_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_curtain_toggle_uri = {
        .uri = "/api/curtain/toggle",
        .method = HTTP_POST,
        .handler = api_curtain_toggle_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_led_brightness_uri = {
        .uri = "/api/led/brightness",
        .method = HTTP_POST,
        .handler = api_led_brightness_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_mode_toggle_uri = {
        .uri = "/api/mode/toggle",
        .method = HTTP_POST,
        .handler = api_mode_toggle_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_smoke_threshold_uri = {
        .uri = "/api/smoke/threshold",
        .method = HTTP_POST,
        .handler = api_smoke_threshold_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_rgb_color_uri = {
        .uri = "/api/rgb/color",
        .method = HTTP_POST,
        .handler = api_rgb_color_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t api_rgb_preset_uri = {
        .uri = "/api/rgb/preset",
        .method = HTTP_POST,
        .handler = api_rgb_preset_handler,
        .user_ctx = NULL,
    };

    const httpd_uri_t *uris[] = {
        &root_uri,
        &api_data_uri,
        &api_led_toggle_uri,
        &api_fan_toggle_uri,
        &api_fan_speed_uri,
        &api_curtain_toggle_uri,
        &api_led_brightness_uri,
        &api_mode_toggle_uri,
        &api_smoke_threshold_uri,
        &api_rgb_color_uri,
        &api_rgb_preset_uri,
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        if (register_uri_handler_checked(server, uris[i]) != ESP_OK) {
            httpd_stop(server);
            ESP_LOGE(TAG, "HTTP server aborted due to URI registration failure");
            return NULL;
        }
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    return server;
}

esp_err_t http_server_stop(httpd_handle_t server)
{
    if (server == NULL) {
        return ESP_OK;
    }
    return httpd_stop(server);
}
