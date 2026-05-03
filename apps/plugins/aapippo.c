#include "plugin.h"

enum plugin_status plugin_start(const void *parameter)
{
  (void)parameter;
  int btn;
  int x = rb->rand() % (LCD_WIDTH - 8) + 4, y = rb->rand() % (LCD_HEIGHT - 8) + 4;

  rb->lcd_setfont(FONT_SYSFIXED);

  bool x_verse = true;
  bool y_verse = true;
  bool bounced_y = false;
  bool bounced_x = false;
  int def_pixel = 8;
  int splat_pixel = 15;
  int pixel_width = 0;
  int pixel_height = 0;

  while (true)
  {
    bounced_x = false;
    bounced_y = false;

    btn = rb->button_get(true);

    if (btn == BUTTON_MENU)
      break;

    if (btn == BUTTON_SCROLL_FWD)
    {
      x_verse = !x_verse;
      y_verse = !y_verse;
    }

    if (btn == BUTTON_SCROLL_BACK)
    {
      x_verse = !x_verse;
      y_verse = !y_verse;
    }

    if (y_verse)
    {
      y += 10;
    }
    else
    {
      y -= 10;
    }

    if (x_verse)
    {
      x += 10;
    }
    else
    {
      x -= 10;
    }

    if ((y + 4) >= LCD_HEIGHT)
    {
      bounced_y = true;
      y_verse = false;
      y = LCD_HEIGHT - 5;
    }
    else if ((y - 4) <= 0)
    {
      bounced_y = true;
      y_verse = true;
      y = 4;
    }

    if ((x + 4) >= LCD_WIDTH)
    {
      bounced_x = true;
      x_verse = false;
      x = LCD_WIDTH - 5;
    }
    else if ((x - 4) <= 0)
    {
      bounced_x = true;
      x_verse = true;
      x = 4;
    }

    rb->lcd_clear_display();
    rb->lcd_set_foreground(LCD_RGBPACK(0, 200, 255));

    if (bounced_x) {
      pixel_height = splat_pixel;
    } else {
      pixel_height = def_pixel;
    }
    
    if (bounced_y) {
      pixel_width = splat_pixel;
    } else {
      pixel_width = def_pixel;
    }

    rb->lcd_fillrect(x - 4, y - 4, pixel_width, pixel_height);
    rb->lcd_set_foreground(LCD_WHITE);

    rb->lcd_putsxy(0, 0, "MENU to quit");
    rb->lcd_update();
  }

  return PLUGIN_OK;
}