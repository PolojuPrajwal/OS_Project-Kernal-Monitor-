#define _POSIX_C_SOURCE 200809L
/*
 * system_info.c  --  CO-1: The OS as a Service Layer
 * -----------------------------------------------------------------------
 * Every function here asks the kernel -- through /proc -- for information
 * a user-space program cannot obtain any other way: uptime, CPU identity,
 * memory totals, and instantaneous CPU utilization (via delta sampling
 * of /proc/stat, since the kernel only exposes cumulative counters).
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/kernel_monitor.h"

typedef struct {
    long long user, nice, system, idle, iowait, irq, softirq, steal;
} cpu_times_t;

static int read_cpu_times(cpu_times_t *t)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) { perror("fopen(/proc/stat)"); return -1; }

    char label[16];
    int n = fscanf(fp, "%15s %lld %lld %lld %lld %lld %lld %lld %lld",
                   label, &t->user, &t->nice, &t->system, &t->idle,
                   &t->iowait, &t->irq, &t->softirq, &t->steal);
    fclose(fp);
    if (n < 9) { fprintf(stderr, "Unexpected format in /proc/stat\n"); return -1; }
    return 0;
}

void show_system_uptime(void)
{
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp) { perror("fopen(/proc/uptime)"); return; }

    double uptime_sec, idle_sec;
    if (fscanf(fp, "%lf %lf", &uptime_sec, &idle_sec) != 2) {
        fprintf(stderr, "Failed to parse /proc/uptime\n");
        fclose(fp);
        return;
    }
    fclose(fp);

    long days  = (long)uptime_sec / 86400;
    long hours = ((long)uptime_sec % 86400) / 3600;
    long mins  = ((long)uptime_sec % 3600) / 60;
    long secs  = (long)uptime_sec % 60;

    printf("\n--- System Uptime (CO-1: kernel-exposed state) ---\n");
    printf("Uptime : %ld d %ld h %ld m %ld s\n", days, hours, mins, secs);
    printf("Idle   : %.2f seconds (cumulative, all cores)\n", idle_sec);
}

void show_cpu_info(void)
{
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) { perror("fopen(/proc/cpuinfo)"); return; }

    char line[256];
    char model_name[192] = "Unknown";
    int core_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                strncpy(model_name, colon + 2, sizeof(model_name) - 1);
                model_name[strcspn(model_name, "\n")] = '\0';
            }
        }
        if (strncmp(line, "processor", 9) == 0) core_count++;
    }
    fclose(fp);

    printf("\n--- CPU Info ---\n");
    printf("Model        : %s\n", model_name);
    printf("Logical cores: %d\n", core_count);
}

void show_memory_info(void)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) { perror("fopen(/proc/meminfo)"); return; }

    char key[64]; long value; char unit[16];
    long mem_total = 0, mem_free = 0, mem_available = 0, buffers = 0, cached = 0;

    while (fscanf(fp, "%63s %ld %15s", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0)          mem_total = value;
        else if (strcmp(key, "MemFree:") == 0)      mem_free = value;
        else if (strcmp(key, "MemAvailable:") == 0) mem_available = value;
        else if (strcmp(key, "Buffers:") == 0)      buffers = value;
        else if (strcmp(key, "Cached:") == 0)        cached = value;
    }
    fclose(fp);

    long used = mem_total - mem_free - buffers - cached;
    double used_pct = mem_total ? (100.0 * used / mem_total) : 0.0;

    printf("\n--- Memory Info ---\n");
    printf("Total     : %8ld MB\n", mem_total / 1024);
    printf("Used      : %8ld MB (%.1f%%)\n", used / 1024, used_pct);
    printf("Free      : %8ld MB\n", mem_free / 1024);
    printf("Available : %8ld MB\n", mem_available / 1024);
    printf("Buffers   : %8ld MB\n", buffers / 1024);
    printf("Cached    : %8ld MB\n", cached / 1024);
}

void show_cpu_usage_live(int seconds, int interval)
{
    if (interval <= 0) interval = 1;
    int rounds = seconds / interval;
    if (rounds <= 0) rounds = 1;

    printf("\n--- Live CPU Usage (%d readings, %ds apart) ---\n", rounds, interval);

    cpu_times_t prev, curr;
    if (read_cpu_times(&prev) != 0) return;

    for (int i = 0; i < rounds; i++) {
        sleep(interval);
        if (read_cpu_times(&curr) != 0) return;

        long long prev_idle = prev.idle + prev.iowait;
        long long curr_idle = curr.idle + curr.iowait;
        long long prev_non_idle = prev.user + prev.nice + prev.system + prev.irq + prev.softirq + prev.steal;
        long long curr_non_idle = curr.user + curr.nice + curr.system + curr.irq + curr.softirq + curr.steal;
        long long total_delta = (curr_idle + curr_non_idle) - (prev_idle + prev_non_idle);
        long long idle_delta  = curr_idle - prev_idle;

        double usage_pct = total_delta ? (100.0 * (total_delta - idle_delta) / total_delta) : 0.0;
        printf("[%2d] CPU usage: %5.1f%%\n", i + 1, usage_pct);
        prev = curr;
    }
}
