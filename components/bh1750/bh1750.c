#include "bh1750.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BH1750";

// 新版 I2C Master API 句柄
static i2c_master_bus_handle_t s_bus_handle = NULL;
static i2c_master_dev_handle_t s_dev_handle = NULL;

esp_err_t bh1750_init(uint8_t sda_gpio, uint8_t scl_gpio)
{
    // 配置 I2C 总线
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &s_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // 添加 BH1750 设备
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BH1750_ADDR,
        .scl_speed_hz = 100000,  // 100kHz
    };

    ret = i2c_master_bus_add_device(s_bus_handle, &dev_config, &s_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add BH1750 device: %s", esp_err_to_name(ret));
        i2c_del_master_bus(s_bus_handle);
        s_bus_handle = NULL;
        return ret;
    }

    // 上电命令
    uint8_t cmd = BH1750_POWER_ON;
    ret = i2c_master_transmit(s_dev_handle, &cmd, 1, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 power on failed: %s", esp_err_to_name(ret));
        bh1750_deinit();
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置连续高分辨率模式
    cmd = BH1750_CONTINUOUS_HIGH_RES_MODE;
    ret = i2c_master_transmit(s_dev_handle, &cmd, 1, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BH1750 mode set failed: %s", esp_err_to_name(ret));
        bh1750_deinit();
        return ret;
    }

    ESP_LOGI(TAG, "BH1750 initialized (SDA:%d, SCL:%d)", sda_gpio, scl_gpio);
    return ESP_OK;
}

esp_err_t bh1750_read(float *lux)
{
    if (lux == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_dev_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[2];
    esp_err_t ret = i2c_master_receive(s_dev_handle, data, 2, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read from BH1750: %s", esp_err_to_name(ret));
        return ret;
    }

    uint16_t raw_value = (data[0] << 8) | data[1];
    *lux = raw_value / 1.2f;  // 转换为lux

    ESP_LOGD(TAG, "Light intensity: %.1f lux", *lux);
    return ESP_OK;
}

void bh1750_deinit(void)
{
    if (s_dev_handle != NULL) {
        i2c_master_bus_rm_device(s_dev_handle);
        s_dev_handle = NULL;
    }

    if (s_bus_handle != NULL) {
        i2c_del_master_bus(s_bus_handle);
        s_bus_handle = NULL;
    }

    ESP_LOGI(TAG, "BH1750 deinitialized");
}
