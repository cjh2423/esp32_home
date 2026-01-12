#include "dht11.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static const char *TAG = "DHT11";
static uint8_t dht11_gpio = 0;

// 微秒级延时
#define delay_us(us) ets_delay_us(us)

esp_err_t dht11_init(uint8_t gpio_num)
{
    dht11_gpio = gpio_num;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dht11_gpio),
        .mode = GPIO_MODE_INPUT_OUTPUT,  // 输入输出模式
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed");
        return ret;
    }
    
    gpio_set_level(dht11_gpio, 1);
    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d", dht11_gpio);
    return ESP_OK;
}

static esp_err_t dht11_wait_for_state(uint8_t state, uint32_t timeout_us)
{
    uint32_t count = 0;
    while (gpio_get_level(dht11_gpio) != state) {
        delay_us(1);
        if (++count > timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_OK;
}

esp_err_t dht11_read(dht11_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t raw_data[5] = {0};
    
    // 发送起始信号
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(20));  // 拉低至少18ms
    
    gpio_set_level(dht11_gpio, 1);
    delay_us(30);
    gpio_set_direction(dht11_gpio, GPIO_MODE_INPUT);
    
    // 等待DHT11响应
    if (dht11_wait_for_state(0, 100) != ESP_OK) {
        ESP_LOGW(TAG, "No response from DHT11");
        data->valid = 0;
        return ESP_ERR_TIMEOUT;
    }
    
    if (dht11_wait_for_state(1, 100) != ESP_OK) {
        data->valid = 0;
        return ESP_ERR_TIMEOUT;
    }
    
    if (dht11_wait_for_state(0, 100) != ESP_OK) {
        data->valid = 0;
        return ESP_ERR_TIMEOUT;
    }
    
    // 读取40位数据
    for (int i = 0; i < 40; i++) {
        // 等待数据位开始
        if (dht11_wait_for_state(1, 100) != ESP_OK) {
            data->valid = 0;
            return ESP_ERR_TIMEOUT;
        }
        
        delay_us(30);
        
        // 如果此时还是高电平，则为1，否则为0
        if (gpio_get_level(dht11_gpio)) {
            raw_data[i / 8] |= (1 << (7 - (i % 8)));
        }
        
        // 等待数据位结束
        if (dht11_wait_for_state(0, 100) != ESP_OK) {
            data->valid = 0;
            return ESP_ERR_TIMEOUT;
        }
    }
    
    // 校验和验证
    uint8_t checksum = raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3];
    if (checksum != raw_data[4]) {
        ESP_LOGW(TAG, "Checksum error: %d != %d", checksum, raw_data[4]);
        data->valid = 0;
        return ESP_ERR_INVALID_CRC;
    }
    
    // 解析数据
    data->humidity = (float)raw_data[0] + (float)raw_data[1] / 10.0f;
    data->temperature = (float)raw_data[2] + (float)raw_data[3] / 10.0f;
    data->valid = 1;
    
    // ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", data->temperature, data->humidity);
    
    return ESP_OK;
}
