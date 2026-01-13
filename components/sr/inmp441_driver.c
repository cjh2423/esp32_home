#include "inmp441_driver.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static const char *TAG = "INMP441";

// I2S 配置参数
#define I2S_SAMPLE_RATE     16000
#define I2S_BITS_PER_SAMPLE 32
#define I2S_CHANNELS        1
#define I2S_DMA_BUF_COUNT   4
#define I2S_DMA_BUF_LEN     512

static i2s_chan_handle_t rx_handle = NULL;

esp_err_t inmp441_init(int sck_io, int ws_io, int sd_io)
{
    if (rx_handle != NULL) {
        ESP_LOGW(TAG, "INMP441 already initialized");
        return ESP_OK;
    }

    // 创建 I2S RX 通道
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = I2S_DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = I2S_DMA_BUF_LEN;
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置 I2S 标准模式（INMP441 使用 MSB 对齐，左声道）
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = 32,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = sck_io,
            .ws = ws_io,
            .dout = I2S_GPIO_UNUSED,
            .din = sd_io,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(rx_handle);
        rx_handle = NULL;
        return ret;
    }

    // 启动 I2S 通道
    ret = i2s_channel_enable(rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        i2s_del_channel(rx_handle);
        rx_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "INMP441 initialized (SCK:%d, WS:%d, SD:%d)", sck_io, ws_io, sd_io);
    return ESP_OK;
}

esp_err_t inmp441_read(void *buffer, size_t buffer_size, size_t *bytes_read, uint32_t timeout_ms)
{
    if (rx_handle == NULL) {
        ESP_LOGE(TAG, "INMP441 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    return i2s_channel_read(rx_handle, buffer, buffer_size, bytes_read, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t inmp441_deinit(void)
{
    if (rx_handle == NULL) {
        return ESP_OK;
    }

    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
    rx_handle = NULL;
    
    ESP_LOGI(TAG, "INMP441 deinitialized");
    return ESP_OK;
}
