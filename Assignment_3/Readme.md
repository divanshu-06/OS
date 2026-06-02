# SimpleScheduler — A Round-Robin Process Scheduler in C

A user-space CPU scheduler built from scratch in C. It pairs an interactive shell
with a separate scheduler daemon to run submitted programs under a **round-robin**
policy, simulating how an OS time-slices a limited number of CPUs across many
processes. The scheduler controls each job entirely through Unix signals and
communicates with the shell over pipes.

> Course: CSE 231 — Operating Systems (Assignment 3). Built on top of the
> SimpleShell from Assignment 2.

---

## Authors & Contributions

- **Parth Choyal (2024403)** — queue and round-robin scheduling logic, CPU/quantum management.
- **Divanshu Jain (2024198)** — shell↔scheduler communication, signal handling, job tracking and summary.


---

## What It Does

You launch the shell with a number of CPUs (`NCPU`) and a time quantum
(`TSLICE`, in milliseconds). You then submit executables to it. The shell hands
each job to a scheduler daemon, which runs up to `NCPU` jobs at a time for one
quantum each, pausing and rotating them in a circular queue until they all
finish. On exit, the shell prints a per-job summary of completion time and wait
time.

```
$ ./simple_shell 2 1000          # 2 "CPUs", 1000 ms time slice
SimpleShell$ submit ./a.out
SimpleShell$ submit ./b.out
SimpleShell$ submit ./c.out
SimpleShell$ exit

 Job Summary (times in multiples of 1000 ms)
./a.out  pid=12841  completion=3000  wait=1000
./b.out  pid=12842  completion=3000  wait=1000
./c.out  pid=12843  completion=4000  wait=2000
```

---

## Architecture

The system is split into **two cooperating processes** connected by two
unidirectional pipes:

```
        ADD <pid> / EXIT                
  ┌──────────────────────────────►  ┌───────────────────┐
  │        (pipe_cmd)                │  simple_scheduler  │
┌─┴────────────┐                     │   (daemon)         │
│ simple_shell │                     │                    │
│  (parent)    │  ◄──────────────────┤  ready queue       │
└─┬────────────┘     RUN <pid>       │  round-robin loop  │
  │                  (pipe_rep)      └─────────┬──────────┘
  │ fork + exec                                │ SIGCONT / SIGSTOP
  ▼                                            ▼
 job processes  ◄───────────────── signals ───┘
 (a.out, compiled with dummy_main.h)
```

**Why two processes?** The shell stays responsive to the user (reading commands,
reaping finished jobs) while the scheduler independently runs its timing loop.
The scheduler is a *daemon*: it sleeps when idle and only wakes at the end of
each quantum, so it burns almost no CPU.

A key design choice: **the shell `fork`s and `exec`s the job processes itself**,
so the jobs are children of the *shell*, not the scheduler. This lets the shell
`waitpid()` to reap them and record finish times, while the scheduler — which
only knows their PIDs — controls them purely with `kill()` signals. Clean
separation of ownership (shell) from control (scheduler).

---

## Files

| File | Role |
|------|------|
| `simple_shell.c` | Interactive CLI. Launches the scheduler, forks/execs jobs, tracks timing, prints the summary on exit. |
| `simple_scheduler.c` | The scheduler daemon. Owns the ready queue and runs the round-robin loop. |
| `dummy_main.h` | Header users `#include` in their program so it starts in a *stopped* state, handing the scheduler control from the moment the process is created. |

---

## How It Works (end to end)

1. **Startup.** `simple_shell <NCPU> <TSLICE>` installs a `SIGCHLD` handler,
   creates two pipes, then `fork`s and `exec`s `simple_scheduler`, passing it
   `NCPU`, `TSLICE`, and the pipe file descriptors as arguments.

2. **Submit.** When you type `submit ./a.out`, the shell `fork`s a child that
   `exec`s the program. Because the program was compiled with `dummy_main.h`, the
   very first thing it does is `raise(SIGSTOP)` — it freezes itself immediately,
   *before* running any user code. The shell records the job and sends
   `ADD <pid>` to the scheduler.

3. **Enqueue.** The scheduler reads `ADD <pid>`, confirms the process is alive,
   and pushes it to the rear of the ready queue.

4. **Run a quantum.** The scheduler pops up to `NCPU` PIDs from the front of the
   queue, sends each a `SIGCONT` (resuming it), and reports `RUN <pid>` back to
   the shell (which uses this to count how many quanta each job got). It then
   sleeps for `TSLICE` milliseconds.

5. **Preempt and rotate.** After the quantum, the scheduler sends `SIGSTOP` to
   every job that's still alive and pushes it back to the rear of the queue.
   Jobs that finished during the quantum are simply dropped.

6. **Finish.** When a job terminates naturally, `SIGCHLD` fires in the shell; the
   handler `waitpid()`s it (non-blocking) and stamps its finish time.

7. **Exit.** On `exit`, the shell sends `EXIT`, waits for all jobs to finish and
   the scheduler to terminate, then prints the summary table.

---

## The `dummy_main.h` Trick

This header is the cleverest piece of the design. Its job is to guarantee that a
submitted program **does not start executing its real work until the scheduler
says so.**

```c
int dummy_main(int argc, char **argv);

int main(int argc, char **argv) {
    raise(SIGSTOP);                  // freeze immediately on launch
    int ret = dummy_main(argc, argv);
    return ret;
}

#define main dummy_main              // user's main() is renamed to dummy_main()
```

The `#define main dummy_main` is a preprocessor rename. When a user writes
`int main()` in their program, the preprocessor rewrites it to
`int dummy_main()`. So the *real* entry point becomes the `main()` defined in
this header, which calls `raise(SIGSTOP)` first and only then calls the user's
(now-renamed) code. The result: every submitted process is born stopped, and the
scheduler has total control from the instant the process exists. Without this,
a freshly `exec`'d process would race ahead and run before being scheduled.

---

## Scheduling Algorithm (round-robin)

The ready queue is a **circular array** (`Queue` in `simple_scheduler.c`) giving
O(1) enqueue/dequeue, doubling its capacity when full. Each iteration of the
scheduler loop:

1. Drain any pending `ADD`/`EXIT` messages from the shell.
2. If the queue is empty, **block on `poll()`** until new input arrives (no busy
   waiting). If empty *and* `EXIT` was requested, terminate.
3. Pop up to `NCPU` *alive* PIDs from the front.
4. `SIGCONT` them and report `RUN <pid>` to the shell.
5. `sleep_ms(TSLICE)` — the only place real time passes.
6. `SIGSTOP` the survivors and re-enqueue them at the rear.
7. Drain new arrivals so they're picked up next quantum.

As permitted by the assignment, the quantum is treated as starting and expiring
simultaneously for all currently running jobs; jobs that arrive mid-quantum join
from the next quantum onward.

---

## Inter-Process Communication

Two pipes carry a tiny line-based text protocol:

| Pipe | Direction | Messages |
|------|-----------|----------|
| `pipe_cmd` | shell → scheduler | `ADD <pid>` (new job), `EXIT` (shut down) |
| `pipe_rep` | scheduler → shell | `RUN <pid>` (job was scheduled this quantum) |

Both sides read **non-blocking** and accumulate bytes in a buffer, only acting
on complete newline-terminated lines. This matters because a single `read()` on
a pipe can return a partial message (e.g. `RUN 12` instead of `RUN 1234\n`); the
buffer stitches fragments back together and carries the leftover into the next
read.

---

## CPU Efficiency

A scheduler that spins is a bad scheduler. This one avoids busy-waiting on both
ends:

- **Scheduler**: when the ready queue is empty, it `poll()`s the command pipe and
  sleeps until data arrives. While jobs run, it spends the quantum in
  `nanosleep()`, not a spin loop.
- **Shell**: it `select()`s over `stdin` and the report pipe with a 200 ms
  timeout, so it wakes for user input or scheduler reports without polling.

The `SIGCHLD` handler is registered with `SA_NOCLDSTOP`, so the constant
`SIGSTOP`/`SIGCONT` traffic does *not* generate spurious `SIGCHLD`s — only real
terminations do.

---

## Metrics: Completion and Wait Time

On exit, for each job the shell computes (all values rounded to whole quanta and
reported in milliseconds):

- **Completion (turnaround) time** = wall-clock duration from submit to finish,
  rounded *up* to the nearest `TSLICE`. A job shorter than one quantum still
  counts as `1 × TSLICE`.
- **Run quanta** = number of `RUN <pid>` reports received for that job (how many
  times it was scheduled).
- **Wait time** = `completion_quanta − run_quanta`, clamped at ≥ 0 — i.e. the
  time the job spent in the ready queue rather than running, expressed in
  `TSLICE` units.

---

## Build & Run

```bash
# Build the shell and scheduler
gcc -O2 -Wall -o simple_shell      simple_shell.c
gcc -O2 -Wall -o simple_scheduler  simple_scheduler.c

# Compile a test job (must #include "dummy_main.h" right after stdio.h)
gcc -O2 -o a.out my_program.c

# Run: ./simple_shell <NCPU> <TSLICE_ms>
./simple_shell 2 1000
```

A submitted program looks like this:

```c
#include <stdio.h>
#include "dummy_main.h"   // <-- added right after stdio.h

int main() {              // preprocessor renames this to dummy_main()
    long x = 0;
    for (long i = 0; i < 100000000L; i++) x += i;   // CPU-bound work
    printf("done %ld\n", x);
    return 0;
}
```

**Constraints on jobs** (per the assignment): no command-line arguments, and no
blocking calls such as `scanf` or `sleep`.

---

## Design Decisions

- **`SIGSTOP`/`SIGCONT` for preemption.** These signals cannot be caught,
  blocked, or ignored, so they're a reliable way to pause/resume an uncooperative
  job. The scheduler doesn't need the job's cooperation.
- **Jobs parented to the shell, controlled by the scheduler.** Keeps reaping
  (`waitpid`) and signaling (`kill`) cleanly separated.
- **Line-buffered text protocol over pipes.** Simple to debug and robust to
  partial reads.
- **Circular array queue over a linked list.** O(1) on both ends with good cache
  behavior and a single allocation that grows by doubling.

---

## Limitations

- **Jobs must be recompiled with `dummy_main.h`.** Arbitrary pre-built binaries
  can't be scheduled, because the stop-on-start behavior is injected at compile
  time.
- **No arguments or blocking calls in jobs.** A job that calls `scanf`/`sleep`
  would block a "CPU" for the whole quantum, breaking the timing model.
- **Quanta accounting is an approximation.** A job's final, partial quantum is
  counted as a full `RUN`, so reported CPU time can slightly overestimate actual
  CPU time, and completion time is wall-clock rounded up rather than an exact
  count of elapsed scheduler quanta.
- **Theoretical `SIGCONT`-before-`SIGSTOP` race.** If the scheduler ever sent a
  `SIGCONT` before a freshly launched job had executed `raise(SIGSTOP)`, the
  `SIGCONT` would be lost. In practice the pipe/poll latency makes this
  effectively impossible, but it's an inherent edge case of signal-based control.
- **No pipes between submitted jobs**, by design and per the assignment.
