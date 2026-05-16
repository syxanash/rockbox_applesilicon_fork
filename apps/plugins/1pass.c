#include "plugin.h"

#define CFG_FILE PLUGIN_GAMES_DATA_DIR "/1pass/seeds.cfg"
#define MAX_ENTRIES 16

struct config_entry
{
  char seed[32];
  char vendor[32];
};

static struct config_entry entries[MAX_ENTRIES];
static int num_entries = 0;

static void load_cfg(void)
{
  int fd = rb->open(CFG_FILE, O_RDONLY);
  if (fd < 0)
    return;

  char line[80];

  while (rb->read_line(fd, line, sizeof(line)) > 0)
  {
    char *eq = line;
    while (*eq && *eq != '=')
      eq++;
    if (*eq != '=')
      continue;

    *eq = '\0';
    char *key = line;
    char *val = eq + 1;

    if (rb->strncmp(key, "seed", 4) == 0)
    {
      int idx = rb->atoi(key + 4) - 1; // "seed1" -> 0, "seed2" -> 1
      if (idx >= 0 && idx < MAX_ENTRIES)
      {
        rb->strlcpy(entries[idx].seed, val, sizeof(entries[idx].seed));
        if (idx + 1 > num_entries)
          num_entries = idx + 1;
      }
    }
    else if (rb->strncmp(key, "vendor", 6) == 0)
    {
      int idx = rb->atoi(key + 6) - 1; // "vendor1" -> 0, "vendor2" -> 1
      if (idx >= 0 && idx < MAX_ENTRIES)
      {
        rb->strlcpy(entries[idx].vendor, val, sizeof(entries[idx].vendor));
        if (idx + 1 > num_entries)
          num_entries = idx + 1;
      }
    }
  }

  rb->close(fd);
}

enum plugin_status plugin_start(const void *parameter)
{
  // MARK: Setup

  (void)parameter;

  load_cfg();

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

    // rb->lcd_putsxyf(50, 80, "seed %s", seed);
    // rb->lcd_putsxyf(50, 100, "vendor %s", vendor);

    // paint the clock on the top right corner of the screen

    char timeBuf[32];
    int timer_width, timer_height;
    rb->snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    rb->lcd_getstringsize(timeBuf, &timer_width, &timer_height);
    int timerx = LCD_WIDTH - timer_width;
    int timery = 0;
    rb->lcd_putsxyf(timerx, timery, timeBuf);

    rb->lcd_putsxy(0, 0, "MENU to quit");

    rb->lcd_putsxyf(20, 20, "Seed: %s", entries[0].seed);

    // rb->lcd_set_foreground(LCD_RGBPACK(255, 0, 0));
    // long now = *rb->current_tick;
    // char buf[32];
    // rb->snprintf(buf, sizeof(buf), "%ld", now);
    // rb->lcd_putsxy(0, 20, buf);

    // struct tm t_copy = *t;
    // time_t unix_ts = rb->mktime(&t_copy);
    // rb->lcd_putsxyf(100, 150, "Unix %ld", (long)unix_ts);

    rb->lcd_update();
  }

  return PLUGIN_OK;
}