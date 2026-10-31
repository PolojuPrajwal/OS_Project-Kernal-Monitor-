#define _POSIX_C_SOURCE 200809L
/*
 * memory_mgmt.c  --  CO-4: Memory Management
 * -----------------------------------------------------------------------
 * Three ideas made visible:
 *   1. Virtual address space layout -- /proc/[pid]/maps lists every
 *      mapped segment (text, heap, stack, shared libraries) with its
 *      permissions, exactly as the kernel's page tables see it.
 *   2. Page faults & demand paging -- /proc/[pid]/stat exposes minor and
 *      major fault counts; touching freshly malloc'd memory for the first
 *      time triggers minor faults as the kernel backs virtual pages with
 *      real physical frames on demand.
 *   3. Copy-on-write -- after fork(), parent and child share the same
 *      physical pages until either writes; only then does the kernel
 *      duplicate the page the write touched.
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../include/kernel_monitor.h"

void show_address_space(pid_t pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) { printf("Could not open %s: process may not exist.\n", path); return; }

    printf("\n--- Virtual Address Space for PID %d (/proc/%d/maps) ---\n", pid, pid);
    printf("%-34s %-5s %s\n", "ADDRESS RANGE", "PERMS", "MAPPING");

    char line[512];
    int shown = 0;
    while (fgets(line, sizeof(line), fp) && shown < 20) {
        char addr[40], perms[8], path_field[256] = "";
        sscanf(line, "%39s %7s %*s %*s %*s %255[^\n]", addr, perms, path_field);
        printf("%-34s %-5s %s\n", addr, perms, path_field);
        shown++;
    }
    fclose(fp);
    printf("(showing first %d mapped segments)\n", shown);
}

static long read_fault_field(pid_t pid, int nth_token_after_paren)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char buf[512];
    long value = -1;
    if (fgets(buf, sizeof(buf), fp)) {
        char *close_paren = strrchr(buf, ')');
        if (close_paren) {
            char *tok = strtok(close_paren + 1, " ");
            for (int i = 1; tok; i++, tok = strtok(NULL, " ")) {
                if (i == nth_token_after_paren) { value = atol(tok); break; }
            }
        }
    }
    fclose(fp);
    return value;
}

void show_page_fault_counts(pid_t pid)
{
    /* Token positions after the ")" in /proc/[pid]/stat:
       1=state 2=ppid 3=pgrp 4=session 5=tty_nr 6=tpgid 7=flags
       8=minflt 9=cminflt 10=majflt 11=cmajflt                 */
    long minflt = read_fault_field(pid, 8);
    long majflt = read_fault_field(pid, 10);

    if (minflt < 0) { printf("Could not read fault counts for PID %d.\n", pid); return; }

    printf("\n--- Page Faults for PID %d (demand paging in action) ---\n", pid);
    printf("Minor faults: %ld  (page already in RAM, just needs mapping -- e.g. first touch of a malloc'd page)\n", minflt);
    printf("Major faults: %ld  (page had to be fetched from disk -- far more expensive)\n", majflt);
}

void demo_demand_paging_and_cow(void)
{
    const size_t SIZE = 20 * 1024 * 1024; /* 20 MB */
    const size_t PAGE = 4096;
    pid_t self = getpid();

    printf("\n--- Demand Paging + Copy-on-Write Demo ---\n");

    char *buf = malloc(SIZE);
    if (!buf) { perror("malloc"); return; }

    long minflt_before = read_fault_field(self, 8);
    printf("Allocated %zu MB with malloc() -- virtual address reserved,\n", SIZE / (1024 * 1024));
    printf("but no physical RAM backs it yet (minor faults so far: %ld).\n", minflt_before);

    /* Touch one byte per page: each first touch triggers a minor fault
       as the kernel maps a real physical frame on demand. */
    for (size_t i = 0; i < SIZE; i += PAGE) buf[i] = 1;

    long minflt_after = read_fault_field(self, 8);
    printf("After touching every page once: minor faults = %ld (+%ld)\n",
           minflt_after, minflt_after - minflt_before);
    printf("That jump IS demand paging: pages were backed by real RAM only\n");
    printf("when first accessed, not when malloc() was called.\n");

    buf[0] = 'P'; /* parent's original value */

    /* Flush before fork(): otherwise the child inherits a COPY of any
       buffered-but-not-yet-written stdout content, and whichever of
       parent/child flushes first will duplicate it. This is a classic
       fork()+stdio pitfall in its own right. */
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); free(buf); return; }

    if (pid == 0) {
        /* Child: still sees the parent's data -- same physical pages,
           shared read-only since fork(). Writing now triggers COW. */
        printf("[child ] Before writing, buf[0] = '%c' (shared page from parent)\n", buf[0]);
        buf[0] = 'C'; /* this write forces the kernel to copy this page */
        printf("[child ] After writing,  buf[0] = '%c' (now a private copy)\n", buf[0]);
        /* _exit() skips stdio flushing -- without this, buffered output
           is silently lost whenever stdout isn't line-buffered (e.g. when
           piped). Flushing explicitly avoids duplicating the parent's own
           still-pending buffer, which a plain exit() here could do. */
        fflush(stdout);
        _exit(0);
    } else {
        waitpid(pid, NULL, 0);
        printf("[parent] After child exited, buf[0] = '%c' (unchanged -- COW isolated the write)\n", buf[0]);
    }

    free(buf);
}
