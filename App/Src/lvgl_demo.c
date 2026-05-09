#include "lvgl_demo.h"

#include <stdint.h>

#include "cmsis_os.h"
#include "lcd.h"
#include "lvgl.h"
#include "ui.h"

#define LVGL_DEMO_BUF_LINES 4U
#define LVGL_DEMO_MAX_HOR_RES 800U
#define LVGL_DEMO_TICK_PERIOD_MS 5U

/* LVGL display buffer in internal RAM */
static uint8_t lvgl_draw_buf[LVGL_DEMO_MAX_HOR_RES * LVGL_DEMO_BUF_LINES * 2U];

static void lvgl_lcd_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;
  const uint16_t *color = (const uint16_t *)px_map;
  // printf("flush: x=%ld..%ld y=%ld..%ld\r\n",
  //        area->x1, area->x2, area->y1, area->y2);

  if ((width <= 0) || (height <= 0))
  {
    lv_display_flush_ready(disp);
    return;
  }

  lcd_set_window((uint16_t)area->x1, (uint16_t)area->y1, (uint16_t)width, (uint16_t)height);
  lcd_write_ram_prepare();

  for (int32_t y = 0; y < height; y++)
  {
    for (int32_t x = 0; x < width; x++)
    {
      LCD->LCD_RAM = *color++;
    }
  }

  lv_display_flush_ready(disp);
}

void lvgl_demo_task(void *argument)
{
  (void)argument;

  lv_init();

  lv_display_t *display = lv_display_create(lcddev.width, lcddev.height);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, lvgl_lcd_flush_cb);
  lv_display_set_buffers(display, lvgl_draw_buf, NULL, sizeof(lvgl_draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  ui_init();

  uint32_t last_tick = osKernelGetTickCount();

  for (;;)
  {
    const uint32_t now = osKernelGetTickCount();
    const uint32_t elapsed = now - last_tick;

    if (elapsed > 0U)
    {
      lv_tick_inc(elapsed);
      last_tick = now;
    }

    ui_tick();
    lv_timer_handler();
    osDelay(LVGL_DEMO_TICK_PERIOD_MS);
  }
}
