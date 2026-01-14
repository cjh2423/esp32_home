#include "http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "app_state.h"
#include "app_control.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "HTTP_SERVER";
static sensor_data_t *g_sensor_data = NULL;

// 引用嵌入的HTML文件
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

// 主页处理
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    // 计算HTML长度
    size_t html_len = index_html_end - index_html_start;
    httpd_resp_send(req, (const char *)index_html_start, html_len);
    return ESP_OK;
}

// API数据处理
static esp_err_t api_data_handler(httpd_req_t *req)
{
    if (g_sensor_data == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    sensor_data_t snapshot;
    app_state_lock();
    snapshot = *g_sensor_data;
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
    cJSON_AddNumberToObject(root, "led_state", snapshot.led_state);
    cJSON_AddNumberToObject(root, "led_brightness", snapshot.led_brightness);
    cJSON_AddNumberToObject(root, "fan_state", snapshot.fan_state);
    cJSON_AddNumberToObject(root, "fan_speed", snapshot.fan_speed);
    cJSON_AddNumberToObject(root, "curtain_state", snapshot.curtain_state);
    cJSON_AddNumberToObject(root, "control_mode", snapshot.control_mode);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t resp_ret = httpd_resp_send(req, json_str, strlen(json_str));

    free(json_str);
    cJSON_Delete(root);
    return resp_ret;
}

// LED控制处理（这些只是示例，实际控制需要在主程序中实现）
static esp_err_t api_led_toggle_handler(httpd_req_t *req)
{
    if (g_sensor_data != NULL) {
        app_state_lock();
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
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t api_fan_toggle_handler(httpd_req_t *req)
{
    if (g_sensor_data != NULL) {
        app_state_lock();
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
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}



static esp_err_t api_curtain_toggle_handler(httpd_req_t *req)
{
    if (g_sensor_data != NULL) {
        app_state_lock();
        g_sensor_data->curtain_state = !g_sensor_data->curtain_state;
        app_state_unlock();
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t api_led_brightness_handler(httpd_req_t *req)
{
    char buf[32];
    int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
    if (ret == ESP_OK) {
        char param[32];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            if (g_sensor_data != NULL) {
                int value = atoi(param);
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                app_state_lock();
                g_sensor_data->led_brightness = (uint8_t)value;
                g_sensor_data->led_state = (value > 0) ? 1 : 0;
                app_state_unlock();
            }
        }
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t api_fan_speed_handler(httpd_req_t *req)
{
    char buf[32];
    int ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
    if (ret == ESP_OK) {
        char param[32];
        if (httpd_query_key_value(buf, "value", param, sizeof(param)) == ESP_OK) {
            if (g_sensor_data != NULL) {
                int value = atoi(param);
                if (value < 0) value = 0;
                if (value > 255) value = 255;
                app_state_lock();
                g_sensor_data->fan_speed = (uint8_t)value;
                // 联动状态
                g_sensor_data->fan_state = (g_sensor_data->fan_speed > 0) ? 1 : 0;
                app_state_unlock();
            }
        }
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// 模式切换处理
static esp_err_t api_mode_toggle_handler(httpd_req_t *req)
{
    if (g_sensor_data != NULL) {
        if (app_state_lock() != ESP_OK) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (g_sensor_data->control_mode == CONTROL_MODE_AUTO) {
            app_control_set_mode(g_sensor_data, CONTROL_MODE_MANUAL);
        } else {
            app_control_set_mode(g_sensor_data, CONTROL_MODE_AUTO);
        }
        app_state_unlock();
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

httpd_handle_t http_server_start(sensor_data_t *sensor_data)
{
    g_sensor_data = sensor_data;
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_uri_handlers = 12; // 增加handler数量限制
    
    httpd_handle_t server = NULL;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        
        httpd_uri_t api_data_uri = {
            .uri = "/api/data",
            .method = HTTP_GET,
            .handler = api_data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_data_uri);
        
        httpd_uri_t api_led_toggle_uri = {
            .uri = "/api/led/toggle",
            .method = HTTP_GET,
            .handler = api_led_toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_led_toggle_uri);
        
        httpd_uri_t api_fan_toggle_uri = {
            .uri = "/api/fan/toggle",
            .method = HTTP_GET,
            .handler = api_fan_toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_fan_toggle_uri);
        
        httpd_uri_t api_fan_speed_uri = {
            .uri = "/api/fan/speed",
            .method = HTTP_GET,
            .handler = api_fan_speed_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_fan_speed_uri);
        


        httpd_uri_t api_curtain_toggle_uri = {
            .uri = "/api/curtain/toggle",
            .method = HTTP_GET,
            .handler = api_curtain_toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_curtain_toggle_uri);
        
        httpd_uri_t api_led_brightness_uri = {
            .uri = "/api/led/brightness",
            .method = HTTP_GET,
            .handler = api_led_brightness_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_led_brightness_uri);

        httpd_uri_t api_mode_toggle_uri = {
            .uri = "/api/mode/toggle",
            .method = HTTP_GET,
            .handler = api_mode_toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &api_mode_toggle_uri);

        ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
        return server;
    }
    
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
}

esp_err_t http_server_stop(httpd_handle_t server)
{
    return httpd_stop(server);
}
