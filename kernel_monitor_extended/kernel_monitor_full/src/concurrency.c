#define _POSIX_C_SOURCE 200809L
/*
 * concurrency.c  --  CO-6: Concurrency and Synchronization
 * -----------------------------------------------------------------------
 * Three demos:
 *   1. A background pthread sharing mutex-protected state with main()
 *      -- the thread equivalent of process control from CO-2.
 *   2. A deliberate race condition on an unprotected shared counter,
 *      then the same code made correct with a mutex -- makes the need
 *      for mutual exclusion visible rather than theoretical.
 *   3. A producer/consumer pair coordinated with counting semaphores
 *      instead of a mutex, showing thread coordination beyond simple
 *      exclusion.
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sched.h>
#include "../include/kernel_monitor.h"

/* usleep() was removed from POSIX.1-2008; nanosleep() is the portable
   replacement and needs no extra feature-test macro under C11. */
static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ---------- 1. Background monitor thread ---------- */
typedef struct {
    pthread_mutex_t lock;
    int sample_count;
    double last_value;
    int stop;
} shared_stats_t;

static void *monitor_thread_fn(void *arg)
{
    shared_stats_t *stats = (shared_stats_t *)arg;
    while (1) {
        pthread_mutex_lock(&stats->lock);
        if (stats->stop) { pthread_mutex_unlock(&stats->lock); break; }
        stats->sample_count++;
        stats->last_value = (double)rand() / RAND_MAX * 100.0; /* stand-in for a live reading */
        pthread_mutex_unlock(&stats->lock);
        sleep_ms(400);
    }
    return NULL;
}

void demo_background_monitor_thread(void)
{
    shared_stats_t stats = { PTHREAD_MUTEX_INITIALIZER, 0, 0.0, 0 };
    pthread_t tid;

    printf("\n--- Background Monitor Thread (mutex-protected shared state) ---\n");
    if (pthread_create(&tid, NULL, monitor_thread_fn, &stats) != 0) {
        perror("pthread_create"); return;
    }

    for (int i = 0; i < 5; i++) {
        sleep_ms(500);
        pthread_mutex_lock(&stats.lock);
        printf("[main] sample_count=%d  last_value=%.2f\n", stats.sample_count, stats.last_value);
        pthread_mutex_unlock(&stats.lock);
    }

    pthread_mutex_lock(&stats.lock);
    stats.stop = 1;
    pthread_mutex_unlock(&stats.lock);
    pthread_join(tid, NULL);
    pthread_mutex_destroy(&stats.lock);
    printf("Monitor thread joined cleanly.\n");
}

/* ---------- 2. Race condition vs. mutex-protected counter ---------- */
#define INCREMENTS 50000

static long g_counter = 0;
static pthread_mutex_t g_counter_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_use_lock = 0;

static void *increment_fn(void *arg)
{
    (void)arg;
    for (int i = 0; i < INCREMENTS; i++) {
        if (g_use_lock) {
            pthread_mutex_lock(&g_counter_lock);
            g_counter++;
            pthread_mutex_unlock(&g_counter_lock);
        } else {
            /* Unprotected read-modify-write, split into explicit steps
               with a yield in between. On a fast machine the two threads
               can otherwise finish their loops almost back-to-back with
               few real interleavings; the yield widens the window so the
               race actually manifests, which is the point of the demo. */
            long temp = g_counter;
            sched_yield();
            temp = temp + 1;
            g_counter = temp;
        }
    }
    return NULL;
}

static long run_two_threads(void)
{
    g_counter = 0;
    pthread_t t1, t2;
    pthread_create(&t1, NULL, increment_fn, NULL);
    pthread_create(&t2, NULL, increment_fn, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return g_counter;
}

void demo_race_condition(void)
{
    long expected = 2L * INCREMENTS;

    printf("\n--- Race Condition Demo (two threads, %d increments each) ---\n", INCREMENTS);

    g_use_lock = 0;
    long without_lock = run_two_threads();
    printf("WITHOUT mutex: final counter = %ld  (expected %ld) %s\n",
           without_lock, expected, without_lock == expected ? "" : "<-- lost updates!");

    g_use_lock = 1;
    long with_lock = run_two_threads();
    printf("WITH    mutex: final counter = %ld  (expected %ld) %s\n",
           with_lock, expected, with_lock == expected ? "correct" : "");

    printf(
        "Without the mutex, 'counter++' (read-modify-write) from two threads\n"
        "can interleave and overwrite each other's update -- a classic race\n"
        "condition. The mutex enforces mutual exclusion so each increment\n"
        "completes atomically.\n"
    );
}

/* ---------- 3. Producer/consumer via counting semaphores ---------- */
#define BUF_SIZE 5
#define ITEMS 8

static int g_buffer[BUF_SIZE];
static int g_in = 0, g_out = 0;
static sem_t g_empty_slots, g_filled_slots;
static pthread_mutex_t g_buf_lock = PTHREAD_MUTEX_INITIALIZER;

static void *producer_fn(void *arg)
{
    (void)arg;
    for (int i = 1; i <= ITEMS; i++) {
        sem_wait(&g_empty_slots);           /* wait for a free slot */
        pthread_mutex_lock(&g_buf_lock);
        g_buffer[g_in] = i;
        printf("[producer] produced %d at slot %d\n", i, g_in);
        g_in = (g_in + 1) % BUF_SIZE;
        pthread_mutex_unlock(&g_buf_lock);
        sem_post(&g_filled_slots);          /* signal a filled slot */
        sleep_ms(100);
    }
    return NULL;
}

static void *consumer_fn(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITEMS; i++) {
        sem_wait(&g_filled_slots);          /* wait for a filled slot */
        pthread_mutex_lock(&g_buf_lock);
        int item = g_buffer[g_out];
        printf("[consumer] consumed %d from slot %d\n", item, g_out);
        g_out = (g_out + 1) % BUF_SIZE;
        pthread_mutex_unlock(&g_buf_lock);
        sem_post(&g_empty_slots);           /* signal a free slot */
        sleep_ms(150);
    }
    return NULL;
}

void demo_producer_consumer(void)
{
    printf("\n--- Producer/Consumer Demo (counting semaphores, buffer size %d) ---\n", BUF_SIZE);

    sem_init(&g_empty_slots, 0, BUF_SIZE); /* starts with BUF_SIZE free slots */
    sem_init(&g_filled_slots, 0, 0);       /* starts with 0 items ready */
    g_in = g_out = 0;

    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer_fn, NULL);
    pthread_create(&cons, NULL, consumer_fn, NULL);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    sem_destroy(&g_empty_slots);
    sem_destroy(&g_filled_slots);

    printf(
        "The two semaphores coordinate the threads without busy-waiting:\n"
        "the producer blocks when the buffer is full, the consumer blocks\n"
        "when it's empty -- classic bounded-buffer synchronization.\n"
    );
}
