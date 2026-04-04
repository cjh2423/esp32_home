#include "wifi.h"

#include <string>
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "wifi_manager.h"
#include "ssid_manager.h"

static const char *TAG = "WIFI";

#define WIFI_CONNECTED_BIT BIT0
#define STA_FALLBACK_TIMEOUT_US (30LL * 1000LL * 1000LL)

static EventGroupHandle_t s_event_group = NULL;
static uint8_t s_is_connected = 0;
static wifi_connected_callback_t s_connected_cb = NULL;
static wifi_disconnected_callback_t s_disconnected_cb = NULL;
static esp_timer_handle_t s_sta_fallback_timer = NULL;
static TaskHandle_t s_boot_button_task = NULL;
static bool s_boot_button_gpio_initialized = false;
static bool s_boot_button_isr_service_ready = false;

static void init_boot_button_gpio(void)
{
    if (s_boot_button_gpio_initialized) {
        return;
    }

    const gpio_num_t boot_gpio = (gpio_num_t)WIFI_CONFIG_BOOT_BUTTON_GPIO;
    gpio_reset_pin(boot_gpio);
    gpio_set_direction(boot_gpio, GPIO_MODE_INPUT);
    gpio_pullup_en(boot_gpio);
    gpio_pulldown_dis(boot_gpio);
    gpio_set_intr_type(boot_gpio, GPIO_INTR_NEGEDGE);
    s_boot_button_gpio_initialized = true;
}

static bool is_boot_button_pressed(void)
{
    init_boot_button_gpio();
    const gpio_num_t boot_gpio = (gpio_num_t)WIFI_CONFIG_BOOT_BUTTON_GPIO;
    return gpio_get_level(boot_gpio) == 0;
}

static void stop_sta_fallback_timer(void)
{
    if (s_sta_fallback_timer != NULL) {
        esp_timer_stop(s_sta_fallback_timer);
    }
}

static void sta_fallback_timer_callback(void *arg)
{
    (void)arg;
    auto &wifi = WifiManager::GetInstance();
    if (wifi.IsConnected() || wifi.IsConfigMode()) {
        return;
    }

    ESP_LOGW(TAG, "Station did not connect within 30 seconds, switching to AP config mode");
    wifi.StartConfigAp();
}

static void ensure_sta_fallback_timer_created(void)
{
    if (s_sta_fallback_timer != NULL) {
        return;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &sta_fallback_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifi_sta_fallback",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_sta_fallback_timer));
}

static void start_station_with_fallback(void)
{
    ensure_sta_fallback_timer_created();
    stop_sta_fallback_timer();
    ESP_ERROR_CHECK(esp_timer_start_once(s_sta_fallback_timer, STA_FALLBACK_TIMEOUT_US));
    WifiManager::GetInstance().StartStation();
}

static void IRAM_ATTR boot_button_isr_handler(void *arg)
{
    (void)arg;
    BaseType_t high_task_wakeup = pdFALSE;
    if (s_boot_button_task != NULL) {
        vTaskNotifyGiveFromISR(s_boot_button_task, &high_task_wakeup);
    }
    if (high_task_wakeup == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void boot_button_monitor_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(80));

        if (is_boot_button_pressed()) {
            auto &wifi = WifiManager::GetInstance();
            if (!wifi.IsConfigMode()) {
                ESP_LOGI(TAG, "BOOT button pressed, switching to AP config mode");
                wifi.StartConfigAp();
            }
            while (is_boot_button_pressed()) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
        }
    }
}

static void ensure_boot_button_monitor_started(void)
{
    if (s_boot_button_task != NULL) {
        return;
    }

    init_boot_button_gpio();
    BaseType_t ret = xTaskCreate(boot_button_monitor_task,
                                 "boot_btn_mon",
                                 3072,
                                 NULL,
                                 tskIDLE_PRIORITY + 1,
                                 &s_boot_button_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create BOOT button monitor task");
        s_boot_button_task = NULL;
        return;
    }

    if (!s_boot_button_isr_service_ready) {
        esp_err_t isr_ret = gpio_install_isr_service(0);
        if (isr_ret == ESP_OK || isr_ret == ESP_ERR_INVALID_STATE) {
            s_boot_button_isr_service_ready = true;
        } else {
            ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(isr_ret));
            return;
        }
    }

    esp_err_t handler_ret = gpio_isr_handler_add((gpio_num_t)WIFI_CONFIG_BOOT_BUTTON_GPIO,
                                                 boot_button_isr_handler,
                                                 NULL);
    if (handler_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add BOOT button ISR handler: %s", esp_err_to_name(handler_ret));
    }
}

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
            stop_sta_fallback_timer();
            xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
            if (s_connected_cb) s_connected_cb();
            break;
        case WifiEvent::Disconnected:
            ESP_LOGW(TAG, "WiFi disconnected");
            s_is_connected = 0;
            if (!WifiManager::GetInstance().IsConfigMode()) {
                ensure_sta_fallback_timer_created();
                stop_sta_fallback_timer();
                ESP_ERROR_CHECK(esp_timer_start_once(s_sta_fallback_timer, STA_FALLBACK_TIMEOUT_US));
            }
            if (s_disconnected_cb) s_disconnected_cb();
            break;
        case WifiEvent::ConfigModeEnter:
            stop_sta_fallback_timer();
            ESP_LOGI(TAG, "AP config mode: SSID=%s URL=%s",
                     WifiManager::GetInstance().GetApSsid().c_str(),
                     WifiManager::GetInstance().GetApWebUrl().c_str());
            break;
        case WifiEvent::ConfigModeExit:
            /* 配网完成，切换到 Station 模式连接 */
            ESP_LOGI(TAG, "Config mode exited, starting station");
            start_station_with_fallback();
            break;
        default:
            break;
        }
    });

    ensure_boot_button_monitor_started();

    /* 检查 NVS 是否有凭据 */
    const auto& ssid_list = SsidManager::GetInstance().GetSsidList();
    if (!ssid_list.empty()) {
        ESP_LOGI(TAG, "Found %d saved credential(s), starting station", (int)ssid_list.size());
        start_station_with_fallback();
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
