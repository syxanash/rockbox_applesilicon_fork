# Rockbox Plugin Development — Complete Guide

> **Target device:** iPod Video (5th gen) — 320×240 colour LCD, clickwheel  
> **Simulator:** `rockboxui` (macOS build) for fast iteration without hardware  
> **All API calls go through the `rb->` pointer** (`const struct plugin_api *rb`)

---

## Build & install cheatsheet

```bash
# Simulator (fastest for testing — runs on your Mac)
cd ~/Desktop/rockbox/build-sim && rm -f apps/plugins/1pass.rock && make -j$(sysctl -n hw.logicalcpu) && cp apps/plugins/1pass.rock simdisk/.rockbox/rocks/games/1pass.rock && ./rockboxui

# Real iPod
cd ~/Desktop/rockbox/build-ipod && make -j$(sysctl -n hw.logicalcpu) && cp apps/plugins/1pass.rock "/Volumes/OPENS'S IPO/.rockbox/rocks/games/1pass.rock"

# If firmware version mismatch error after copying:
cp build-ipod/rockbox.ipod "/Volumes/YOUR_IPOD/.rockbox/"
# Hold Menu+Select ~5s to reboot
```

---

## Chapter 0 — C basics you need

You don't need to master C to write plugins, but a handful of concepts come up everywhere.

### `#define` — compile-time constants

```c
#define HZ 100          // replaces every mention of HZ with the literal 100
#define LCD_WIDTH 320   // no runtime cost, no variable stored in memory
```

Unlike a variable, a `#define` doesn't exist at runtime — the compiler pastes the value in wherever you write the name.

### Variables and types

```c
int score = 0;           // whole number (positive or negative)
long ticks = 0;          // larger whole number (at least 32 bits)
bool running = true;     // true / false (from <stdbool.h>)
char name[32] = "";      // array of 32 characters = a string buffer
```

### `struct` — a bundle of variables

```c
struct point {
    int x;
    int y;
};

struct point p;   // create one
p.x = 10;        // access fields with a dot
p.y = 20;
```

### Pointers and `&` / `*`

A pointer is the *address* of a variable, not the value itself.

```c
int score = 5;
int *ptr = &score;   // ptr = address of score  (&  = "address of")
*ptr = 10;           // change score through the pointer  (* = "value at address")
// score is now 10
```

Why does this matter?  Some Rockbox functions want to *modify* your variable — you pass `&score` (the address) so the function can write back into it:

```c
rb->set_int("Score", "", UNIT_INT, &score, ...);
//                                  ^ pass address so set_int can modify score
```

### `->` vs `.`

When you have a pointer to a struct, you use `->` instead of `.`:

```c
struct point p;       // normal struct
p.x = 5;              // dot notation

struct point *pp = &p;   // pointer to a struct
pp->x = 5;               // arrow notation (same as (*pp).x)
```

This is why every API call uses `rb->lcd_update()` — `rb` is a *pointer* to the API struct.

### `enum` — named numbers

```c
enum color { RED, GREEN, BLUE };   // RED=0, GREEN=1, BLUE=2
enum color c = GREEN;

enum plugin_status { PLUGIN_OK = 0, PLUGIN_ERROR = -1, ... };
```

Enums are just integers with readable names.  `plugin_start` returns `enum plugin_status` — that's just an integer, but the name makes the intent clear.

### `static` inside a function

```c
static int counter = 0;   // initialised once, survives between calls
counter++;
```

At file scope (outside any function), `static` means "only visible in this file" — used to avoid name collisions across plugins.

### `sizeof`

```c
sizeof(int)         // how many bytes an int uses (usually 4)
sizeof(my_struct)   // how many bytes the whole struct uses
sizeof(my_array)    // total bytes in an array
```

Used constantly with `rb->read` and `rb->write` to say how many bytes to transfer.

### `NULL` and checking return values

```c
int fd = rb->open("file.dat", O_RDONLY);
if (fd < 0) {
    // file didn't open — handle the error
}

DIR *dp = rb->opendir("/path");
if (dp == NULL) {
    // directory doesn't exist
}
```

Always check return values from file/directory calls.  A negative `int` or a `NULL` pointer means failure.

---

## Chapter 1 — Plugin anatomy

Every plugin is a single `.c` file.  The mandatory boilerplate is exactly two things:

```c
#include "plugin.h"   // pulls in the entire Rockbox API

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;   // silence "unused variable" compiler warning

    /* your code here */

    return PLUGIN_OK;
}
```

`plugin_start` is the entry point — Rockbox calls it when the user launches your plugin.  It's like `main()` in a normal C program.

The `(void)parameter;` line is a C idiom: casting a variable to void tells the compiler "I know I'm not using this — don't warn me."

### Return codes

| Value | Meaning |
|---|---|
| `PLUGIN_OK` | Exited normally, return to file browser |
| `PLUGIN_USB_CONNECTED` | USB was plugged in — Rockbox handles it |
| `PLUGIN_GOTO_WPS` | Return to "now playing" screen |
| `PLUGIN_GOTO_ROOT` | Return to main menu |
| `PLUGIN_ERROR` | Something went wrong |

### Registering the plugin

1. Create `apps/plugins/myplugin.c`
2. Add `myplugin.c` to `apps/plugins/SOURCES`
3. Add `myplugin,apps` to `apps/plugins/CATEGORIES`  
   (categories: `apps`, `games`, `demos`, `viewers`)

### Path constants (strings you can use in file paths)

| Macro | Expands to |
|---|---|
| `PLUGIN_APPS_DATA_DIR` | `.rockbox/rocks/apps` |
| `PLUGIN_GAMES_DATA_DIR` | `.rockbox/rocks/games` |
| `PLUGIN_DEMOS_DATA_DIR` | `.rockbox/rocks/demos` |
| `ROCKBOX_DIR` | `.rockbox` |

```c
#define SAVE_FILE  PLUGIN_APPS_DATA_DIR "/myplugin.dat"
// expands to: ".rockbox/rocks/apps/myplugin.dat"
// String literals next to each other in C are automatically joined.
```

---

## Chapter 2 — LCD / Drawing API

> **C concept refresher:** `LCD_WIDTH` and `LCD_HEIGHT` are `#define` constants — 320 and 240.  `LCD_RGBPACK(r,g,b)` is a macro that packs three bytes into a single integer colour value.

The screen is **320×240**, colour, origin `(0,0)` at top-left.

### Clear & flush

```c
rb->lcd_clear_display();          // wipe framebuffer (fast — just fills with bg colour)
rb->lcd_update();                 // push entire framebuffer to the physical screen
rb->lcd_update_rect(x, y, w, h); // partial update — faster when only a small area changed
```

Drawing calls write into an off-screen buffer.  Nothing appears on the screen until you call `lcd_update()`.  This prevents flickering.

### Colours

```c
rb->lcd_set_foreground(LCD_RGBPACK(255, 0, 0));  // bright red
rb->lcd_set_background(LCD_BLACK);

// Built-in colour constants:
// LCD_BLACK, LCD_WHITE, LCD_RED, LCD_GREEN, LCD_BLUE
// LCD_YELLOW, LCD_CYAN, LCD_MAGENTA, LCD_DARKGRAY, LCD_LIGHTGRAY
```

### Drawing primitives

```c
rb->lcd_drawpixel(x, y);                    // single pixel
rb->lcd_drawline(x1, y1, x2, y2);          // line between two points
rb->lcd_hline(x1, x2, y);                  // horizontal line — faster than drawline
rb->lcd_vline(x, y1, y2);                  // vertical line
rb->lcd_drawrect(x, y, w, h);              // rectangle outline
rb->lcd_fillrect(x, y, w, h);              // filled rectangle
```

All these use the current foreground colour.

### Draw mode (how pixels combine)

```c
rb->lcd_set_drawmode(DRMODE_SOLID);       // normal overwrite (default)
rb->lcd_set_drawmode(DRMODE_INVERSEVID);  // invert what's already on screen
rb->lcd_set_drawmode(DRMODE_FG);          // draw only foreground colour pixels (skip bg)
rb->lcd_set_drawmode(DRMODE_BG);          // draw only background colour pixels

int old_mode = rb->lcd_get_drawmode();    // save current mode
// ... draw something ...
rb->lcd_set_drawmode(old_mode);           // restore
```

### Text

```c
rb->lcd_setfont(FONT_SYSFIXED);    // tiny built-in font, always available
rb->lcd_setfont(FONT_UI);          // user's chosen proportional font (may be large)

// Draw at pixel coordinates (x, y = top-left corner of text)
rb->lcd_putsxy(10, 20, "hello world");
rb->lcd_putsxyf(10, 20, "score: %d", score);   // printf-style formatting

// Draw at character-cell coordinates (column, row) — simpler for text-only UIs
rb->lcd_puts(0, 0, "line one");   // col 0, row 0
rb->lcd_puts(0, 1, "line two");   // col 0, row 1

// Long lines that auto-scroll when wider than the screen
rb->lcd_puts_scroll(0, 2, "this line will scroll if too wide for the display");
rb->lcd_scroll_stop();            // stop all scrolling
```

**Measuring text before drawing** (essential for centring):

```c
int w, h;
rb->lcd_getstringsize("hello", &w, &h);
// w = pixel width of this string in the current font
// h = pixel height (line height)

int cx = (LCD_WIDTH  - w) / 2;   // horizontal centre
int cy = (LCD_HEIGHT - h) / 2;   // vertical centre
rb->lcd_putsxy(cx, cy, "hello");
```

The `&w` passes the *address* of w — `getstringsize` writes back into your variables.

### Splash (quick overlay message)

```c
rb->splash(HZ * 2, "Saved!");              // show for 2 seconds then return
rb->splashf(HZ, "Level %d complete", lvl); // printf-style, HZ = 1 second
rb->splash(0, "Press any key");            // stays until button press
```

### Embedded bitmaps (sprite sheets)

Bitmap files live in `apps/plugins/bitmaps/` and are converted to C arrays at build time.

```c
// Include the generated header — name matches the .bmp filename
#include "bitmaps/myplugin_tiles.h"
// This defines:  extern const fb_data myplugin_tiles[];
//                #define MYPLUGIN_TILES_WIDTH  ...
//                #define MYPLUGIN_TILES_HEIGHT ...

// Draw the whole bitmap at screen position (x, y)
rb->lcd_bitmap(myplugin_tiles, x, y, MYPLUGIN_TILES_WIDTH, MYPLUGIN_TILES_HEIGHT);

// Draw one tile from a sprite sheet (tiles are arranged horizontally)
// lcd_bitmap_part(src, src_x, src_y, stride, dst_x, dst_y, w, h)
int tile = 3;
int tile_w = 16, tile_h = 16;
rb->lcd_bitmap_part(myplugin_tiles,
                    tile * tile_w, 0,          // crop from position in sheet
                    MYPLUGIN_TILES_WIDTH,       // stride = full sheet width
                    x, y, tile_w, tile_h);     // where to draw on screen

// Transparent blit (one colour treated as transparent)
rb->lcd_bitmap_transparent(myplugin_tiles, x, y, W, H);
```

Add bitmaps to `apps/plugins/bitmaps/SOURCES`:
```
myplugin_tiles.bmp
```

---

## Chapter 3 — Input API

> **C concept refresher:** `long` is a wider integer type (at least 32 bits).  Button events are encoded as integers with bit flags — you test them with the bitwise AND operator `&`.

### The tick clock

```c
// HZ is always 100 (ticks per second), regardless of device speed
rb->sleep(HZ);          // sleep exactly 1 second
rb->sleep(HZ / 10);     // sleep 100 ms  (integer division: 100/10 = 10 ticks)
rb->sleep(HZ / 60);     // sleep ~16 ms  (≈60 fps frame budget)

rb->yield();            // give up CPU for one tick without sleeping — be polite

long now = *rb->current_tick;   // read current tick count
// *rb->current_tick  — the * dereferences the pointer to get the actual value
```

### Reading buttons

```c
long btn = rb->button_get(true);     // blocks until a button is pressed
long btn = rb->button_get(false);    // returns BUTTON_NONE immediately if nothing
long btn = rb->button_get_w_tmo(HZ / 10);  // wait up to 100 ms, then return SYS_TIMEOUT
```

### iPod Video button constants

```c
BUTTON_MENU        // top
BUTTON_PLAY        // bottom  (Play/Pause label on the hardware)
BUTTON_LEFT        // left
BUTTON_RIGHT       // right
BUTTON_SELECT      // centre press

BUTTON_SCROLL_FWD  // clockwise scroll
BUTTON_SCROLL_BACK // counter-clockwise scroll

BUTTON_NONE        // nothing pressed
```

**Modifier flags** (OR'd together with the button):

```c
if (btn & BUTTON_REPEAT) { /* button is being held */ }
if (btn & BUTTON_REL)    { /* button was just released */ }

// Combination: detect Select+hold as a "hard quit"
if (btn == (BUTTON_SELECT | BUTTON_REPEAT)) { done = true; }
```

The `|` operator combines two values by setting both sets of bits.  
The `&` operator checks whether certain bits are set.

### Hold switch

```c
if (rb->button_hold()) {
    // Physical hold switch is on — typically ignore all input
}
```

### Portable actions (recommended for portable plugins)

Instead of hard-coding `BUTTON_MENU` etc., use abstract actions that map correctly on every device:

```c
#include "lib/pluginlib_actions.h"

const struct button_mapping *plugin_contexts[] = { pla_main_ctx };

int action = pluginlib_getaction(TIMEOUT_BLOCK,           // TIMEOUT_BLOCK = wait forever
                                  plugin_contexts,
                                  ARRAYLEN(plugin_contexts));
// ARRAYLEN(arr) = sizeof(arr)/sizeof(*arr) — number of elements in an array
switch (action) {
    case PLA_UP:           /* up or scroll forward */  break;
    case PLA_DOWN:         /* down or scroll back */   break;
    case PLA_LEFT:                                     break;
    case PLA_RIGHT:                                    break;
    case PLA_SELECT:       /* confirm */               break;
    case PLA_SELECT_REL:   /* confirm on release */    break;
    case PLA_CANCEL:       /* go back / quit */        break;
    case PLA_SCROLL_FWD:   /* scroll wheel forward */  break;
    case PLA_SCROLL_BACK:  /* scroll wheel back */     break;
    // _REPEAT variants: PLA_UP_REPEAT, PLA_DOWN_REPEAT, ...
}
```

### Handling USB & system events

Always pass unknown button values to `default_event_handler` so Rockbox can handle USB, power-off, etc.:

```c
switch (btn) {
    case BUTTON_MENU:   done = true; break;
    // ... your cases ...
    default:
        if (rb->default_event_handler(btn) == SYS_USB_CONNECTED)
            return PLUGIN_USB_CONNECTED;
        break;
}
```

Or with a cleanup callback:

```c
static void on_exit(void *param) {
    (void)param;
    save_state();    // runs before Rockbox handles the event
}

if (rb->default_event_handler_ex(btn, on_exit, NULL) == SYS_USB_CONNECTED)
    return PLUGIN_USB_CONNECTED;
```

---

## Chapter 4 — Timing patterns

### Simple pause

```c
rb->sleep(HZ / 2);   // pause 500 ms, then continue
```

### Game loop with tick-based timing (fixed timestep)

```c
long next_frame = *rb->current_tick;

while (running) {
    long now = *rb->current_tick;
    if (now >= next_frame) {
        update_game();
        draw_game();
        next_frame = now + HZ / 30;   // target ~30 fps
    }

    long btn = rb->button_get(false);   // non-blocking poll
    handle_input(btn);

    rb->yield();   // don't hog the CPU
}
```

### `button_get_w_tmo` pattern (simplest for games)

This is the most common pattern — the timeout sets your maximum frame rate:

```c
while (!done) {
    update_state();
    redraw();

    long btn = rb->button_get_w_tmo(HZ / 20);   // 50 ms = 20 fps max
    switch (btn) {
        case BUTTON_LEFT:  x--; break;
        case BUTTON_RIGHT: x++; break;
        case BUTTON_MENU:  done = true; break;
        default:
            if (rb->default_event_handler(btn) == SYS_USB_CONNECTED)
                return PLUGIN_USB_CONNECTED;
    }
    rb->reset_poweroff_timer();   // keep screen + device awake
}
```

`rb->reset_poweroff_timer()` resets the auto-poweroff countdown — call it every frame so the iPod doesn't sleep mid-game.

### Hardware interrupt timer

For exact-frequency callbacks (sound synthesis, sensor polling):

```c
static volatile bool tick_flag = false;

static void timer_callback(void) {
    // called at interrupt level — MUST be fast, no malloc, no rb-> calls
    tick_flag = true;
}

// Register: priority=1, no-unregister-cb, TIMER_FREQ/100 cycles per call
rb->timer_register(1, NULL, TIMER_FREQ / 100, timer_callback IF_COP(, CPU));

// In your main loop, check the flag:
if (tick_flag) {
    tick_flag = false;
    do_precise_work();
}

rb->timer_unregister();   // always unregister before returning from plugin_start
```

`volatile bool` — the `volatile` keyword tells the compiler "this variable can change unexpectedly (from an interrupt), don't optimise it away."

---

## Chapter 5 — Menus

### Quick string-list menu

The `MENUITEM_STRINGLIST` macro defines a menu from a list of string literals.  `do_menu` runs it and returns the index of the selected item (0-based), or a negative value if the user pressed Back.

```c
static int show_main_menu(void) {
    int sel = 0;

    MENUITEM_STRINGLIST(menu, "My Plugin", NULL,
                        "Start Game",      // index 0
                        "Settings",        // index 1
                        "High Scores",     // index 2
                        "Quit");           // index 3

    while (true) {
        switch (rb->do_menu(&menu, &sel, NULL, false)) {
            case 0: return 1;               // Start Game selected
            case 1: settings_screen(); break;
            case 2: scores_screen();   break;
            default: return 0;              // Quit or Back
        }
    }
}
```

> **C note:** `&sel` passes the address of `sel` so `do_menu` can remember which item was last highlighted — if you re-enter the menu it'll show the cursor in the same place.

### Built-in setting screens

Each of these replaces your screen with a full-screen setting editor:

```c
// Integer spinner
int speed = 5;
rb->set_int("Speed",       // title
             "steps",      // unit label
             UNIT_INT,     // voice unit
             &speed,       // pointer to variable to modify
             NULL,         // optional change callback
             1,            // step size
             1, 10,        // min, max
             NULL);        // optional formatter function

// Boolean toggle (on/off)
bool wrap = true;
rb->set_bool("Wrap around", &wrap);

// Enum (pick from a list of strings)
static const struct opt_items difficulty_opts[] = {
    { "Easy",   TALK_ID(0, UNIT_INT) },   // TALK_ID is for voice output; use 0 if unsure
    { "Normal", TALK_ID(1, UNIT_INT) },
    { "Hard",   TALK_ID(2, UNIT_INT) },
};
int difficulty = 1;
rb->set_option("Difficulty", &difficulty, RB_INT,
                difficulty_opts,
                3,      // number of options
                NULL);  // change callback
```

### Yes/no confirmation

```c
if (rb->yesno_pop("Overwrite save?")) {
    // user chose Yes
}
```

### Keyboard text input

```c
char username[32] = "";
int result = rb->kbd_input(username, sizeof(username), NULL);
if (result >= 0) {
    // user confirmed — username holds their text
} else {
    // user cancelled
}
```

---

## Chapter 6 — File I/O

> **C concept refresher:** File access uses integer "file descriptors" (`fd`).  Think of it like a ticket number you get when you open a file — you use the number for all subsequent operations, then hand it back with `close`.

### Writing a file

```c
int fd = rb->open(PLUGIN_APPS_DATA_DIR "/mydata.dat",
                  O_CREAT | O_WRONLY | O_TRUNC,   // create/overwrite
                  0666);                           // permissions
if (fd >= 0) {
    rb->write(fd, &my_struct, sizeof(my_struct));  // write bytes
    rb->close(fd);
}
```

`sizeof(my_struct)` gives the exact number of bytes to write — this is how you save a C struct directly to disk.

### Reading a file

```c
int fd = rb->open(PLUGIN_APPS_DATA_DIR "/mydata.dat", O_RDONLY);
if (fd >= 0) {
    rb->read(fd, &my_struct, sizeof(my_struct));
    rb->close(fd);
}
```

### Common open flags

| Flag | Meaning |
|---|---|
| `O_RDONLY` | Read only |
| `O_WRONLY` | Write only |
| `O_RDWR` | Read + write |
| `O_CREAT` | Create if missing |
| `O_TRUNC` | Truncate (empty) existing file |
| `O_APPEND` | Seek to end before each write |

Combine multiple flags with `|`:  `O_CREAT | O_WRONLY | O_TRUNC`

### Utility helpers

```c
rb->file_exists("/path/to/file");          // returns true/false
rb->remove("/path/to/file");               // delete
rb->rename("/old.path", "/new.path");      // rename or move
off_t size = rb->filesize(fd);             // size of open file in bytes
rb->lseek(fd, 0, SEEK_SET);               // jump to start of file
rb->lseek(fd, offset, SEEK_CUR);          // jump relative to current position
int n = rb->read_line(fd, buf, sizeof(buf)); // read one text line, returns chars read
```

### Writing formatted text

```c
rb->fdprintf(fd, "score=%d name=%s\n", score, name);
// Works like printf but writes to a file.
```

### Directory operations

```c
rb->mkdir("/path/to/newdir");

DIR *dp = rb->opendir("/some/path");
if (dp != NULL) {
    struct dirent *entry;
    while ((entry = rb->readdir(dp)) != NULL) {
        // entry->d_name  = filename string
        struct dirinfo info = rb->dir_get_info(dp, entry);
        bool is_dir = (info.attribute & ATTR_DIRECTORY) != 0;
        (void)is_dir;
    }
    rb->closedir(dp);
}
```

---

## Chapter 7 — Persistent config with `configfile`

`lib/configfile.h` manages a simple key=value text file on disk.  You describe your settings in a struct array; the library handles the parsing and serialisation.

```c
#include "lib/configfile.h"

#define CFG_FILE   PLUGIN_APPS_DATA_DIR "/myplugin.cfg"
#define CFG_VER    1   // increment if you change the config structure

static int  volume   = 5;
static int  speed    = 3;
static bool wrap     = true;
static char pname[32] = "Player";

// For a TYPE_ENUM, you need a string array for the values:
static const char *wrap_vals[] = { "off", "on" };

static struct configdata cfg[] = {
    // { type,    min, max, pointer,           key name,   enum strings }
    { TYPE_INT,    0,  10, { .int_p  = &volume }, "volume",  NULL       },
    { TYPE_INT,    1,  10, { .int_p  = &speed  }, "speed",   NULL       },
    { TYPE_ENUM,   0,   2, { .int_p  = (int*)&wrap }, "wrap", (char**)wrap_vals },
    { TYPE_STRING, 0,  32, { .string = pname   }, "name",    NULL       },
};

// At startup:
configfile_load(CFG_FILE, cfg, ARRAYLEN(cfg), CFG_VER);
// If the file doesn't exist yet, your defaults above are kept.

// Whenever settings change:
configfile_save(CFG_FILE, cfg, ARRAYLEN(cfg), CFG_VER);
```

> **C note:** The `.int_p = &volume` syntax is a *designated initialiser* for a union — it selects which field of the union to use and sets it.  Just copy this pattern; you don't need to understand unions deeply.

---

## Chapter 8 — High scores with `highscore`

```c
#include "lib/highscore.h"

#define SCORE_FILE  PLUGIN_GAMES_DATA_DIR "/mygame.score"
#define NUM_SCORES  5

static struct highscore scores[NUM_SCORES];
// struct highscore { char name[32]; int score; int level; }

// At startup:
highscore_load(SCORE_FILE, scores, NUM_SCORES);
// If file doesn't exist yet, the array stays zeroed.

// After game over:
int rank = highscore_update(final_score, level, "???", scores, NUM_SCORES);
// rank >= 0  means it made the table (rank = position, 0 = best)
// rank < 0   means it was too low to appear

highscore_save(SCORE_FILE, scores, NUM_SCORES);

// Show the leaderboard (Rockbox draws it for you):
highscore_show(rank, scores, NUM_SCORES, true);  // true = show level column
```

---

## Chapter 9 — Sound & audio

### Beep

Works on all targets; no audio buffer or setup needed:

```c
// beep_play(frequency_hz, duration_ms, amplitude)
// amplitude range: 0 (silent) to 32767 (max)
rb->beep_play(880,  80, 15000);   // A5 — short high beep
rb->beep_play(440, 300, 20000);   // A4 — longer medium beep
rb->beep_play(220, 500, 25000);   // A3 — low rumble
```

### System sounds

```c
rb->system_sound_play(SOUND_KEYCLICK);   // button click
rb->system_sound_play(SOUND_TRACK_SKIP); // skip jingle
```

to replicate the click sound:

```c
rb->keyclick_click(true, btn);
```


### Controlling music playback from your plugin

The user's music keeps playing by default when your plugin runs.  You can control it:

```c
int status = rb->audio_status();
// status & AUDIO_STATUS_PLAY  = playing
// status & AUDIO_STATUS_PAUSE = paused

rb->audio_pause();
rb->audio_resume();
rb->audio_stop();
rb->audio_next();     // next track
rb->audio_prev();     // previous track

struct mp3entry *track = rb->audio_current_track();
if (track != NULL) {
    rb->lcd_putsxyf(0, 0, "%s", track->title);
    rb->lcd_putsxyf(0, 10, "%s", track->artist);
}
```

Add a **Playback Control** submenu with a single function call:

```c
#include "lib/playback_control.h"

// Inside your menu switch:
case MY_MENU_PLAYBACK:
    playback_control(NULL);
    break;
```

### Volume

```c
rb->adjust_volume(+1);   // raise one step
rb->adjust_volume(-1);   // lower one step
int current = rb->sound_current(SOUND_VOLUME);
```

---

## Chapter 10 — Hardware APIs

### Battery

```c
int pct     = rb->battery_level();      // 0–100 percent charge
int minutes = rb->battery_time();       // estimated minutes remaining (-1 if unknown)
int mv      = rb->battery_voltage();    // millivolts
bool safe   = rb->battery_level_safe(); // false = critically low, warn user

// Is charger connected?
bool charging = rb->charger_inserted();
```

### Backlight

```c
rb->backlight_on();
rb->backlight_off();
rb->backlight_set_brightness(8);   // 0–max; max varies by target
bool lit = rb->is_backlight_on(false);
```

### Real-time clock

```c
struct tm *t = rb->get_time();
// Fields: tm_hour, tm_min, tm_sec  (0-based)
//         tm_year (years since 1900), tm_mon (0-based), tm_mday (1-based)

rb->lcd_putsxyf(0, 0, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);

// Seed the random number generator from current time (do this at startup):
rb->srand(t->tm_sec + t->tm_min * 60 + t->tm_hour * 3600);
int roll = rb->rand() % 6 + 1;   // random 1–6
```

`%02d` in a format string means "print integer, minimum 2 digits, pad with zero".

### USB detection

```c
if (rb->usb_inserted()) {
    save_state();
    return PLUGIN_USB_CONNECTED;
}
```

### Power

```c
rb->sys_poweroff();   // shut down device
rb->sys_reboot();     // reboot
```

---

## Chapter 11 — Viewports (advanced layout)

A viewport is a clipping rectangle.  Everything you draw is cropped to it.  They let you build split-screen layouts (e.g. header + game area + footer) cleanly.

```c
struct viewport game_area;
rb->viewport_set_defaults(&game_area, SCREEN_MAIN);  // start with full-screen defaults

// Customise the region
game_area.x      = 0;
game_area.y      = 20;                  // leave 20px for a header
game_area.width  = LCD_WIDTH;
game_area.height = LCD_HEIGHT - 40;    // leave 20px for a footer
game_area.fg_pattern = LCD_WHITE;
game_area.bg_pattern = LCD_BLACK;
game_area.font   = FONT_SYSFIXED;

// Get the screen object (needed for viewport-specific methods)
struct screen *display = rb->screens[SCREEN_MAIN];

// Activate (returns old viewport so you can restore it)
struct viewport *saved = display->set_viewport(&game_area);

// Now draw — all operations clipped to game_area
display->clear_viewport();
rb->lcd_putsxy(0, 0, "inside game area");
display->update_viewport();    // flush only this viewport's region (faster)

// Restore
display->set_viewport(saved);
```

### Multi-screen (main + remote LCD)

Some targets have a secondary remote display.  The portable pattern uses `FOR_NB_SCREENS`:

```c
FOR_NB_SCREENS(i) {
    struct screen *display = rb->screens[i];
    display->clear_display();
    display->getstringsize("hi", &w, &h);
    display->putsxy((display->lcdwidth - w) / 2,
                    (display->lcdheight - h) / 2,
                    "hi");
    display->update();
}
// On iPod (no remote), NB_SCREENS == 1, so the loop runs once.
// FOR_NB_SCREENS expands to:  for (int i = 0; i < NB_SCREENS; i++)
```

`struct screen` has method-style pointers that mirror `rb->lcd_*`:  
`display->clear_display()`, `display->putsxy()`, `display->update()`,  
`display->getstringsize()`, `display->setfont()`, `display->puts_scroll()`, etc.

---

## Chapter 12 — Plugin memory & utilities

### Plugin buffer

A chunk of free RAM reserved for the plugin's use:

```c
size_t bufsize;
void *buf = rb->plugin_get_buffer(&bufsize);
// bufsize tells you how many bytes you got (varies; ~512 KB on iPod Video)
// You can use this as a large array, tile cache, etc.
```

### String utilities

Use these instead of the standard C library:

```c
rb->snprintf(buf, sizeof(buf), "score: %d", score);   // safe printf to buffer
rb->strlen("hello");          // string length (not counting null terminator)
rb->strcmp(a, b);             // 0 if equal, <0 if a<b, >0 if a>b
rb->strcpy(dst, src);         // copy string
rb->strlcpy(dst, src, sizeof(dst));   // safe copy — always null-terminates
rb->strcat(dst, src);         // append src onto end of dst
rb->memset(ptr, 0, size);     // fill memory with a byte value (0 = zero it out)
rb->memcpy(dst, src, size);   // copy bytes (regions must not overlap)
rb->memmove(dst, src, size);  // copy bytes (handles overlapping regions)
```

### Sorting

```c
// Compare function: return negative if a < b, 0 if equal, positive if a > b
static int compare_scores(const void *a, const void *b) {
    return ((const int *)b)[0] - ((const int *)a)[0];  // descending
}

rb->qsort(my_array, count, sizeof(my_array[0]), compare_scores);
```

### Random numbers

```c
rb->srand(*rb->current_tick);   // seed once at startup (or use rb->get_time())
int r = rb->rand();              // returns 0 to RAND_MAX
int r = rb->rand() % N;          // 0 to N-1
int r = rb->rand() % N + 1;      // 1 to N
```

---

## Chapter 13 — Threads & queues (advanced)

Most plugins are single-threaded.  Only reach for threads when you genuinely need background work (e.g. loading data while keeping the UI responsive).

> **C concept:** A thread is an independent stream of execution.  It runs the function you give it concurrently with your main loop.  They share memory, so you must synchronise access to shared variables.

```c
static unsigned int bg_thread_id;
static volatile bool bg_done = false;

static void bg_worker(void) {
    // runs in parallel with plugin_start
    do_long_calculation();
    bg_done = true;
    rb->thread_exit();
}

// Stack memory — must stay valid for the thread's lifetime
static uint32_t bg_stack[DEFAULT_STACK_SIZE / sizeof(uint32_t)];

// Spawn the thread
bg_thread_id = rb->create_thread(bg_worker,
                                   bg_stack, sizeof(bg_stack),
                                   0, "myplugin bg"
                                   IF_PRIO(, PRIORITY_NORMAL)
                                   IF_COP(, CPU));

// Wait for it to finish
rb->thread_wait(bg_thread_id);
```

### Mutex (mutual exclusion — protecting shared data)

```c
static struct mutex my_mutex;
rb->mutex_init(&my_mutex);

// In thread A:
rb->mutex_lock(&my_mutex);
shared_data++;            // only one thread can be here at a time
rb->mutex_unlock(&my_mutex);
```

### Event queues

```c
static struct event_queue my_queue;
rb->queue_init(&my_queue, false);

// Post a message from anywhere:
#define MY_EVENT 1
rb->queue_post(&my_queue, MY_EVENT, 0 /* data */);

// Receive with 100 ms timeout:
struct queue_event ev;
rb->queue_wait_w_tmo(&my_queue, &ev, HZ / 10);
if (ev.id == MY_EVENT) { /* handle */ }

rb->queue_delete(&my_queue);   // always clean up
```

---

## Chapter 14 — USB HID keyboard (typing into the Mac)

When the iPod is connected via USB in HID mode, your plugin can send keystrokes to the host computer — the Mac receives them as if a real keyboard typed them.  This lets you build text expanders, macro pads, presentation clickers, or password typers.

> **How HID mode works:** Rockbox registers the iPod as a USB HID keyboard device.  The Mac sees it as a keyboard, so whatever keys you send land in whatever app has focus — a text editor, a terminal, a browser URL bar, anything.

### The API

```c
#ifdef USB_ENABLE_HID
// sends one key press (or release)
rb->usb_hid_send(usage_page_t usage_page, int key_id);
#endif
```

Always wrap HID code in `#ifdef USB_ENABLE_HID` — not all Rockbox targets compile with HID support, and without the guard your plugin won't build on those targets.

The two usage pages you'll use most:

| Usage page | What it controls |
|---|---|
| `HID_USAGE_PAGE_KEYBOARD_KEYPAD` | Individual keys (letters, F-keys, modifiers) |
| `HID_USAGE_PAGE_CONSUMER` | Media keys (play/pause, volume, next track) |

### Key press + release pattern

A single logical keystroke is two calls: press, then release.  Release is always `HID_KEYBOARD_RESERVED` (value `0x00`).

```c
// type the letter H
rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_H);
rb->sleep(2);   // 20 ms — give the OS time to register the press
rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_RESERVED);
rb->sleep(2);   // 20 ms gap before the next key
```

Without the release (`HID_KEYBOARD_RESERVED`), the OS thinks the key is still held down and will auto-repeat it.

### Available key constants

All defined in `firmware/usbstack/usb_hid_usage_tables.h` (included automatically via `plugin.h` when `USB_ENABLE_HID` is defined):

```c
// Letters
HID_KEYBOARD_A  through  HID_KEYBOARD_Z

// Digits
HID_KEYBOARD_0  through  HID_KEYBOARD_9

// Common punctuation / control
HID_KEYBOARD_SPACEBAR
HID_KEYBOARD_RETURN
HID_KEYBOARD_ESCAPE
HID_KEYBOARD_DELETE
HID_KEYBOARD_TAB

// Modifier keys (hold these while sending another key for combos)
HID_KEYBOARD_LEFT_SHIFT
HID_KEYBOARD_LEFT_CONTROL
HID_KEYBOARD_LEFT_GUI     // Command key on Mac

// Function keys
HID_KEYBOARD_F1  through  HID_KEYBOARD_F12

// Navigation
HID_KEYBOARD_PAGE_UP
HID_KEYBOARD_PAGE_DOWN
```

### Typing a full string

The key constants only cover lowercase letters.  For uppercase or symbols you need to hold Shift:

```c
// To type uppercase 'H': send Left Shift + H together, then release both
rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_LEFT_SHIFT);
rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_H);
rb->sleep(2);
rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_RESERVED);
rb->sleep(2);
```

A helper that types a lowercase ASCII string:

```c
static void type_string(const char *str) {
    // maps printable ASCII characters to HID key codes
    // HID_KEYBOARD_A = 0x04, letters are sequential after that
    // HID_KEYBOARD_1 = 0x1E, digits follow in order too
    for (int i = 0; str[i]; i++) {
        char c = str[i];
        int key = HID_KEYBOARD_RESERVED;   // default: nothing

        if (c >= 'a' && c <= 'z')
            key = HID_KEYBOARD_A + (c - 'a');   // sequential from A
        else if (c >= '1' && c <= '9')
            key = HID_KEYBOARD_1 + (c - '1');
        else if (c == '0')
            key = HID_KEYBOARD_0;
        else if (c == ' ')
            key = HID_KEYBOARD_SPACEBAR;
        else if (c == '\n')
            key = HID_KEYBOARD_RETURN;

        if (key != HID_KEYBOARD_RESERVED) {
            rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, key);
            rb->sleep(2);
            rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_RESERVED);
            rb->sleep(2);
        }
    }
}
```

> **C note:** `HID_KEYBOARD_A + (c - 'a')` works because both the ASCII alphabet and the HID key codes are sequential in the same order.  `c - 'a'` gives you the offset from 'a' (so 'b' → 1, 'c' → 2, …) and adding that to `HID_KEYBOARD_A` gives the matching HID code.

### Minimal "hello world" plugin

```c
#include "plugin.h"

#ifdef USB_ENABLE_HID

static void type_string(const char *str) {
    for (int i = 0; str[i]; i++) {
        char c = str[i];
        int key = HID_KEYBOARD_RESERVED;

        if (c >= 'a' && c <= 'z')      key = HID_KEYBOARD_A + (c - 'a');
        else if (c >= '1' && c <= '9') key = HID_KEYBOARD_1 + (c - '1');
        else if (c == '0')             key = HID_KEYBOARD_0;
        else if (c == ' ')             key = HID_KEYBOARD_SPACEBAR;
        else if (c == '\n')            key = HID_KEYBOARD_RETURN;

        if (key != HID_KEYBOARD_RESERVED) {
            rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, key);
            rb->sleep(2);
            rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_RESERVED);
            rb->sleep(2);
        }
    }
}

enum plugin_status plugin_start(const void *parameter) {
    (void)parameter;

    rb->lcd_set_background(LCD_BLACK);
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_clear_display();
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_putsxy(0, 0, "SELECT = type hello world");
    rb->lcd_putsxy(0, 12, "MENU   = quit");
    rb->lcd_update();

    while (true) {
        long btn = rb->button_get(true);
        switch (btn) {
            case BUTTON_SELECT:
                type_string("hello world\n");
                break;
            case BUTTON_MENU:
                return PLUGIN_OK;
            default:
                if (rb->default_event_handler(btn) == SYS_USB_CONNECTED)
                    return PLUGIN_USB_CONNECTED;
                break;
        }
    }
}

#else   /* no HID support on this target */

enum plugin_status plugin_start(const void *parameter) {
    (void)parameter;
    rb->splash(HZ * 2, "HID not supported on this target");
    return PLUGIN_OK;
}

#endif
```

### Media key example (consumer page)

The `HID_USAGE_PAGE_CONSUMER` page sends media events rather than key codes.  These control whatever media app the Mac has focused:

```c
// from apps/plugins/remote_control.c — the built-in remote control plugin
rb->usb_hid_send(HID_USAGE_PAGE_CONSUMER, HID_CONSUMER_PLAY);
rb->usb_hid_send(HID_USAGE_PAGE_CONSUMER, HID_CONSUMER_SCAN_NEXT_TRACK);
rb->usb_hid_send(HID_USAGE_PAGE_CONSUMER, HID_CONSUMER_VOLUME_INCREMENT);
```

Consumer events don't need an explicit release — they're one-shot signals.

> **Tip:** Rockbox ships a complete example at `apps/plugins/remote_control.c` — a presentation/desktop remote with menus for slide control, volume, and browser shortcuts.  It's the best real-world reference for HID plugin code.

---

## Chapter 15 — Complete example: "Dot Catcher"

A complete game tying together everything: screen, input, timing, menus, config, high scores, beep.

To build it:
1. Copy this into `apps/plugins/dotcatcher.c`
2. Add `dotcatcher.c` to `apps/plugins/SOURCES`
3. Add `dotcatcher,games` to `apps/plugins/CATEGORIES`
4. Run `make` in your build directory

```c
#include "plugin.h"
#include "lib/configfile.h"
#include "lib/highscore.h"
#include "lib/playback_control.h"

/* ---------- file paths ---------- */
#define CFG_FILE    PLUGIN_GAMES_DATA_DIR "/dotcatcher.cfg"
#define SCORE_FILE  PLUGIN_GAMES_DATA_DIR "/dotcatcher.score"
#define NUM_SCORES  5
#define CFG_VER     1

/* ---------- game state ---------- */
static int player_x, player_y;   /* player centre position */
static int dot_x, dot_y;         /* dot centre position */
static int score;
static bool running;
static struct highscore scores[NUM_SCORES];

/* ---------- config ---------- */
static int speed = 3;             /* 1 (slow) to 10 (fast) */

static struct configdata cfg[] = {
    { TYPE_INT, 1, 10, { .int_p = &speed }, "speed", NULL },
};

/* ---------- helper: place the dot randomly ---------- */
static void spawn_dot(void) {
    dot_x = rb->rand() % (LCD_WIDTH  - 16) + 8;
    dot_y = rb->rand() % (LCD_HEIGHT - 24) + 12;
}

/* ---------- draw everything ---------- */
static void draw(void) {
    rb->lcd_clear_display();

    /* dot — bright red square */
    rb->lcd_set_foreground(LCD_RGBPACK(255, 80, 80));
    rb->lcd_fillrect(dot_x - 4, dot_y - 4, 8, 8);

    /* player — cyan square */
    rb->lcd_set_foreground(LCD_RGBPACK(80, 200, 255));
    rb->lcd_fillrect(player_x - 6, player_y - 6, 12, 12);

    /* HUD text */
    rb->lcd_set_foreground(LCD_WHITE);
    rb->lcd_setfont(FONT_SYSFIXED);
    rb->lcd_putsxyf(0, 0, "Score: %d  Speed: %d", score, speed);

    rb->lcd_update();
}

/* ---------- main game loop ---------- */
static void run_game(void) {
    player_x = LCD_WIDTH  / 2;
    player_y = LCD_HEIGHT / 2;
    score    = 0;
    running  = true;

    rb->srand(*rb->current_tick);
    spawn_dot();

    while (running) {
        /* timeout drives frame rate: slower speed = longer wait = fewer fps */
        long btn = rb->button_get_w_tmo(HZ / (speed * 2 + 2));
        rb->reset_poweroff_timer();

        switch (btn) {
            case BUTTON_LEFT:                    player_x -= 8; break;
            case BUTTON_RIGHT:                   player_x += 8; break;
            case BUTTON_MENU:                    player_y -= 8; break;
            case BUTTON_PLAY:                    player_y += 8; break;
            case BUTTON_SCROLL_FWD:              player_x += 4; break;
            case BUTTON_SCROLL_BACK:             player_x -= 4; break;
            case BUTTON_SELECT | BUTTON_REPEAT:  running = false; break;
            default:
                if (rb->default_event_handler(btn) == SYS_USB_CONNECTED)
                    running = false;
                break;
        }

        /* clamp player inside screen */
        if (player_x < 6)           player_x = 6;
        if (player_x > LCD_WIDTH-6) player_x = LCD_WIDTH - 6;
        if (player_y < 6)           player_y = 6;
        if (player_y > LCD_HEIGHT-6) player_y = LCD_HEIGHT - 6;

        /* collision: catch dot if close enough */
        int dx = player_x - dot_x;
        int dy = player_y - dot_y;
        if (dx < 0) dx = -dx;   /* absolute value without using abs() */
        if (dy < 0) dy = -dy;
        if (dx < 10 && dy < 10) {
            score++;
            rb->beep_play(880 + score * 20, 60, 15000);
            spawn_dot();
        }

        draw();
    }
}

/* ---------- settings sub-menu ---------- */
static void settings_menu(void) {
    int sel = 0;
    bool done = false;

    MENUITEM_STRINGLIST(menu, "Settings", NULL,
                        "Speed",
                        "Playback Control",
                        "Back");

    while (!done) {
        switch (rb->do_menu(&menu, &sel, NULL, false)) {
            case 0:
                rb->set_int("Speed", "", UNIT_INT,
                            &speed, NULL, 1, 1, 10, NULL);
                configfile_save(CFG_FILE, cfg, ARRAYLEN(cfg), CFG_VER);
                break;
            case 1:
                playback_control(NULL);
                break;
            default:
                done = true;
                break;
        }
    }
}

/* ---------- plugin entry point ---------- */
enum plugin_status plugin_start(const void *parameter) {
    (void)parameter;

    /* load saved config and scores */
    configfile_load(CFG_FILE, cfg, ARRAYLEN(cfg), CFG_VER);
    highscore_load(SCORE_FILE, scores, NUM_SCORES);

    int sel = 0;
    bool quit = false;

    MENUITEM_STRINGLIST(main_menu, "Dot Catcher", NULL,
                        "Play",
                        "Settings",
                        "High Scores",
                        "Quit");

    while (!quit) {
        switch (rb->do_menu(&main_menu, &sel, NULL, false)) {
            case 0: {
                /* braces required: C can't jump over a variable declaration */
                run_game();
                int rank = highscore_update(score, 0, "---",
                                             scores, NUM_SCORES);
                highscore_save(SCORE_FILE, scores, NUM_SCORES);
                if (rank >= 0)
                    highscore_show(rank, scores, NUM_SCORES, false);
                break;
            }
            case 1:
                settings_menu();
                break;
            case 2:
                highscore_show(-1, scores, NUM_SCORES, false);
                break;
            default:
                quit = true;
                break;
        }
    }

    return PLUGIN_OK;
}
```

---

## Quick reference card

```
Screen:   320×240 px, (0,0) = top-left corner
Time:     HZ = 100 ticks/sec  |  *rb->current_tick = ticks elapsed
Buttons:  BUTTON_MENU, BUTTON_PLAY, BUTTON_LEFT, BUTTON_RIGHT, BUTTON_SELECT
          BUTTON_SCROLL_FWD (CW), BUTTON_SCROLL_BACK (CCW)
          | BUTTON_REPEAT = held   | BUTTON_REL = released
Fonts:    FONT_SYSFIXED (tiny, always available)  FONT_UI (user's font)
Colors:   LCD_RGBPACK(r,g,b)   LCD_WHITE   LCD_BLACK   LCD_RED ...
Files:    PLUGIN_APPS_DATA_DIR    PLUGIN_GAMES_DATA_DIR
USB:      if (rb->default_event_handler(btn) == SYS_USB_CONNECTED) return PLUGIN_USB_CONNECTED;
HID:      rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_A);  // key press
          rb->usb_hid_send(HID_USAGE_PAGE_KEYBOARD_KEYPAD, HID_KEYBOARD_RESERVED); // release
          rb->usb_hid_send(HID_USAGE_PAGE_CONSUMER, HID_CONSUMER_PLAY);  // media key (no release needed)
          Wrap all HID code in #ifdef USB_ENABLE_HID ... #endif
Timing:   rb->reset_poweroff_timer()   (call each frame to keep device awake)
Libs:     configfile.h   highscore.h   playback_control.h   pluginlib_actions.h
```
