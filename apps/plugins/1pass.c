#include "plugin.h"

static const char *read_file(const char *filePath)
{
  static char file_text[128]; /* static = survives after function returns */
  file_text[0] = '\0';        /* clear it on each call */

  char path[256];
  rb->snprintf(path, sizeof(path), "%s%s", PLUGIN_GAMES_DATA_DIR, filePath);

  int fd = rb->open(path, O_RDONLY);
  if (fd >= 0)
  {
    int bytes_read = rb->read(fd, file_text, sizeof(file_text) - 1);
    if (bytes_read > 0)
    {
      file_text[bytes_read] = '\0';
      while (bytes_read > 0 && (unsigned char)file_text[bytes_read - 1] < 32)
      {
        bytes_read--;
        file_text[bytes_read] = '\0';
      }
    }
    rb->close(fd);
  }

  return file_text;
}

enum plugin_status plugin_start(const void *parameter)
{
  // MARK: Setup

  (void)parameter;

  int btn;

  // const char *file_text = read_file("/1pass/test.dat");

  // int text_width, text_height;
  // rb->lcd_getstringsize(file_text, &text_width, &text_height);

  // int cx = (LCD_WIDTH - text_width) / 2;  // horizontal centre
  // int cy = (LCD_HEIGHT - text_height) / 2; // vertical centre
  // rb->lcd_putsxy(cx, cy, file_text);

  while (true)
  {
    btn = rb->button_get(false);

    if (btn == BUTTON_MENU)
      break;

    rb->lcd_clear_display();

    struct tm *t = rb->get_time();
    // Fields: tm_hour, tm_min, tm_sec  (0-based)
    //         tm_year (years since 1900), tm_mon (0-based), tm_mday (1-based)

    rb->lcd_set_foreground(LCD_WHITE);

    // paint the clock on the top right corner of the screen

    char timeBuf[32];
    int timer_width, timer_height;
    rb->snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    rb->lcd_getstringsize(timeBuf, &timer_width, &timer_height);
    int timerx = LCD_WIDTH - timer_width;
    int timery = 0;
    rb->lcd_putsxyf(timerx, timery, timeBuf);

    // rb->lcd_set_foreground(LCD_RGBPACK(255, 0, 0));
    // long now = *rb->current_tick;
    // char buf[32];
    // rb->snprintf(buf, sizeof(buf), "%ld", now);
    // rb->lcd_putsxy(0, 20, buf);

    struct tm t_copy = *t;
    time_t unix_ts = rb->mktime(&t_copy);

    rb->lcd_putsxyf(100, 150, "Unix %ld", (long)unix_ts);

    rb->lcd_putsxy(0, 0, "MENU to quit");
    rb->lcd_update();
  }

  return PLUGIN_OK;
}