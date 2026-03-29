#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi事件回调函数类型
 */
typedef void (*wifi_connected_callback_t)(void);
typedef void (*wifi_disconnected_callback_t)(void);

/**
 * @brief 启动WiFi
 *
 * 若 NVS 中已有凭据，直接进入 Station 模式连接；
 * 若无凭据，开启 AP 热点配网模式（SSID: ESP32-Home-XXXX），
 * 用户手机连接后访问 192.168.4.1 填写 SSID/密码，保存后自动切换到 Station 模式。
 *
 * 该函数阻塞直到 WiFi 连接成功。
 *
 * @param connected_cb    连接成功回调（可为 NULL）
 * @param disconnected_cb 断开连接回调（可为 NULL）
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t wifi_start(wifi_connected_callback_t connected_cb,
                     wifi_disconnected_callback_t disconnected_cb);

/**
 * @brief 获取WiFi连接状态
 * @return 1-已连接，0-未连接
 */
uint8_t wifi_is_connected(void);

/**
 * @brief 获取IP地址字符串
 * @param ip_str 输出缓冲区
 * @param len    缓冲区长度
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t wifi_get_ip_string(char *ip_str, size_t len);

#ifdef __cplusplus
}
#endif

#endif // WIFI_H
