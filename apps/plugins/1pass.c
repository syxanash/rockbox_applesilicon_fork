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
  uint8_t digits;
};

static struct config_entry entries[MAX_ENTRIES];
static int num_entries = 0;

// 40 = (header + top ... and bottom ...)
// then 30 = items per row (totp + progress bar)
static int entries_per_page = (LCD_HEIGHT - 40) / 30;
static int utc_offset = 0;

static void load_cfg(void)
{
  int fd = rb->open(CFG_FILE, O_RDONLY);
  if (fd < 0)
    return;

  char line[128];

  while (rb->read_line(fd, line, sizeof(line)) > 0)
  {
    if (rb->strncmp(line, "UTC=", 4) == 0)
    {
      utc_offset = rb->atoi(line + 4);
      continue;
    }

    if (rb->strncmp(line, "otpauth://totp/", 15) != 0)
      continue;
    if (num_entries >= MAX_ENTRIES)
      break;

    char *vendor_start = line + 15;
    char *q = vendor_start;
    while (*q && *q != '?')
      q++;
    if (!*q)
      continue;

    *q = '\0';
    char *params = q + 1;

    struct config_entry *e = &entries[num_entries];
    rb->strlcpy(e->vendor, vendor_start, sizeof(e->vendor));

    // default values if user doesn't specify
    e->period = 30;
    e->digits = 6;

    char *p = params;
    while (*p)
    {
      char *key = p;
      while (*p && *p != '=')
        p++;
      if (!*p)
        break;
      *p++ = '\0';
      char *val = p;
      while (*p && *p != '&')
        p++;
      if (*p)
        *p++ = '\0';

      if (rb->strcmp(key, "secret") == 0)
        rb->strlcpy(e->seed, val, sizeof(e->seed));
      else if (rb->strcmp(key, "period") == 0)
        e->period = (uint8_t)rb->atoi(val);
      else if (rb->strcmp(key, "digits") == 0)
        e->digits = (uint8_t)rb->atoi(val);
    }

    num_entries++;
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
  bool press_selected = false;
  bool usb_connected = false;

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
    unsigned long now = (unsigned long)rb->mktime(&t_copy) - (utc_offset * 3600);

    btn = rb->button_get(false);

    if (btn == SYS_USB_CONNECTED)
    {
      rb->splash(HZ * 2, "USB Connected!");

      rb->usb_acknowledge(SYS_USB_CONNECTED_ACK, 0);
      usb_connected = true;
      btn = 0;
    }
    else if (btn == SYS_USB_DISCONNECTED)
    {
      rb->splash(HZ * 2, "USB Disconnected!");
      usb_connected = false;
      btn = 0;
    }

    if (btn == BUTTON_MENU)
      break;

    int btn_action = btn & ~BUTTON_REPEAT;

    if (btn_action == BUTTON_SCROLL_FWD && select_pixel.position < num_entries)
    {
      rb->keyclick_click(true, btn);
      select_pixel.position++;
    }
    else if (btn_action == BUTTON_SCROLL_BACK && select_pixel.position > 1)
    {
      rb->keyclick_click(true, btn);
      select_pixel.position--;
    }

    select_pixel.y = ((select_pixel.position - 1) % entries_per_page) * 30 + 30;

    for (int i = page_start; i < page_end; i++)
    {
      uint64_t counter = now / entries[i].period;

      if (counter != last_counter[i])
      {
        entries[i].otp = totp(entries[i].seed, entries[i].period, entries[i].digits, now);

        last_counter[i] = counter;
      }
    }

#ifdef USB_ENABLE_HID
    if (btn == BUTTON_SELECT && usb_connected)
    {
      char otp_buf[9];
      rb->snprintf(otp_buf, sizeof(otp_buf), "%0*lu", entries[select_pixel.position - 1].digits, entries[select_pixel.position - 1].otp);

      type_string(otp_buf);
      press_selected = true;
    }
#endif

    if (press_selected && select_pixel.x < LCD_WIDTH)
    {
      select_pixel.x = select_pixel.x + 5;
    }
    else if (press_selected && select_pixel.x >= LCD_WIDTH)
    {
      press_selected = false;
      select_pixel.x = 20;
    }

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
      rb->lcd_putsxyf(20, (row * 30) + 30, "%s - %0*u", entries[i].vendor, entries[i].digits, entries[i].otp);
    }

    // paint selection pixel

    if (press_selected)
      rb->lcd_set_foreground(LCD_RGBPACK(255, 255, 0));
    else
      rb->lcd_set_foreground(LCD_RGBPACK(255, 0, 0));

    rb->lcd_fillrect(select_pixel.x - 18, select_pixel.y, select_pixel.width, select_pixel.height);

    rb->lcd_update();
    rb->yield();
  }

  return PLUGIN_OK;
}