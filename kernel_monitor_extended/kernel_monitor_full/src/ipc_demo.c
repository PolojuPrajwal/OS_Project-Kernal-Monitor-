#define _POSIX_C_SOURCE 200809L
/*
 * ipc_demo.c  --  CO-3: Inter-Process Communication
 * -----------------------------------------------------------------------
 * Four distinct IPC/job-control mechanisms, each demonstrated live:
 *   1. Anonymous pipes   -- pipe() + fork(), parent/child share one end each
 *   2. Named pipes(FIFOs) -- mkfifo(), two unrelated processes rendezvous
 *      through a path in the filesystem instead of a shared descriptor
 *   3. Signal handlers    -- sigaction() installs an async notification
 *      handler; the signal interrupts normal control flow at any point
 *   4. Process groups/sessions -- getpgrp()/getsid() expose the job-control
 *      identifiers the shell uses to manage foreground/background jobs
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "../include/kernel_monitor.h"

/* ---------- 1. Anonymous pipe: fork() + pipe() ---------- */
void demo_anonymous_pipe(void)
{
    int fd[2];
    if (pipe(fd) == -1) { perror("pipe"); return; }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* Child: write end only -- close the read end it inherited */
        close(fd[0]);
        const char *msg = "Hello from child via anonymous pipe!";
        write(fd[1], msg, strlen(msg) + 1);
        close(fd[1]);
        _exit(0);
    } else {
        /* Parent: read end only -- close the write end it inherited */
        close(fd[1]);
        char buf[128];
        ssize_t n = read(fd[0], buf, sizeof(buf));
        close(fd[0]);
        waitpid(pid, NULL, 0);
        if (n > 0) printf("Parent received: %s\n", buf);
    }
}

/* ---------- 2. Named pipe (FIFO): mkfifo() ---------- */
void demo_named_pipe(void)
{
    const char *fifo_path = "/tmp/kernel_monitor_fifo";

    if (mkfifo(fifo_path, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* Child acts as an independent writer process */
        int wfd = open(fifo_path, O_WRONLY);
        if (wfd == -1) { perror("open(fifo, O_WRONLY)"); _exit(1); }
        const char *msg = "Hello via named pipe (FIFO) -- unrelated processes!";
        write(wfd, msg, strlen(msg) + 1);
        close(wfd);
        _exit(0);
    } else {
        /* Parent acts as an independent reader process */
        int rfd = open(fifo_path, O_RDONLY);
        if (rfd == -1) { perror("open(fifo, O_RDONLY)"); return; }
        char buf[128];
        ssize_t n = read(rfd, buf, sizeof(buf));
        close(rfd);
        waitpid(pid, NULL, 0);
        unlink(fifo_path);
        if (n > 0) printf("Reader received: %s\n", buf);
    }
}

/* ---------- 3. Signal handler: sigaction() ---------- */
static volatile sig_atomic_t handler_fired = 0;

static void sigusr1_handler(int signum)
{
    (void)signum;
    /* Signal handlers must stay async-signal-safe: no printf(), use write() */
    const char *msg = "  [handler] Caught SIGUSR1 asynchronously!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    handler_fired = 1;
}

void demo_signal_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) { perror("sigaction"); return; }

    printf("Handler installed for SIGUSR1. Sending it to ourselves...\n");
    handler_fired = 0;
    kill(getpid(), SIGUSR1);   /* asynchronous notification, delivered by the kernel */

    if (handler_fired) printf("Control returned to normal flow after the handler ran.\n");
}

/* ---------- 4. Process groups, sessions, job control ---------- */
void show_process_group_info(void)
{
    printf("\n--- Process Group / Session Info (job control identifiers) ---\n");
    printf("PID          : %d\n", getpid());
    printf("Parent PID   : %d\n", getppid());
    printf("Process group: %d  (getpgrp())\n", getpgrp());
    printf("Session ID   : %d  (getsid(0))\n", getsid(0));
    printf(
        "\nThe shell uses these IDs for job control: all processes launched\n"
        "from one command line share a process group, so Ctrl+C (SIGINT)\n"
        "or Ctrl+Z (SIGTSTP) from the terminal can be delivered to the whole\n"
        "group at once, and the session ID ties that group to one controlling\n"
        "terminal and login session.\n"
    );
}
