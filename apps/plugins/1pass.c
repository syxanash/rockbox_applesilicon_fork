#include "plugin.h"
#include <stdint.h>

#include "totp_utils/totp.h"

#define CFG_FILE PLUGIN_GAMES_DATA_DIR "/1pass/seeds.cfg"
#define MAX_ENTRIES 16

struct config_entry
{
  char seed[32];
  char vendor[32];
  uint8_t period;
  uint32_t otp;
};

static struct config_entry entries[MAX_ENTRIES];
static int num_entries = 0;
static int entries_per_page = 3;

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
    else if (rb->strncmp(key, "period", 6) == 0)
    {
      int idx = rb->atoi(key + 6) - 1; // "period1" -> 0, "period2" -> 1
      if (idx >= 0 && idx < MAX_ENTRIES)
      {
        entries[idx].period = (uint8_t)rb->atoi(val);
        if (idx + 1 > num_entries)
          num_entries = idx + 1;
      }
    }
  }

  rb->close(fd);
}

#ifdef USB_ENABLE_HID
static void type_string(const char *str)
{
  // maps printable ASCII characters to HID key codes
  // HID_KEYBOARD_A = 0x04, letters are sequential after that
  // HID_KEYBOARD_1 = 0x1E, digits follow in order too
  for (int i = 0; str[i]; i++)
  {
    char c = str[i];
    int key = HID_KEYBOARD_RESERVED; // default: nothing

    if (c >= 'a' && c <= 'z')
      key = HID_KEYBOARD_A + (c - 'a'); // sequential from A
    else if (c >= '1' && c <= '9')
      key = HID_KEYBOARD_1 + (c - '1');
    else if (c == '0')
      key = HID_KEYBOARD_0;
    else if (c == ' ')
      key = HID_KEYBOARD_SPACEBAR;
    else if (c == '\n')
      key = HID_KEYBOARD_RETURN;

    if (key != HID_KEYBOARD_RESERVED)
    {
      rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, key);
      rb->sleep(1);
      rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_RESERVED);
      rb->sleep(1);
    }
  }
}
#endif

enum plugin_status plugin_start(const void *parameter)
{
  // MARK: Setup

  (void)parameter;

  load_cfg();

  rb->lcd_setfont(FONT_UI);

  int btn;

  typedef struct
  {
    int x;
    int y;
    int width;
    int height;
    int position;
  } SelectPixel;

  SelectPixel select_pixel = {
      .x = 20,
      .y = 30,
      .width = 12,
      .height = 12,
      .position = 1,
  };

  uint64_t last_counter[MAX_ENTRIES] = {0};
  int timer_width, timer_height;
  char sample_time[] = "00:00:00";
  rb->lcd_getstringsize(sample_time, &timer_width, &timer_height);

  // MARK: Update
  while (true)
  {
    int current_page = (select_pixel.position - 1) / entries_per_page + 1;
    int page_start = (current_page - 1) * entries_per_page;
    int page_end = (current_page * entries_per_page) < num_entries
                       ? (current_page * entries_per_page)
                       : num_entries;

    struct tm *t = rb->get_time();
    struct tm t_copy = *t;
    unsigned long now = (unsigned long)rb->mktime(&t_copy);
    // fields: tm_hour, tm_min, tm_sec  (0-based)
    // tm_year (years since 1900), tm_mon (0-based), tm_mday (1-based)

    btn = rb->button_get(false);

    // if the USB is connected while the app is running do not quit the plugin
    if (btn == SYS_USB_CONNECTED)
    {
      rb->usb_acknowledge(SYS_USB_CONNECTED_ACK, 0);
      btn = 0;
    }

    if (btn == BUTTON_MENU)
      break;

    int btn_action = btn & ~BUTTON_REPEAT;

    if (btn_action == BUTTON_SCROLL_FWD && select_pixel.position < num_entries)
      select_pixel.position++;
    else if (btn_action == BUTTON_SCROLL_BACK && select_pixel.position > 1)
      select_pixel.position--;

    select_pixel.y = ((select_pixel.position - 1) % entries_per_page) * 30 + 30;

    for (int i = page_start; i < page_end; i++)
    {
      uint64_t counter = now / entries[i].period;

      if (counter != last_counter[i])
      {
        entries[i].otp = totp(entries[i].seed, entries[i].period, now);

        last_counter[i] = counter;
      }
    }

#ifdef USB_ENABLE_HID
    if (btn == BUTTON_SELECT)
    {
      char otp_buf[7];
      rb->snprintf(otp_buf, sizeof(otp_buf), "%06lu", entries[select_pixel.position - 1].otp);

      type_string(otp_buf);
    }
#endif

    // MARK: Draw

    rb->lcd_clear_display();

    rb->lcd_set_foreground(LCD_WHITE);

    // paint the clock on the top right corner of the screen

    char timeBuf[32];
    rb->snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    int timerx = LCD_WIDTH - timer_width;
    int timery = 0;
    rb->lcd_putsxyf(timerx, timery, timeBuf);

    rb->lcd_putsxy(0, 0, "MENU to quit");

    if ((current_page * entries_per_page) < num_entries)
      rb->lcd_putsxy(0, LCD_HEIGHT - 10, "···");

    if (current_page > 1)
      rb->lcd_putsxy(0, 15, "···");

    for (int i = page_start; i < page_end; i++)
    {
      int row = i - page_start;

      // paint the actual vendor with otp code
      unsigned long period_secs = now % entries[i].period;
      int bar_size = ((entries[i].period - period_secs) * LCD_WIDTH) / entries[i].period;

      // paint the elapsed seconds progress bar

      rb->lcd_set_foreground(LCD_RGBPACK(255, 140, 0));
      rb->lcd_fillrect(0, (row * 30) + 44, LCD_WIDTH, 8);

      rb->lcd_set_foreground(LCD_RGBPACK(0, 200, 255));
      rb->lcd_fillrect(0, (row * 30) + 44, bar_size, 8);

      // paint the otp code with vendor
      rb->lcd_set_foreground(LCD_WHITE);
      rb->lcd_putsxyf(20, (row * 30) + 30, "%s - %06u", entries[i].vendor, entries[i].otp);
    }

    // paint selection pixel

    rb->lcd_set_foreground(LCD_RGBPACK(255, 0, 0));
    rb->lcd_fillrect(select_pixel.x - 18, select_pixel.y, select_pixel.width, select_pixel.height);

    rb->lcd_update();
    rb->yield();
  }

  return PLUGIN_OK;
}