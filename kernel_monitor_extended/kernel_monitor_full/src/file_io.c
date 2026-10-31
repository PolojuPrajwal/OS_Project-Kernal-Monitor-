#define _POSIX_C_SOURCE 200809L
/*
 * file_io.c  --  CO-5: File Systems, File Abstractions, and File I/O
 * -----------------------------------------------------------------------
 * Four ideas made concrete:
 *   1. File descriptor table  -- /proc/[pid]/fd lists every fd a process
 *      has open as symlinks back to the real file (or pipe/socket).
 *   2. The Unix file abstraction (inodes) -- stat() exposes the inode
 *      number, size and permissions the VFS tracks for any file.
 *   3. Raw vs buffered I/O -- open()/read()/write()/close() talk to the
 *      kernel on every call; fopen()/fread() (used in system_info.c)
 *      add a userspace buffer on top.
 *   4. Memory-mapped file I/O -- mmap() lets a file's contents be read
 *      as if it were an in-memory array, with the kernel paging it in
 *      on demand (the same mechanism as CO-4's demand paging).
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "../include/kernel_monitor.h"

void list_open_fds(pid_t pid)
{
    char dir_path[64];
    snprintf(dir_path, sizeof(dir_path), "/proc/%d/fd", pid);

    DIR *d = opendir(dir_path);
    if (!d) { printf("Could not open %s: process may not exist or isn't owned by you.\n", dir_path); return; }

    printf("\n--- Open File Descriptors for PID %d (/proc/%d/fd) ---\n", pid, pid);
    printf("%-5s %s\n", "FD", "TARGET");

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char link_path[600], target[256];
        snprintf(link_path, sizeof(link_path), "%s/%s", dir_path, entry->d_name);
        ssize_t len = readlink(link_path, target, sizeof(target) - 1);
        if (len >= 0) {
            target[len] = '\0';
            printf("%-5s %s\n", entry->d_name, target);
        }
    }
    closedir(d);
}

void show_inode_info(const char *path)
{
    struct stat st;
    if (stat(path, &st) == -1) { perror("stat"); return; }

    printf("\n--- Inode Info for %s (the Unix file abstraction) ---\n", path);
    printf("Inode number : %lu\n", (unsigned long)st.st_ino);
    printf("Size         : %lld bytes\n", (long long)st.st_size);
    printf("Hard links   : %lu\n", (unsigned long)st.st_nlink);
    printf("Permissions  : %o (octal)\n", st.st_mode & 0777);
    printf(
        "\nThe filename is just a directory-entry pointer to this inode --\n"
        "that's why multiple names (hard links) can share one inode number.\n"
    );
}

void demo_raw_io(void)
{
    const char *path = "/tmp/kernel_monitor_raw_io_demo.txt";
    const char *msg = "Written with raw open()/write() -- no libc buffering.\n";

    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); return; }
    ssize_t written = write(fd, msg, strlen(msg));
    close(fd);
    printf("Wrote %zd bytes to %s using raw write().\n", written, path);

    fd = open(path, O_RDONLY);
    if (fd == -1) { perror("open"); return; }
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
        buf[n] = '\0';
        printf("Read back using raw read(): %s", buf);
    }
    unlink(path);
    printf(
        "Unlike fopen()/fread() (used elsewhere in this project for /proc),\n"
        "open()/read()/write() talk to the kernel on every single call --\n"
        "no userspace buffering layer in between.\n"
    );
}

void demo_mmap_read(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd == -1) { perror("open"); return; }

    struct stat st;
    if (fstat(fd, &st) == -1) { perror("fstat"); close(fd); return; }

    if (st.st_size == 0) { printf("File is empty, nothing to map.\n"); close(fd); return; }

    void *mapped = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd); /* fd can be closed once mapped -- the mapping stays valid */

    if (mapped == MAP_FAILED) { perror("mmap"); return; }

    printf("\n--- Memory-Mapped Read of %s (%lld bytes) ---\n", path, (long long)st.st_size);
    size_t show = st.st_size < 300 ? (size_t)st.st_size : 300;
    fwrite(mapped, 1, show, stdout);
    if (st.st_size > 300) printf("\n...(truncated, %lld bytes total)...\n", (long long)st.st_size);
    printf("\nThe file's bytes were accessed directly as memory -- the kernel\n"
           "pages the file's contents in on demand, same mechanism as CO-4.\n");

    munmap(mapped, st.st_size);
}
