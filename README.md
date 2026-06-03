# Operating Systems in C — CSE 231

![C](https://img.shields.io/badge/C-00599C?logo=c&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B11-00599C?logo=cplusplus&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)
![Make](https://img.shields.io/badge/GNU%20Make-A42E2B?logo=gnu&logoColor=white)

A collection of five low-level systems-programming projects built **from scratch in C / C++**, each re-implementing a core operating-system mechanism without relying on high-level library abstractions. The goal across all of them was to understand how the OS actually works underneath: process creation, ELF loading, CPU scheduling, virtual memory, and multithreading.


---

## Repository Map

The folders use mixed naming, so here's what each one actually contains:

| Folder | Project | One-liner |
|--------|---------|-----------|
| [`upload`](./upload) | **SimpleLoader** | A minimal ELF 32-bit loader that loads and runs an executable by hand |
| [`Assignment_2`](./Assignment_2) | **SimpleShell** | A working Unix shell with pipes and command history |
| [`Assignment_3`](./Assignment_3) | **SimpleScheduler** | A round-robin CPU scheduler daemon driven by the shell |
| [`Assignment-4`](./Assignment-4) | **SimpleSmartLoader** | The loader, upgraded to lazy / demand paging via page-fault handling |
| [`Assignment-5`](./Assignment-5) | **SimpleMultithreader** | A header-only Pthreads abstraction with `parallel_for` and C++11 lambdas |


---

## Projects

### 1. SimpleLoader — a hand-written ELF loader
Loads and executes a statically-linked **32-bit ELF** binary in pure C, without using any ELF-manipulation libraries. It opens the binary, reads it into memory, parses the ELF header, walks the program-header table to find the `PT_LOAD` segment containing the entry point, allocates memory with `mmap`, copies the segment in, casts the entry address to a function pointer, and calls `_start`. Packaged as a shared library (`lib_simpleloader.so`) invoked by a small `launch` helper.

**Concepts:** ELF format, program headers, `mmap`, function pointers, shared libraries.

### 2. SimpleShell — a Unix shell
A read–parse–execute loop that behaves like a real shell. It reads a command, parses the command and arguments, forks a child process, and `exec`s the command — supporting external programs from `/usr/bin` as well as **pipes** (`cmd1 | cmd2 | cmd3`). A `history` command tracks every command entered, and on exit the shell reports per-command execution details: PID, start time, and total duration.

**Concepts:** `fork` / `exec` / `wait`, `pipe` + `dup2`, signal handling, process timing.

### 3. SimpleScheduler — a round-robin process scheduler
Builds on the shell to add CPU scheduling. The shell launches with two parameters, `NCPU` (number of CPUs) and `TSLICE` (time quantum). Users `submit ./a.out` to queue jobs, which wait on a signal rather than running immediately. A single scheduler **daemon** picks `NCPU` jobs from the front of a ready queue each quantum, signals them to run (`SIGCONT`), pauses them when the slice expires (`SIGSTOP`), and rotates them to the back of the queue. On termination it prints each job's completion and wait times. A `dummy_main.h` header redirects the user's `main` so the scheduler can control execution.

**Concepts:** round-robin scheduling, `SIGSTOP` / `SIGCONT`, daemon processes, ready queues, preprocessor tricks (`#define main dummy_main`).

### 4. SimpleSmartLoader — a lazy-loading ELF loader
The "smart" upgrade to SimpleLoader: it loads **no segments upfront**. It jumps straight to `_start`, which triggers a segmentation fault on the unmapped page. That fault is caught and treated as a **page fault** — the loader maps exactly one 4 KB page at a time and copies in only the needed slice of the segment, then resumes execution. After the program finishes it reports the total page faults, total page allocations, and internal fragmentation.

**Concepts:** demand paging, `SIGSEGV` handling, page-by-page (4 KB) allocation, internal fragmentation, virtual memory.

### 5. SimpleMultithreader — a Pthreads abstraction
A **header-only** C++ library (`simple-multithreader.h`) that hides Pthreads boilerplate behind two clean APIs using **C++11 lambdas**: a 1D `parallel_for(low, high, lambda, numThreads)` and a 2D version for nested loops. Each call spawns exactly `numThreads` Pthreads, partitions the iteration range across them, joins them, and reports the execution time — with no thread pools and no `std::thread`.

**Concepts:** Pthreads, C++11 lambdas (`std::function`), work partitioning, parallel loops.

---

## Core OS Concepts Demonstrated

- **Process management** — `fork`, `exec`, `wait`, signals
- **Inter-process communication** — pipes and `dup2`
- **CPU scheduling** — round-robin with time quanta and a daemon scheduler
- **Executable & memory model** — manual ELF parsing and loading
- **Virtual memory** — demand paging via `SIGSEGV`, 4 KB page allocation, fragmentation tracking
- **Concurrency** — raw Pthreads wrapped in an ergonomic parallel-for API

## Tech Stack

C · C++11 · POSIX / Unix system calls · Pthreads · GNU Make · Linux
