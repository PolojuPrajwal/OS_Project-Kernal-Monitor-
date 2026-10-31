#define _POSIX_C_SOURCE 200809L
/*
 * process_info.c  --  CO-2: Processes and Process Control
 * -----------------------------------------------------------------------
 * Every numeric folder under /proc is a live process abstraction.
 * Walking them with opendir()/readdir() demonstrates enumeration; parsing
 * each one's /proc/[pid]/status demonstrates lifecycle state (R/S/D/Z/T).
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include "../include/kernel_monitor.h"

static int is_numeric(const char *s)
{
    if (!s || !*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static void read_status_fields(pid_t pid, char *name, size_t name_len,
                                char *state, size_t state_len, long *vm_rss_kb)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");
    strncpy(name, "?", name_len);
    strncpy(state, "?", state_len);
    *vm_rss_kb = 0;
    if (!fp) return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Name:", 5) == 0) {
            sscanf(line + 5, "%255s", name);
            name[name_len - 1] = '\0';
        } else if (strncmp(line, "State:", 6) == 0) {
            sscanf(line + 6, "%15s", state);
            state[state_len - 1] = '\0';
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", vm_rss_kb);
        }
    }
    fclose(fp);
}

void list_all_processes(void)
{
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) { perror("opendir(/proc)"); return; }

    printf("\n%-8s %-20s %-6s %10s\n", "PID", "NAME", "STATE", "RSS(KB)");
    printf("---------------------------------------------------\n");

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(proc_dir)) != NULL) {
        if (!is_numeric(entry->d_name)) continue;
        pid_t pid = (pid_t)atoi(entry->d_name);

        char name[256], state[16]; long vm_rss_kb;
        read_status_fields(pid, name, sizeof(name), state, sizeof(state), &vm_rss_kb);
        printf("%-8d %-20s %-6s %10ld\n", pid, name, state, vm_rss_kb);
        count++;
    }
    closedir(proc_dir);

    printf("---------------------------------------------------\n");
    printf("Total processes: %d\n", count);
}

void show_process_details(pid_t pid)
{
    char status_path[64], stat_path[64], cmd_path[64];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
    snprintf(cmd_path, sizeof(cmd_path), "/proc/%d/cmdline", pid);

    FILE *fp = fopen(status_path, "r");
    if (!fp) { printf("No such process (PID %d), or /proc/%d unreadable.\n", pid, pid); return; }

    printf("\n--- Details for PID %d ---\n", pid);
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Name:", 5) == 0 || strncmp(line, "State:", 6) == 0 ||
            strncmp(line, "PPid:", 5) == 0 || strncmp(line, "Threads:", 8) == 0 ||
            strncmp(line, "VmRSS:", 6) == 0 || strncmp(line, "VmSize:", 7) == 0) {
            printf("%s", line);
        }
    }
    fclose(fp);

    fp = fopen(cmd_path, "r");
    if (fp) {
        char cmd[512];
        size_t n = fread(cmd, 1, sizeof(cmd) - 1, fp);
        fclose(fp);
        for (size_t i = 0; i < n; i++) if (cmd[i] == '\0') cmd[i] = ' ';
        cmd[n] = '\0';
        printf("Cmdline: %s\n", n ? cmd : "(kernel thread / none)");
    }

    fp = fopen(stat_path, "r");
    if (fp) {
        char buf[512];
        if (fgets(buf, sizeof(buf), fp)) {
            char *close_paren = strrchr(buf, ')');
            if (close_paren) {
                long utime = 0, stime = 0;
                sscanf(close_paren + 1,
                       " %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %ld %ld",
                       &utime, &stime);
                long ticks_per_sec = sysconf(_SC_CLK_TCK);
                printf("CPU time: %.2fs user, %.2fs system\n",
                       (double)utime / ticks_per_sec, (double)stime / ticks_per_sec);
            }
        }
        fclose(fp);
    }
}
