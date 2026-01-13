/**
 * @file bh1750_driver.c
 * @brief BH1750 光照传感器驱动封装 (使用 esp-idf-lib/bh1750 组件)
 */

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// esp-idf-lib 头文件
#include <i2cdev.h>
#include <bh1750.h>

// 我们的头文件
#include "bh1750_driver.h"

static const char *TAG = "BH1750";

// esp-idf-lib 使用 i2cdev 库管理 I2C
static i2c_dev_t s_dev = {0};
static bool s_initialized = false;

esp_err_t bh1750_sensor_init(uint8_t sda_gpio, uint8_t scl_gpio)
{
    // 初始化 i2cdev 库 (全局只需调用一次)
    esp_err_t ret = i2cdev_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE 表示已经初始化过
        ESP_LOGE(TAG, "i2cdev init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始化 BH1750 设备描述符
    memset(&s_dev, 0, sizeof(i2c_dev_t));
    ret = bh1750_init_desc(&s_dev, BH1750_ADDR_LO, I2C_NUM_0,
                           (gpio_num_t)sda_gpio, (gpio_num_t)scl_gpio);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 init_desc failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 上电
    ret = bh1750_power_on(&s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 power on failed: %s", esp_err_to_name(ret));
        bh1750_free_desc(&s_dev);
        return ret;
    }

    // 设置连续高分辨率测量模式
    ret = bh1750_setup(&s_dev, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 setup failed: %s", esp_err_to_name(ret));
        bh1750_free_desc(&s_dev);
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "BH1750 initialized (SDA:%d, SCL:%d) using esp-idf-lib/bh1750",
             sda_gpio, scl_gpio);
    return ESP_OK;
}

esp_err_t bh1750_sensor_read(float *lux)
{
    if (lux == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t level = 0;
    // 调用 esp-idf-lib 的 bh1750_read 函数
    esp_err_t ret = bh1750_read(&s_dev, &level);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read from BH1750: %s", esp_err_to_name(ret));
        return ret;
    }

    *lux = (float)level;
    ESP_LOGD(TAG, "Light intensity: %.1f lux", *lux);
    return ESP_OK;
}

void bh1750_sensor_deinit(void)
{
    if (s_initialized) {
        bh1750_power_down(&s_dev);
        bh1750_free_desc(&s_dev);
        s_initialized = false;
        ESP_LOGI(TAG, "BH1750 deinitialized");
    }
}
