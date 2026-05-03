#include "plugin.h"

enum plugin_status plugin_start(const void *parameter)
{
  // MARK: Setup

  (void)parameter;
  int btn;

  rb->lcd_setfont(FONT_SYSFIXED);

  typedef struct
  {
    int x;
    int y;
    bool x_verse;
    bool y_verse;
    bool bounced_y;
    bool bounced_x;
    int def_pixel;
    int splat_pixel;
    int pixel_width;
    int pixel_height;
  } Blue;

  Blue pippo = {
      .x = rb->rand() % (LCD_WIDTH - 8) + 4,
      .y = rb->rand() % (LCD_HEIGHT - 8) + 4,
      .x_verse = true,
      .y_verse = true,
      .bounced_y = false,
      .bounced_x = false,
      .def_pixel = 8,
      .splat_pixel = 15,
      .pixel_width = 0,
      .pixel_height = 0,
  };

  // MARK: Update

  while (true)
  {
    pippo.bounced_x = false;
    pippo.bounced_y = false;

    btn = rb->button_get(true);

    if (btn == BUTTON_MENU)
      break;

    if (btn == BUTTON_SCROLL_FWD)
    {
      pippo.x_verse = !pippo.x_verse;
      pippo.y_verse = !pippo.y_verse;
    }

    if (btn == BUTTON_SCROLL_BACK)
    {
      pippo.x_verse = !pippo.x_verse;
      pippo.y_verse = !pippo.y_verse;
    }

    if (pippo.y_verse)
    {
      pippo.y += 10;
    }
    else
    {
      pippo.y -= 10;
    }

    if (pippo.x_verse)
    {
      pippo.x += 10;
    }
    else
    {
      pippo.x -= 10;
    }

    if ((pippo.y + 4) >= LCD_HEIGHT)
    {
      pippo.bounced_y = true;
      pippo.y_verse = false;
      pippo.y = LCD_HEIGHT - 5;
    }
    else if ((pippo.y - 4) <= 0)
    {
      pippo.bounced_y = true;
      pippo.y_verse = true;
      pippo.y = 4;
    }

    if ((pippo.x + 4) >= LCD_WIDTH)
    {
      pippo.bounced_x = true;
      pippo.x_verse = false;
      pippo.x = LCD_WIDTH - 5;
    }
    else if ((pippo.x - 4) <= 0)
    {
      pippo.bounced_x = true;
      pippo.x_verse = true;
      pippo.x = 4;
    }

    // MARK: Draw

    rb->lcd_clear_display();
    rb->lcd_set_foreground(LCD_RGBPACK(0, 200, 255));

    if (pippo.bounced_x)
    {
      pippo.pixel_height = pippo.splat_pixel;
    }
    else
    {
      pippo.pixel_height = pippo.def_pixel;
    }

    if (pippo.bounced_y)
    {
      pippo.pixel_width = pippo.splat_pixel;
    }
    else
    {
      pippo.pixel_width = pippo.def_pixel;
    }

    rb->lcd_fillrect(pippo.x - 4, pippo.y - 4, pippo.pixel_width, pippo.pixel_height);
    rb->lcd_set_foreground(LCD_WHITE);

    rb->lcd_putsxy(0, 0, "MENU to quit");
    rb->lcd_update();
  }

  return PLUGIN_OK;
}