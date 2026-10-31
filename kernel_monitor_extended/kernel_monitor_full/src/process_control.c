#define _POSIX_C_SOURCE 200809L
/*
 * process_control.c  --  CO-2 (termination) / CO-3 (signals as IPC)
 * -----------------------------------------------------------------------
 * kill() is the single syscall behind both features below. Despite its
 * name it just delivers a signal -- SIGTERM politely asks a process to
 * exit; any other signal number gives finer control.
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "../include/kernel_monitor.h"

void terminate_process(pid_t pid)
{
    if (kill(pid, SIGTERM) == 0) {
        printf("SIGTERM sent to PID %d.\n", pid);
    } else {
        printf("Failed to signal PID %d: %s\n", pid, strerror(errno));
        if (errno == EPERM) printf("(Hint: you may need elevated privileges for this PID.)\n");
    }
}

void send_custom_signal(pid_t pid, int signum)
{
    if (kill(pid, signum) == 0) {
        printf("Signal %d sent to PID %d.\n", signum, pid);
    } else {
        printf("Failed to signal PID %d: %s\n", pid, strerror(errno));
    }
}
