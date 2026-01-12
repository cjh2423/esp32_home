#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

/**
 * @brief 系统全局传感器与状态数据结构
 */
typedef struct {
    // 传感器数据
    float temperature;
    float humidity;
    float light;
    uint32_t smoke;

    // 设备状态
    uint8_t led_state;
    uint8_t led_brightness;
    uint8_t fan_state;
    uint8_t fan_speed;
    uint8_t curtain_state;
} sensor_data_t;

#endif // APP_TYPES_H
