#ifndef KERNEL_MONITOR_H
#define KERNEL_MONITOR_H

#include <sys/types.h>

/* ===================== CO-1: system_info.c =====================
 * OS as a service layer -- system-wide info read through /proc.
 */
void show_system_uptime(void);
void show_cpu_info(void);
void show_memory_info(void);
void show_cpu_usage_live(int seconds, int interval);

/* ===================== CO-2: process_info.c =====================
 * Process abstraction, lifecycle, and enumeration.
 */
void list_all_processes(void);
void show_process_details(pid_t pid);

/* ===================== CO-2: process_control.c =====================
 * Process termination / signaling (also touches CO-3's signal model).
 */
void terminate_process(pid_t pid);
void send_custom_signal(pid_t pid, int signum);

/* ===================== CO-3: ipc_demo.c =====================
 * Anonymous pipes, named pipes (FIFOs), signal handlers,
 * process groups / sessions / job control.
 */
void demo_anonymous_pipe(void);
void demo_named_pipe(void);
void demo_signal_handler(void);
void show_process_group_info(void);

/* ===================== CO-4: memory_mgmt.c =====================
 * Virtual memory, address space layout, page faults, demand paging,
 * copy-on-write.
 */
void show_address_space(pid_t pid);
void show_page_fault_counts(pid_t pid);
void demo_demand_paging_and_cow(void);

/* ===================== CO-5: file_io.c =====================
 * File descriptors, the Unix file abstraction (inodes), raw vs
 * buffered I/O, memory-mapped file I/O.
 */
void list_open_fds(pid_t pid);
void show_inode_info(const char *path);
void demo_raw_io(void);
void demo_mmap_read(const char *path);

/* ===================== CO-6: concurrency.c =====================
 * POSIX threads, mutexes, race conditions, semaphores.
 */
void demo_background_monitor_thread(void);
void demo_race_condition(void);
void demo_producer_consumer(void);

#endif /* KERNEL_MONITOR_H */
