#pragma once

#include <cstdint>
#include <esp_err.h>
#include <esp_log.h>
#include <lvgl.h>

class font_view
{
public:
    static bool get_glyph_dsc_handler(const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out, uint32_t unicode_letter, uint32_t unicode_letter_next)
    {
        return true;
    }

    static const uint8_t *get_glyph_bitmap_handler(const lv_font_t *font, uint32_t unicode_letter)
    {
        return nullptr;
    }

public:
    esp_err_t init(const uint8_t *buf, size_t len, uint8_t _height_px)
    {
        height_px = _height_px;
        lv_font.line_height = height_px;
        lv_font.get_glyph_dsc = get_glyph_dsc_handler;
        lv_font.get_glyph_bitmap = get_glyph_bitmap_handler;
        lv_font.user_data = this;
        lv_font.base_line = 0;

        if (stbtt_InitFont(&stb_font, buf, 0) < 1) {
            ESP_LOGE(TAG, "Load font failed");
            return ESP_FAIL;
        }

        font_buf = (uint8_t *)malloc(len * len);
        if (font_buf == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate font buffer");
            return ESP_ERR_NO_MEM;
        }

        memset(font_buf, 0, len * len);
        scale = stbtt_ScaleForPixelHeight(&stb_font, height_px);
        return ESP_OK;
    }

    esp_err_t decorate_font_style(lv_style_t *style)
    {
        if (style == nullptr) {
            return ESP_ERR_INVALID_ARG;
        }

        lv_style_set_text_font(style, &lv_font);
        return ESP_OK;
    }


private:
    uint8_t height_px = 0;
    uint8_t *font_buf = nullptr;
    float scale = 0;
    lv_font_t lv_font = {};
    stbtt_fontinfo stb_font = {};
    static const constexpr char *TAG = "font_view";
};