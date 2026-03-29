#include "wifi.h"

#include <string>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "wifi_manager.h"
#include "ssid_manager.h"

static const char *TAG = "WIFI";

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t s_event_group = NULL;
static uint8_t s_is_connected = 0;
static wifi_connected_callback_t s_connected_cb = NULL;
static wifi_disconnected_callback_t s_disconnected_cb = NULL;

esp_err_t wifi_start(wifi_connected_callback_t connected_cb,
                     wifi_disconnected_callback_t disconnected_cb)
{
    s_connected_cb = connected_cb;
    s_disconnected_cb = disconnected_cb;
    s_event_group = xEventGroupCreate();

    WifiManagerConfig config;
    config.ssid_prefix = "ESP32-Home";
    config.language = "zh-CN";
    config.station_scan_min_interval_seconds = 10;
    config.station_scan_max_interval_seconds = 300;

    auto& wifi = WifiManager::GetInstance();

    if (!wifi.Initialize(config)) {
        ESP_LOGE(TAG, "WifiManager init failed");
        return ESP_FAIL;
    }

    wifi.SetEventCallback([](WifiEvent event, const std::string& data) {
        switch (event) {
        case WifiEvent::Connected:
            ESP_LOGI(TAG, "WiFi connected, IP: %s", data.c_str());
            s_is_connected = 1;
            xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
            if (s_connected_cb) s_connected_cb();
            break;
        case WifiEvent::Disconnected:
            ESP_LOGW(TAG, "WiFi disconnected");
            s_is_connected = 0;
            if (s_disconnected_cb) s_disconnected_cb();
            break;
        case WifiEvent::ConfigModeEnter:
            ESP_LOGI(TAG, "AP config mode: SSID=%s URL=%s",
                     WifiManager::GetInstance().GetApSsid().c_str(),
                     WifiManager::GetInstance().GetApWebUrl().c_str());
            break;
        default:
            break;
        }
    });

    /* 检查 NVS 是否有凭据 */
    const auto& ssid_list = SsidManager::GetInstance().GetSsidList();
    if (!ssid_list.empty()) {
        ESP_LOGI(TAG, "Found %d saved credential(s), starting station", (int)ssid_list.size());
        wifi.StartStation();
    } else {
        ESP_LOGI(TAG, "No WiFi credentials, starting AP config mode");
        wifi.StartConfigAp();
    }

    /* 阻塞等待连接成功 */
    xEventGroupWaitBits(s_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);
    return ESP_OK;
}

uint8_t wifi_is_connected(void)
{
    return s_is_connected;
}

esp_err_t wifi_get_ip_string(char *ip_str, size_t len)
{
    if (ip_str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const std::string ip = WifiManager::GetInstance().GetIpAddress();
    if (ip.empty()) {
        return ESP_FAIL;
    }
    snprintf(ip_str, len, "%s", ip.c_str());
    return ESP_OK;
}

