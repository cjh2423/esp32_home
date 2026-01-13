/**
 * @file dht_driver.c
 * @brief DHT11 温湿度传感器驱动封装 (使用 esp-idf-lib/dht 组件)
 */

#include "dht_driver.h"
#include <dht.h>
#include "esp_log.h"

static const char *TAG = "DHT11";

static gpio_num_t s_dht_gpio = GPIO_NUM_NC;

esp_err_t dht11_init(uint8_t gpio_num)
{
    s_dht_gpio = (gpio_num_t)gpio_num;

    // esp-idf-lib/dht 组件不需要显式初始化
    // 只需记录 GPIO 引脚，读取时会自动配置
    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d (using esp-idf-lib/dht)", gpio_num);
    return ESP_OK;
}

esp_err_t dht11_read(dht11_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_dht_gpio == GPIO_NUM_NC) {
        data->valid = 0;
        return ESP_ERR_INVALID_STATE;
    }

    // 使用 esp-idf-lib 的 dht_read_float_data 函数直接获取浮点值
    float temperature = 0;
    float humidity = 0;

    esp_err_t ret = dht_read_float_data(DHT_TYPE_DHT11, s_dht_gpio,
                                         &humidity, &temperature);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read DHT11: %s", esp_err_to_name(ret));
        data->valid = 0;
        return ret;
    }

    data->temperature = temperature;
    data->humidity = humidity;
    data->valid = 1;

    ESP_LOGD(TAG, "Temperature: %.1f C, Humidity: %.1f%%",
             data->temperature, data->humidity);

    return ESP_OK;
}

void dht11_deinit(void)
{
    s_dht_gpio = GPIO_NUM_NC;
    ESP_LOGI(TAG, "DHT11 deinitialized");
}
