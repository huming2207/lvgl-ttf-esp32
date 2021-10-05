#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <lvgl.h>
#include <lv_hal_st7789.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <font_view.hpp>

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

    auto *buf1 = (lv_color_t *)heap_caps_malloc(ST7789_DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != nullptr);

    auto *buf2 = (lv_color_t *)heap_caps_malloc(ST7789_DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != nullptr);

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

extern "C" void app_main(void)
{
    ESP_LOGW(TAG, "Max heap: %u", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    start_disp();

    ESP_LOGW(TAG, "Max heap: %u", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    ESP_LOGI(TAG, "Loading fonts");
    font_view font;
    ESP_ERROR_CHECK(font.init(wqy_font_start, (wqy_font_end - wqy_font_start), 64));

    ESP_LOGW(TAG, "Max heap: %u", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    lv_obj_t *label = lv_label_create(lv_scr_act());
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_color(&style, lv_color_white());

    font.decorate_font_style(&style);
    lv_obj_add_style(label, &style, 0);

    lv_label_set_text(label, "TrueType字体测试\n"
                             "永远相信美好的事情\n即将发生\n"
//                             "English works too\n"
//                             "Привет товарищ\n"
                            );


    ESP_LOGW(TAG, "Max heap: %u", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    while(true) {
        vTaskDelay(1);
        lv_task_handler();
    }
}
