#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <lvgl.h>
#include <lv_hal_st7789.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#define TAG "main"

SemaphoreHandle_t xGuiSemaphore;
static stbtt_fontinfo stb_font = {};

extern const uint8_t wqy_font_start[]   asm("_binary_wqy_ttf_start");
extern const uint8_t wqy_font_end[]     asm("_binary_wqy_ttf_end");

#define LV_TICK_PERIOD_MS 1

static void IRAM_ATTR lv_tick_cb(void *arg)
{
    (void) arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void start_disp()
{
    ESP_LOGI(TAG, "Initialising LittlevGL");
    lv_init();

    ESP_LOGI(TAG, "Initialising ST7789");
    lv_st7789_init();

    lv_color_t* buf1 = heap_caps_malloc(ST7789_DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);

    lv_color_t* buf2 = heap_caps_malloc(ST7789_DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);

    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, ST7789_DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = lv_st7789_flush;
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.antialiasing = 1;
    lv_disp_drv_register(&disp_drv);

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &lv_tick_cb,
            .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));
    ESP_LOGI(TAG, "Display init done");
}

static bool my_get_glyph_dsc_cb(const lv_font_t *font, lv_font_glyph_dsc_t *dsc_out, uint32_t unicode_letter, uint32_t unicode_letter_next)
{
    if (dsc_out == NULL) return false;
    float scale = stbtt_ScaleForPixelHeight(&stb_font, font->line_height);

    if (stbtt_FindGlyphIndex(&stb_font, (int)unicode_letter) == 0) {
        return false;
    }

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBox(&stb_font, (int)unicode_letter, scale, scale, &x0, &y0, &x1, &y1);

    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&stb_font, &ascent, &descent, &line_gap);

    int adv_w = 0;
    int left_side_bearing = 0;
    stbtt_GetCodepointHMetrics(&stb_font, (int)unicode_letter, &adv_w, &left_side_bearing);
    adv_w = (int)(roundf(adv_w * scale));
    left_side_bearing = (int)(roundf((left_side_bearing * scale)));

    dsc_out->adv_w = adv_w;
    dsc_out->box_h = (uint16_t)(y1 - y0);
    dsc_out->box_w = (uint16_t)(x1 - x0);
    dsc_out->ofs_x = (int16_t)left_side_bearing;
    dsc_out->ofs_y = (int16_t)(y1 * -1);

    dsc_out->bpp = 8;
    return true;
}

static const uint8_t *my_get_glyph_bitmap_cb(const lv_font_t *font, uint32_t unicode_letter)
{
    static uint8_t ret_buf[1024] = {};
    float scale = stbtt_ScaleForPixelHeight(&stb_font, font->line_height);
    int width = 0, height = 0;
    uint8_t *buf = stbtt_GetCodepointBitmap(&stb_font, scale, scale, (int)unicode_letter, &width, &height, NULL, NULL);

    memcpy(ret_buf, buf, width * height);
    stbtt_FreeBitmap(buf, NULL);

    return ret_buf;
}

void app_main(void)
{
    ESP_LOGW(TAG, "Max heap: %u", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    start_disp();


    ESP_LOGW(TAG, "Max heap: %u", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    ESP_LOGI(TAG, "Loading fonts");
    if (stbtt_InitFont(&stb_font, wqy_font_start, 0) < 1) {
        ESP_LOGE(TAG, "Load font failed");
    }

    lv_font_t lv_wqy = {
            .user_data = NULL,
            .get_glyph_dsc = my_get_glyph_dsc_cb,
            .get_glyph_bitmap = my_get_glyph_bitmap_cb,
            .line_height = 32,
            .base_line = 0,
    };


    ESP_LOGW(TAG, "Max heap: %u", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    lv_obj_t *label = lv_label_create(lv_scr_act());
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_text_font(&style, &lv_wqy);
    lv_obj_add_style(label, &style, 0);

    lv_label_set_text(label, "TrueType字体测试\n"
                             "永远相信美好的事情\n即将发生\n"
                             "English works too\n"
                             "Привет товарищ\n");


    ESP_LOGW(TAG, "Max heap: %u", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    while(true) {
        vTaskDelay(1);
        lv_task_handler();
    }
}
