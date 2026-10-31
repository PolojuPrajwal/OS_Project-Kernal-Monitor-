/*
 * main.c  --  Kernel Monitor (Extended Edition)
 * -----------------------------------------------------------------------
 * A user-space Linux systems-programming tool that deliberately exercises
 * every course outcome of Operating Systems and Systems Programming:
 *   CO-1  The OS as a Service Layer       (system_info.c)
 *   CO-2  Processes and Process Control    (process_info.c, process_control.c)
 *   CO-3  Inter-Process Communication      (ipc_demo.c)
 *   CO-4  Memory Management                (memory_mgmt.c)
 *   CO-5  File Systems & File I/O          (file_io.c)
 *   CO-6  Concurrency and Synchronization  (concurrency.c)
 * main.c itself only handles the menu and dispatch -- every actual OS
 * interaction lives in the module for its course outcome.
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/kernel_monitor.h"

static int read_int(long *out)
{
    char buf[64];
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    char *endptr;
    long val = strtol(buf, &endptr, 10);
    if (endptr == buf) return 0;
    *out = val;
    return 1;
}

static long ask_pid(const char *prompt)
{
    long pid = -1;
    printf("%s", prompt);
    if (!read_int(&pid)) return -1;
    return pid;
}

static void print_main_menu(void)
{
    printf("\n=============== KERNEL MONITOR (Extended) ===============\n");
    printf(" 1. CO-1: OS & System Info\n");
    printf(" 2. CO-2: Process Management\n");
    printf(" 3. CO-3: Inter-Process Communication\n");
    printf(" 4. CO-4: Memory Management\n");
    printf(" 5. CO-5: File Systems & I/O\n");
    printf(" 6. CO-6: Concurrency & Synchronization\n");
    printf(" 0. Exit\n");
    printf("===========================================================\n");
    printf("Choice: ");
}

static void menu_co1(void)
{
    long choice;
    while (1) {
        printf("\n--- CO-1: OS & System Info ---\n");
        printf(" 1. System uptime\n 2. CPU info\n 3. Memory info\n");
        printf(" 4. Live CPU usage (5 readings)\n 0. Back\nChoice: ");
        if (!read_int(&choice)) continue;
        switch (choice) {
            case 1: show_system_uptime(); break;
            case 2: show_cpu_info(); break;
            case 3: show_memory_info(); break;
            case 4: show_cpu_usage_live(5, 1); break;
            case 0: return;
            default: printf("Unknown choice.\n");
        }
    }
}

static void menu_co2(void)
{
    long choice, pid, sig;
    while (1) {
        printf("\n--- CO-2: Process Management ---\n");
        printf(" 1. List all processes\n 2. Show process details (by PID)\n");
        printf(" 3. Terminate a process (SIGTERM)\n 4. Send a custom signal\n");
        printf(" 5. Show this program's own PID (getpid)\n 0. Back\nChoice: ");
        if (!read_int(&choice)) continue;
        switch (choice) {
            case 1: list_all_processes(); break;
            case 2: pid = ask_pid("Enter PID: "); if (pid >= 0) show_process_details((pid_t)pid); break;
            case 3: pid = ask_pid("Enter PID to terminate: "); if (pid >= 0) terminate_process((pid_t)pid); break;
            case 4:
                pid = ask_pid("Enter PID: ");
                if (pid < 0) break;
                sig = -1; printf("Enter signal number (9=SIGKILL, 15=SIGTERM, 19=SIGSTOP): ");
                if (read_int(&sig)) send_custom_signal((pid_t)pid, (int)sig);
                break;
            case 5: printf("This process's PID: %d\n", getpid()); break;
            case 0: return;
            default: printf("Unknown choice.\n");
        }
    }
}

static void menu_co3(void)
{
    long choice;
    while (1) {
        printf("\n--- CO-3: Inter-Process Communication ---\n");
        printf(" 1. Anonymous pipe demo (fork + pipe)\n");
        printf(" 2. Named pipe / FIFO demo (mkfifo)\n");
        printf(" 3. Signal handler demo (sigaction + SIGUSR1)\n");
        printf(" 4. Show process group / session info\n 0. Back\nChoice: ");
        if (!read_int(&choice)) continue;
        switch (choice) {
            case 1: demo_anonymous_pipe(); break;
            case 2: demo_named_pipe(); break;
            case 3: demo_signal_handler(); break;
            case 4: show_process_group_info(); break;
            case 0: return;
            default: printf("Unknown choice.\n");
        }
    }
}

static void menu_co4(void)
{
    long choice, pid;
    while (1) {
        printf("\n--- CO-4: Memory Management ---\n");
        printf(" 1. Show address space of a PID (/proc/[pid]/maps)\n");
        printf(" 2. Show page fault counts of a PID\n");
        printf(" 3. Demand paging + copy-on-write demo\n 0. Back\nChoice: ");
        if (!read_int(&choice)) continue;
        switch (choice) {
            case 1: pid = ask_pid("Enter PID (or your own via CO-2 > 5): "); if (pid >= 0) show_address_space((pid_t)pid); break;
            case 2: pid = ask_pid("Enter PID: "); if (pid >= 0) show_page_fault_counts((pid_t)pid); break;
            case 3: demo_demand_paging_and_cow(); break;
            case 0: return;
            default: printf("Unknown choice.\n");
        }
    }
}

static void menu_co5(void)
{
    long choice, pid;
    char path[256];
    while (1) {
        printf("\n--- CO-5: File Systems & I/O ---\n");
        printf(" 1. List open file descriptors of a PID\n");
        printf(" 2. Show inode info for a file\n");
        printf(" 3. Raw open()/read()/write()/close() demo\n");
        printf(" 4. Memory-mapped file read demo (mmap)\n 0. Back\nChoice: ");
        if (!read_int(&choice)) continue;
        switch (choice) {
            case 1: pid = ask_pid("Enter PID: "); if (pid >= 0) list_open_fds((pid_t)pid); break;
            case 2:
                printf("Enter file path: ");
                if (fgets(path, sizeof(path), stdin)) {
                    path[strcspn(path, "\n")] = '\0';
                    show_inode_info(path);
                }
                break;
            case 3: demo_raw_io(); break;
            case 4:
                printf("Enter file path to mmap: ");
                if (fgets(path, sizeof(path), stdin)) {
                    path[strcspn(path, "\n")] = '\0';
                    demo_mmap_read(path);
                }
                break;
            case 0: return;
            default: printf("Unknown choice.\n");
        }
    }
}

static void menu_co6(void)
{
    long choice;
    while (1) {
        printf("\n--- CO-6: Concurrency & Synchronization ---\n");
        printf(" 1. Background monitor thread (pthread + mutex)\n");
        printf(" 2. Race condition demo (with/without mutex)\n");
        printf(" 3. Producer/consumer demo (semaphores)\n 0. Back\nChoice: ");
        if (!read_int(&choice)) continue;
        switch (choice) {
            case 1: demo_background_monitor_thread(); break;
            case 2: demo_race_condition(); break;
            case 3: demo_producer_consumer(); break;
            case 0: return;
            default: printf("Unknown choice.\n");
        }
    }
}

int main(void)
{
    long choice;
    printf("Kernel Monitor (Extended) started. PID of this process: %d\n", getpid());

    while (1) {
        print_main_menu();
        if (!read_int(&choice)) { printf("Invalid input.\n"); continue; }
        switch (choice) {
            case 1: menu_co1(); break;
            case 2: menu_co2(); break;
            case 3: menu_co3(); break;
            case 4: menu_co4(); break;
            case 5: menu_co5(); break;
            case 6: menu_co6(); break;
            case 0: printf("Exiting Kernel Monitor.\n"); return EXIT_SUCCESS;
            default: printf("Unknown choice.\n");
        }
    }
}
