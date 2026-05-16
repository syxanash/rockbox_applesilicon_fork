#include "plugin.h"

#include "hotp_utils/sha1.c"
#include "hotp_utils/hmac.c"
#include "hotp_utils/base32.h"

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
      rb->sleep(2);
      rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_RESERVED);
      rb->sleep(2);
    }
  }
}
#endif

enum plugin_status plugin_start(const void *parameter)
{
  // MARK: Setup

  (void)parameter;

  load_cfg();

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
      .y = 20,
      .width = 10,
      .height = 10,
      .position = 1,
  };

  // int text_width, text_height;
  // rb->lcd_getstringsize(file_text, &text_width, &text_height);

  // int cx = (LCD_WIDTH - text_width) / 2;  // horizontal centre
  // int cy = (LCD_HEIGHT - text_height) / 2; // vertical centre
  // rb->lcd_putsxy(cx, cy, file_text);

  while (true)
  {
    // MARK: Update

    struct tm *t = rb->get_time();
    // fields: tm_hour, tm_min, tm_sec  (0-based)
    // tm_year (years since 1900), tm_mon (0-based), tm_mday (1-based)

    btn = rb->button_get(false);

    // if the USB is connected while the app is running do not quit the plugin
    if (btn == SYS_USB_CONNECTED)
    {
      rb->usb_acknowledge(SYS_USB_CONNECTED_ACK, 0);
      continue;
    }

    if (btn == BUTTON_MENU)
      break;

    if (btn == BUTTON_SCROLL_FWD)
    {
      if ((select_pixel.position + 1) <= num_entries)
        select_pixel.position++;

      select_pixel.y = select_pixel.position * 20;
    }

    if (btn == BUTTON_SCROLL_BACK)
    {
      if ((select_pixel.position - 1) >= 1)
        select_pixel.position--;

      select_pixel.y = select_pixel.position * 20;
    }

#ifdef USB_ENABLE_HID
    if (btn == BUTTON_SELECT)
    {
      type_string("hello world");
    }
#endif

    // MARK: Draw

    rb->lcd_clear_display();

    rb->lcd_set_foreground(LCD_WHITE);

    // paint the clock on the top right corner of the screen

    char timeBuf[32];
    int timer_width, timer_height;
    rb->snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    rb->lcd_getstringsize(timeBuf, &timer_width, &timer_height);
    int timerx = LCD_WIDTH - timer_width;
    int timery = 0;
    rb->lcd_putsxyf(timerx, timery, timeBuf);

    rb->lcd_putsxy(0, 0, "MENU to quit");

    for (int i = 0; i < num_entries; i++)
    {
      rb->lcd_putsxyf(20, (i * 20) + 20, "%s - %s", entries[i].vendor, entries[i].seed);
    }

    rb->lcd_set_foreground(LCD_RGBPACK(255, 0, 0));
    rb->lcd_fillrect(select_pixel.x - 18, select_pixel.y, select_pixel.width, select_pixel.height);

    // unsigned char key_bytes[20];
    // int key_len = base32_decode(entries[0].seed, key_bytes, sizeof(key_bytes));

    // rb->lcd_putsxyf(20, 40, "len: %i", key_len);

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