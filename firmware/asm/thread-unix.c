/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2011 by Thomas Martitz
 *
 * Generic unix threading support
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include "debug.h"

static void (*sdl_pump_hook)(void) = NULL;
static pthread_t main_os_thread;

void thread_unix_set_sdl_pump(void (*hook)(void))
{
    main_os_thread = pthread_self();
    sdl_pump_hook = hook;
}

/* Cooperative threading via pthreads + condvars.
 *
 * Each Rockbox "thread" is a real pthread that blocks on a per-ctx semaphore
 * (wake_count + condvar) when not scheduled. Only one Rockbox thread runs at
 * a time — this mimics the original single-OS-thread cooperative model.
 *
 * Using a counter (wake_count) instead of a bool avoids the lost-wakeup race:
 * if the new thread runs and tries to wake old before old has gone to sleep,
 * the increment is visible when old eventually checks the count.
 */

static struct ctx {
    pthread_t       thread;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             wake_count;
} thread_bufs[MAXTHREADS];

static threadbit_t free_thread_bufs;
static struct ctx *target_context;

static void init_thread_bufs(void)
{
    for (unsigned int i = 0; i < MAXTHREADS; i++)
        threadbit_set_bit(&free_thread_bufs, i);
}

static struct ctx *alloc_thread_buf(void)
{
    unsigned int idx = threadbit_ffs(&free_thread_bufs);
    threadbit_clear_bit(&free_thread_bufs, idx);
    struct ctx *c = &thread_bufs[idx];
    pthread_mutex_init(&c->mutex, NULL);
    pthread_cond_init(&c->cond, NULL);
    c->wake_count = 0;
    return c;
}

static void free_thread_buf(struct ctx *c) __attribute__((unused));
static void free_thread_buf(struct ctx *c)
{
    unsigned int idx = c - thread_bufs;
    pthread_cond_destroy(&c->cond);
    pthread_mutex_destroy(&c->mutex);
    threadbit_set_bit(&free_thread_bufs, idx);
}

/* Post one wakeup token — safe to call before the thread is waiting. */
static void ctx_wake(struct ctx *c)
{
    pthread_mutex_lock(&c->mutex);
    c->wake_count++;
    pthread_cond_signal(&c->cond);
    pthread_mutex_unlock(&c->mutex);
}

/* Consume one wakeup token, blocking until one is available.
 * On macOS main thread: uses a timed wait so SDL events are pumped
 * periodically while idle. */
static void ctx_wait(struct ctx *c)
{
    pthread_mutex_lock(&c->mutex);
    while (c->wake_count == 0) {
#ifdef __APPLE__
        if (sdl_pump_hook && pthread_equal(pthread_self(), main_os_thread)) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 16000000; /* 16ms ~ 60fps */
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_cond_timedwait(&c->cond, &c->mutex, &ts);
            pthread_mutex_unlock(&c->mutex);
            sdl_pump_hook();
            pthread_mutex_lock(&c->mutex);
            continue;
        }
#endif
        pthread_cond_wait(&c->cond, &c->mutex);
    }
    c->wake_count--;
    pthread_mutex_unlock(&c->mutex);
}

struct thread_args {
    void (*f)(void);
    struct ctx *ctx;
};

static void *thread_runner(void *arg)
{
    struct thread_args *a = (struct thread_args *)arg;
    void (*f)(void) = a->f;
    struct ctx *c   = a->ctx;
    free(a);
    ctx_wait(c);
    f();
    return NULL;
}

static int make_context(struct ctx *ctx, void (*f)(void), char *sp, size_t stack_size)
{
    (void)sp; (void)stack_size;

    struct thread_args *args = malloc(sizeof(*args));
    if (!args) return false;
    args->f   = f;
    args->ctx = ctx;

    int ret = pthread_create(&ctx->thread, NULL, thread_runner, args);
    if (ret != 0) {
        free(args);
        DEBUGF("%s(): %s\n", __func__, strerror(ret));
        return false;
    }
    pthread_detach(ctx->thread);
    return true;
}

static inline void set_context(struct ctx *c)
{
    ctx_wake(c);
}

static inline void swap_context(struct ctx *old, struct ctx *new)
{
    ctx_wake(new);
    if (old)
        ctx_wait(old);
}

static inline void get_context(struct ctx *c)
{
    (void)c; /* main thread is already running; wake_count=0 is correct */
}

static void setup_thread(struct regs *context);

#define INIT_MAIN_THREAD
static void init_main_thread(void *addr)
{
    struct regs *context = (struct regs *)addr;
    init_thread_bufs();
    context->uc = alloc_thread_buf();
    get_context(context->uc);
}

#define THREAD_STARTUP_INIT(core, thread, function) \
    ({ (thread)->context.stack_size = (thread)->stack_size, \
       (thread)->context.stack = (uintptr_t)(thread)->stack; \
       (thread)->context.start = function; })

static void setup_thread(struct regs *context)
{
    void (*fn)(void) = context->start;
    context->uc = alloc_thread_buf();
    while (!make_context(context->uc, fn, (char*)context->stack, context->stack_size))
        DEBUGF("Thread creation failed. Retrying");
}

static inline void store_context(void *addr)
{
    struct regs *r = (struct regs *)addr;
    target_context = r->uc;
}

static inline void load_context(const void *addr)
{
    struct regs *r = (struct regs *)addr;
    if (UNLIKELY(r->start)) {
        setup_thread(r);
        r->start = NULL;
    }
    swap_context(target_context, r->uc);
    target_context = NULL;
}
