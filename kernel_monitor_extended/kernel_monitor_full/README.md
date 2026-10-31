# Kernel Monitor (Extended Edition)

A user-space Linux systems-programming tool that deliberately exercises
**every course outcome (CO-1 \u2192 CO-6)** of Operating Systems and Systems
Programming, built on top of the original Kernel Monitor project
(Team 10, Sec 13).

## Why this version exists

The original Kernel Monitor covered CO-1 (syscalls/OS services), CO-2
(process management) and, partially, CO-3 (signals). CO-4 (memory
management), CO-5 (file systems) and CO-6 (concurrency) were gaps. This
version adds one module per missing CO so the project maps onto the full
syllabus, not just half of it.

## Build

```bash
make          # builds ./kernel_monitor (links -lpthread for CO-6)
make run      # build + run
make clean
```

Requires a Linux environment (relies on `/proc`) with `gcc` and `make`.

## Project layout and CO coverage

```
kernel_monitor/
├── include/kernel_monitor.h
├── src/
│   ├── main.c            # menu dispatcher only -- no OS logic of its own
│   ├── system_info.c      # CO-1
│   ├── process_info.c      # CO-2 (listing, detail view)
│   ├── process_control.c    # CO-2 (termination) / CO-3 (signals)
│   ├── ipc_demo.c             # CO-3
│   ├── memory_mgmt.c           # CO-4
│   ├── file_io.c                 # CO-5
│   └── concurrency.c              # CO-6
├── Makefile
└── README.md
```

| CO | Topic | Module | What it demonstrates |
|----|-------|--------|----------------------|
| **CO-1** | OS as a service layer | `system_info.c` | `/proc/uptime`, `/proc/cpuinfo`, `/proc/meminfo`, `/proc/stat` read via `fopen`/`fscanf` (libc wrappers around `open`/`read`/`close`); `getpid()` used directly |
| **CO-2** | Processes & process control | `process_info.c`, `process_control.c` | `opendir()`/`readdir()` enumerate every PID under `/proc`; lifecycle states (R/S/D/Z/T) parsed from `/proc/[pid]/status`; `kill()` terminates/signals a chosen process |
| **CO-3** | Inter-process communication | `ipc_demo.c` | Anonymous pipe (`pipe()`+`fork()`), named pipe/FIFO (`mkfifo()`, two independent processes), a custom signal handler (`sigaction()` + `SIGUSR1`), and process-group/session IDs (`getpgrp()`, `getsid()`) for job control |
| **CO-4** | Memory management | `memory_mgmt.c` | Virtual address space layout (`/proc/[pid]/maps`), minor/major page-fault counts (`/proc/[pid]/stat`), and a live demand-paging + copy-on-write demo (`malloc()` + `fork()`) |
| **CO-5** | File systems & file I/O | `file_io.c` | Open file descriptor table (`/proc/[pid]/fd` + `readlink()`), inode info (`stat()`), raw unbuffered I/O (`open()`/`read()`/`write()`/`close()`) contrasted with CO-1's buffered `fopen()`/`fread()`, and memory-mapped file reads (`mmap()`) |
| **CO-6** | Concurrency & synchronization | `concurrency.c` | A `pthread`-based background monitor thread with mutex-protected shared state; a race-condition demo that reliably shows lost updates without a mutex and correct results with one; a producer/consumer pair coordinated with counting semaphores |

## Usage

```
$ ./kernel_monitor
Kernel Monitor (Extended) started. PID of this process: 12345

=============== KERNEL MONITOR (Extended) ===============
 1. CO-1: OS & System Info
 2. CO-2: Process Management
 3. CO-3: Inter-Process Communication
 4. CO-4: Memory Management
 5. CO-5: File Systems & I/O
 6. CO-6: Concurrency & Synchronization
 0. Exit
===========================================================
Choice:
```

Pick a top-level category, then a numbered option within it; `0` goes
back one level. Options that need a PID (CO-2/4/5) take any PID you
find via "List all processes" or "Show this program's own PID".

## Notable systems-programming pitfalls this project ran into (and fixed)

Worth mentioning in a report -- these are exactly the kind of "common
process management pitfalls" CO-2 and CO-4 call out by name:

- **`_exit()` skips stdio flushing.** The copy-on-write demo's child
  process originally lost its own `printf()` output because `_exit()`
  (unlike `exit()`) never flushes buffered streams. Fixed with an
  explicit `fflush(stdout)` before `_exit()`.
- **fork() duplicates unflushed buffered output.** After fixing the
  above, output appeared *twice* -- the child inherited a copy of
  stdout's buffer (everything printed by the menu so far but not yet
  flushed, since stdout is fully buffered when not attached to a
  terminal), and both parent and child eventually flushed the same
  bytes. Fixed by flushing stdout immediately before `fork()`.
- **Race conditions aren't guaranteed to show up.** An unprotected
  `counter++` from two threads didn't actually lose any updates on the
  first few runs -- the loop was fast enough that the two threads
  rarely interleaved mid-increment. Splitting the read-modify-write
  into explicit steps with a `sched_yield()` in between widens the
  interleaving window so the race reliably manifests every run,
  which is what makes the demo convincing rather than lucky.

## Verification

Every module was built with `-Wall -Wextra -std=c11` (zero warnings) and
re-run under `-fsanitize=address,undefined` across all six CO menus with
no errors reported -- no memory leaks, no undefined behavior, no data
races flagged.
