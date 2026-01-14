#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

/**
 * @brief 控制模式枚举
 */
typedef enum {
    CONTROL_MODE_AUTO = 0,    // 自动模式 (默认) - 根据环境自动控制外设
    CONTROL_MODE_MANUAL       // 手动模式 - 仅响应用户指令，不自动控制
} control_mode_t;

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

    // 控制模式
    control_mode_t control_mode;  // 0=自动, 1=手动
} sensor_data_t;

#endif // APP_TYPES_H
