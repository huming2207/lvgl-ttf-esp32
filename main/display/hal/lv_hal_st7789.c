#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>

#include <lvgl.h>
#include "lv_hal_st7789.h"

#define LOG_TAG "st7789"

static spi_device_handle_t device_handle;
static volatile bool spi_trans_in_progress;

st7789_seq_t st7789_init_seq[] = {
        {0x36, {0x00}, 1},
        {0x3A, {0x05}, 1},
        {0xB2, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 5},
        {0xB7, {0x35}, 1},
        {0xBB, {0x19}, 1},
        {0xC0, {0x2C}, 1},
        {0xC2, {0x01}, 1},
        {0xC3, {0x12}, 1},
        {0xC4, {0x20}, 1},
        {0xC6, {0x0F}, 1},
        {0xD0, {0xA4, 0xA1}, 2}
};

// Fill to RE0h
uint8_t pos_gamma_val[] = {
        0xD0, 0x04, 0x0D,
        0x11, 0x13, 0x2B,
        0x3F, 0x54, 0x4C,
        0x18, 0x0D, 0x0B,
        0x1F, 0x23
};

// Fill to RE1h
uint8_t ngt_gamma_val[] = {
        0xD0, 0x04, 0x0C,
        0x11, 0x13, 0x2C,
        0x3F, 0x44, 0x51,
        0x2F, 0x1F, 0x1F,
        0x20, 0x23,
};

static void st7789_spi_send_byte(const uint8_t *payload, size_t len, bool is_cmd)
{
    if(!payload) {
        ESP_LOGE(LOG_TAG, "Payload is null!");
        return;
    }

    spi_transaction_t spi_tract;
    memset(&spi_tract, 0, sizeof(spi_tract));

    spi_tract.tx_buffer = payload;
    spi_tract.length = len * 8;
    spi_tract.rxlength = 0;

    // ESP_LOGD(LOG_TAG, "Sending SPI payload, length : %d, is_cmd: %s", len, is_cmd ? "TRUE" : "FALSE");
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_LCD_IO_DC, is_cmd ? ST7789_CMD : ST7789_DAT));
    ESP_ERROR_CHECK(spi_device_polling_transmit(device_handle, &spi_tract)); // Use blocking transmit for now (easier to debug)
    // ESP_LOGD(LOG_TAG, "SPI payload sent!");
}

static void st7789_spi_send_pixel(const uint16_t *payload, size_t len)
{
    if(!payload) {
        ESP_LOGE(LOG_TAG, "Payload is null!");
        return;
    }

    spi_transaction_t spi_tract;
    memset(&spi_tract, 0, sizeof(spi_tract));

    spi_tract.tx_buffer = payload;
    spi_tract.length = len * 8;
    spi_tract.rxlength = 0;

    // ESP_LOGD(LOG_TAG, "Sending SPI payload, length : %d, is_cmd: %s", len, is_cmd ? "TRUE" : "FALSE");
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_LCD_IO_DC, ST7789_DAT));
    ESP_ERROR_CHECK(spi_device_polling_transmit(device_handle, &spi_tract)); // Use blocking transmit for now (easier to debug)
    // ESP_LOGD(LOG_TAG, "SPI payload sent!");
}

static void st7789_send_seq(st7789_seq_t *seq)
{
    if(!seq) {
        ESP_LOGE(LOG_TAG, "Sequence is null!");
        return;
    }

    ESP_LOGD(LOG_TAG, "Writing to register 0x%02X...", seq->reg);
    st7789_spi_send_byte(&seq->reg, 1, true);

    if(seq->len > 0) {
        st7789_spi_send_byte(seq->data, seq->len, false);
        ESP_LOGD(LOG_TAG, "Seq data: 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x; len: %u",
                 seq->data[0], seq->data[1], seq->data[2], seq->data[3], seq->data[4], seq->len);
    }
}

static void st7789_set_pos(const uint16_t x1, const uint16_t x2, const uint16_t y1, const uint16_t y2)
{
    const uint8_t x_pos_cmd = 0x2A, y_pos_cmd = 0x2B;
    const uint8_t x_start[] = {(const uint8_t)(x1 >> 8u), (const uint8_t)(x1 & 0xffU)};
    const uint8_t x_end[] = {(const uint8_t)(x2 >> 8u), (const uint8_t)(x2 & 0xffU)};
    const uint8_t y_start[] = {(const uint8_t)(y1 >> 8u), (const uint8_t)(y1 & 0xffU)};
    const uint8_t y_end[] = {(const uint8_t)(y2 >> 8u), (const uint8_t)(y2 & 0xffU)};

    ESP_LOGD(LOG_TAG, "Setting position in: "
                      "x1: 0x%02X, x2: 0x%02X; y1: 0x%02X, y2: 0x%02X", x1, x2, y1, y2);

    st7789_spi_send_byte(&x_pos_cmd, 1, true);
    st7789_spi_send_byte(x_start, 2, false);
    st7789_spi_send_byte(x_end, 2, false);

    st7789_spi_send_byte(&y_pos_cmd, 1, true);
    st7789_spi_send_byte(y_start, 2, false);
    st7789_spi_send_byte(y_end, 2, false);

    ESP_LOGD(LOG_TAG, "Position set!");
}

static void st7789_prep_write_fb()
{
    const uint8_t write_fb_reg = 0x2C;
    st7789_spi_send_byte(&write_fb_reg, 1, true); // Tell the panel it's about to write something on the screen
}

static void st7789_write_fb(const uint16_t val)
{
    // Split uint16 to two uint8 to save some "context switching" times
    const uint8_t val_buf[2] = {(const uint8_t)(val >> 8u), (const uint8_t)(val & 0xffU)};
    st7789_spi_send_byte(val_buf, 2, false);
}

void lv_st7789_fill(int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t color)
{
    st7789_set_pos(x1, x2, y1, y2);
    st7789_prep_write_fb();

    ESP_LOGD(LOG_TAG, "Sending framebuffer with RGB value: 0x%X", color.full);
    for(uint32_t col = 0; col < (y2 - y1); col += 1) {
        for(uint32_t row = 0; row < (x2 - x1); row += 1) {
            st7789_write_fb(color.full);
        }
    }
    ESP_LOGD(LOG_TAG, "Framebuffer filled!");
}

void lv_st7789_flush(lv_disp_drv_t *disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    st7789_set_pos(area->x1, area->x2, area->y1, area->y2);
    st7789_prep_write_fb();

    uint32_t line_size = area->x2 - area->x1 + 1;

    for(uint32_t curr_y = area->y1; curr_y <= area->y2; curr_y++) {
        st7789_spi_send_pixel(&color_p->full, line_size * 2);
        color_p += line_size;
    }

    lv_disp_flush_ready(disp_drv);
}

void lv_st7789_init()
{

    ESP_LOGI(LOG_TAG, "Performing GPIO init...");
    ESP_ERROR_CHECK(gpio_set_direction(CONFIG_LCD_IO_DC, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_direction(CONFIG_LCD_IO_RST, GPIO_MODE_OUTPUT));
    ESP_LOGI(LOG_TAG, "GPIO initialization finished, resetting OLED...");

    ESP_ERROR_CHECK(gpio_set_level(CONFIG_LCD_IO_RST, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_LCD_IO_RST, 1));
    vTaskDelay(pdMS_TO_TICKS(100));

    spi_bus_config_t bus_config = {
            .mosi_io_num = CONFIG_LCD_SPI_MOSI,
            .sclk_io_num = CONFIG_LCD_SPI_SCLK,
            .miso_io_num = -1, // We don't need to get any bullshit from the panel, so no MISO needed.
            .quadhd_io_num = -1,
            .quadwp_io_num = -1,
            .max_transfer_sz = 240 * 240 * 3
    };

    spi_device_interface_config_t device_config = {
#ifndef CONFIG_LCD_SPI_CLK_DEBUG
            .clock_speed_hz = SPI_MASTER_FREQ_40M,
#else
            .clock_speed_hz = SPI_MASTER_FREQ_8M,
#endif
            .mode = 0, // CPOL = 0, CPHA = 0???
            .spics_io_num = CONFIG_LCD_SPI_CS,
            .queue_size = 7
    };

    ESP_LOGI(LOG_TAG, "Performing SPI init...");
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &bus_config, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &device_config, &device_handle));
    ESP_LOGI(LOG_TAG, "SPI initialization finished, sending init sequence to IPS panel...");

    // Send init sequence
    for(uint32_t curr_pos = 0; curr_pos < sizeof(st7789_init_seq)/sizeof(st7789_init_seq[0]); curr_pos += 1) {
        st7789_send_seq(&st7789_init_seq[curr_pos]);
    }

    // Throw in positive gamma to RE0h
    const uint8_t pos_gamma_reg = 0xE0;
    st7789_spi_send_byte(&pos_gamma_reg, 1, true);
    st7789_spi_send_byte(pos_gamma_val, sizeof(pos_gamma_val), false);

    // Throw in negative gamma to RE1h
    const uint8_t ngt_gamma_reg = 0xE1;
    st7789_spi_send_byte(&ngt_gamma_reg, 1, true);
    st7789_spi_send_byte(ngt_gamma_val, sizeof(ngt_gamma_val), false);

    // Display inversion on
    const uint8_t disp_inv_on_reg = 0x21;
    st7789_spi_send_byte(&disp_inv_on_reg, 1, true);

    // Disable sleep
    const uint8_t sleep_disable_reg = 0x11;
    st7789_spi_send_byte(&sleep_disable_reg, 1, true);

    // Turn on the panel
    const uint8_t disp_on_reg = 0x29;
    st7789_spi_send_byte(&disp_on_reg, 1, true);
}
