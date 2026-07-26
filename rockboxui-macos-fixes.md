# Rockbox Simulator on macOS — Issues & Fixes Timeline

## Issue 1: Thread creation failing

**Symptom**
```
Thread creation failed. Retrying
make_context(): Operation not permitted
```
The simulator process started but immediately spammed this error and never launched.

**Root cause**
`firmware/asm/thread-unix.c` used the *signal stack trick* (GNU Pth variant) to initialize cooperative thread contexts. It works by:
1. Setting up an alternate signal stack with `sigaltstack`
2. Sending `SIGUSR1` to itself with `pthread_kill`
3. Capturing the new stack pointer in the signal handler via `setjmp`/`longjmp`

On macOS 12+ (Monterey and later), the OS blocks `sigaltstack` with `EPERM` in this context, making the whole mechanism fail.

**Fix**
Rewrote `thread-unix.c` entirely. Replaced the signal-stack trick with **pthreads + condvars**. Each Rockbox "thread" became a real `pthread` that blocks on a per-context semaphore when not scheduled. Only one Rockbox thread runs at a time, preserving the original cooperative multithreading model.

---

## Issue 2: Process spawns but no UI window appears

**Symptom**
Running `./rockboxui` showed the process in Activity Monitor but no window appeared on screen.

**Root cause**
macOS requires **all SDL window creation and event pumping to happen on the main OS thread**. In the original single-OS-thread cooperative implementation, this was automatic — everything ran on the main thread. With our new pthread-based approach, Rockbox "threads" were real pthreads, so when the Rockbox scheduler swapped out the main OS thread (via `swap_context`), it blocked it on a `pthread_cond_wait`. The main thread was therefore stuck sleeping, never pumping Cocoa/SDL events, so the window never appeared.

The existing macOS path in `system-sdl.c` already accounted for this by using `SDL_AddEventWatch` instead of a separate event thread — but it assumed the main OS thread would remain free to process events, which our new threading broke.

**Fix**
Added an SDL pump hook mechanism:
- `thread_unix_set_sdl_pump(fn)` in `thread-unix.c` registers a callback and records the main OS thread's `pthread_t`
- In `swap_context`, when the main OS thread is about to sleep, it uses `pthread_cond_timedwait` with a **16ms timeout** instead of blocking indefinitely
- After each timeout it calls `SDL_PumpEvents()`, which lets macOS deliver Cocoa window and input events
- `system-sdl.c` registers `SDL_PumpEvents` as the hook on macOS during `system_init()`

---

## Issue 3: UI appears but clicking freezes the program

**Symptom**
The simulator window appeared and rendered correctly, but any mouse click or keypress caused the program to freeze permanently.

**Root cause**
A **lost-wakeup race condition** in `swap_context`. The sequence was:

1. Thread A (old) wakes Thread B (new) by signalling its condvar, then tries to go to sleep
2. Between A's signal and A locking its own mutex, Thread B runs to completion and tries to wake A back
3. B sets `old->active = true` and signals A's condvar — but A hasn't started waiting yet
4. A then locks its mutex, sets `old->active = false` (overwriting B's write), and waits
5. The signal is gone, nobody will ever wake A again → **permanent deadlock**

**Fix**
Replaced `bool active` with an `int wake_count` (semaphore counter) in each `ctx`. The new logic:
- **`ctx_wake(c)`**: locks mutex, increments `wake_count`, signals condvar, unlocks
- **`ctx_wait(c)`**: locks mutex, waits while `wake_count == 0`, decrements `wake_count`, unlocks

Because the mutex is held when checking the count, a wakeup posted before the wait is never lost — the increment is visible when the thread eventually checks. `swap_context` became simply `ctx_wake(new)` + `ctx_wait(old)`.

---

## Issue 4: Segfault when browsing Demos / Apps

**Symptom**
```
zsh: segmentation fault  ./rockboxui
```
Crashed immediately upon navigating to the Demos or Apps section of the file browser.

**Two separate causes:**

**4a. `make` not recompiling threading changes**
`thread-unix.c` is `#include`d directly into `firmware/kernel/thread.c`, not compiled as its own translation unit. So changes to `thread-unix.c` don't trigger a rebuild via normal dependency tracking. The latest `swap_context` fix was never actually built.

Fix: `touch ../firmware/kernel/thread.c` to force a rebuild.

**4b. Missing simdisk**
The simulator's file browser looks for plugins and assets under a `simdisk/.rockbox/` directory relative to where it's launched. This directory was empty — the `.rock` plugin files, themes, and fonts had never been installed there.

Fix: `make install` populates `simdisk/.rockbox/` from the build output.

**Full command that resolved both:**
```sh
cd ~/Desktop/rockbox/build-sim
touch ../firmware/kernel/thread.c
make -j$(sysctl -n hw.logicalcpu) && make install && ./rockboxui
```

---

## Files modified

| File | Change |
|------|--------|
| `firmware/asm/thread-unix.c` | Full rewrite: signal-stack → pthreads + semaphore condvars + macOS SDL pump hook |
| `firmware/target/hosted/sdl/system-sdl.c` | macOS path: register `SDL_PumpEvents` as the idle hook via `thread_unix_set_sdl_pump` |
