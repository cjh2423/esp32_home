#ifndef CONFIG_H
#define CONFIG_H

// ==================== WiFi 配置 ====================
// 输入您的 WiFi 名称和密码
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_SSID"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "YOUR_PASSWORD"
#endif
#define WIFI_MAXIMUM_RETRY 5

// ==================== GPIO 引脚配置 (ESP32-S3) ====================

// 1. DHT11 温湿度传感器
#define DHT11_GPIO 4

// 2. LED 灯光控制 (PWM - 高电平有效)
#define LED_GPIO 5
#define LED_PWM_CHANNEL 0
#define LED_PWM_FREQ 5000
#define LED_PWM_RESOLUTION 8

// 3. 蜂鸣器 (低电平触发 Active Low)
#define BUZZER_GPIO 6

// 4. MQ-2 烟雾传感器 (ADC)
// ESP32-S3: GPIO 7 对应 ADC1_CHANNEL_6
#define MQ2_ADC_CHANNEL ADC1_CHANNEL_6  // GPIO 7

// 5. 窗帘舵机控制 (PWM)
#define SERVO_GPIO 8
#define SERVO_PWM_CHANNEL 2
#define SERVO_PWM_FREQ 50      // 舵机通常频率 50Hz
#define SERVO_PWM_TIMER LEDC_TIMER_1 // 使用不同的 Timer 或与 LED 共用(如果频率一致)
                                     // LED 是 5000Hz, 舵机是 50Hz，必须用不同 Timer
                                     // LEDC_TIMER_0 被 LED 占用
                                     // LEDC_TIMER_1 给舵机

// 6. BH1750 光照传感器 (I2C)
#define BH1750_SDA_GPIO 15
#define BH1750_SCL_GPIO 16

// 7. 加湿器已移除
// #define RELAY_HUMIDIFIER_GPIO 17 

// 8. 风扇控制 (PWM - 低电平触发)
#define FAN_GPIO 18
#define FAN_PWM_CHANNEL 1
#define FAN_PWM_FREQ 25000
#define FAN_PWM_RESOLUTION 8

// ==================== 传感器阈值配置 ====================
// 温度阈值
#define TEMP_HIGH_THRESHOLD 30.0f   // 高温阈值（℃）
#define TEMP_LOW_THRESHOLD 15.0f    // 低温阈值（℃）

// 湿度阈值
#define HUMIDITY_HIGH_THRESHOLD 80.0f  // 高湿度阈值（%）
#define HUMIDITY_LOW_THRESHOLD 30.0f   // 低湿度阈值（%）

// 光照阈值
#define LIGHT_LOW_THRESHOLD 100.0f  // 低光照阈值 (lux)

// 烟雾阈值 (ADC原始值 0-4095)
#define SMOKE_THRESHOLD 1000

// ==================== 系统配置 ====================
// 传感器读取间隔（毫秒）
#define SENSOR_READ_INTERVAL 2000

// HTTP 服务器端口
#define HTTP_SERVER_PORT 80

// 自动化功能开关 (1=开启, 0=关闭)
#define AUTO_LIGHT_ENABLE 1      // 自动灯光
#define AUTO_FAN_ENABLE 1        // 自动风扇
// #define AUTO_HUMIDIFIER_ENABLE 1 // 自动加湿器 (已移除)

#endif // CONFIG_H
