# SimpleShell — A Unix Shell in C from Scratch

> CSE 231: Operating Systems — Assignment 2 (Section A), IIIT-Delhi
> Instructor: Vivek Kumar

A minimal Unix-style command-line shell written in C. It reads a command from
standard input, parses it, executes it in a child process, supports arbitrarily
long pipelines (`a | b | c | …`), keeps a per-session command history, and prints
a detailed execution summary (PID, start time, duration) when it terminates.

---

## Table of Contents

- [Features](#features)
- [Repository Structure](#repository-structure)
- [Build & Run](#build--run)
- [Supported Commands](#supported-commands)
- [How It Works](#how-it-works)
- [Built-in Commands](#built-in-commands)
- [Exit Summary](#exit-summary)
- [Limitations](#limitations)
- [Contributors](#contributors)
- [Academic Note](#academic-note)

---

## Features

- **Interactive prompt** (`simpleshell> `) running in an infinite read–parse–execute loop.
- **External command execution** via `fork()` + `execvp()` in a separate child process.
- **Pipelines of any length** (`cmd1 | cmd2 | cmd3 | …`) wired together with `pipe()` and `dup2()`.
- **Command history** that records only the commands typed at the prompt.
- **Execution metrics** — for every command it stores the PID, start time, and
  wall-clock duration (nanosecond precision via `clock_gettime`).
- **Graceful termination** on `exit`, `Ctrl-C` (SIGINT), and `Ctrl-D` (EOF),
  each printing a full session summary.
- **Robust error checking** around `pipe()`, `fork()`, `malloc`/`realloc`, and `execvp()`.

---

## Repository Structure

```
Assignment_2/
├── simple-shell.c     # Full shell implementation
├── Readme.txt         # Original design notes / contributions
├── fib                # (provided) Fibonacci calculator executable, for ./fib 40
├── helloworld         # (provided) Hello-world executable, for ./helloworld
└── file.txt           # Sample file with repeated lines, for testing uniq
```

---

## Build & Run

A C compiler (GCC) and a Unix-like environment (Linux or WSL) are required.
**macOS is not supported** for this course's assignments.

**Compile directly with GCC:**

```bash
gcc -Wall -Wextra -o simple-shell simple-shell.c
```

**Run:**

```bash
./simple-shell
```

You will be greeted by the `simpleshell> ` prompt. Type commands as you would in
a normal shell. Exit with `exit`, `Ctrl-C`, or `Ctrl-D`.


---

## Supported Commands

The shell does **not** implement the commands themselves — it locates and runs
the real binaries (found in `/usr/bin`, `/bin`, etc.) through `execvp`, which
searches your `PATH`. As long as a command resolves to an executable on `PATH`
or is given as a relative/absolute path, it will run.

| Category            | Examples                                                        |
| ------------------- | --------------------------------------------------------------- |
| Listing             | `ls`, `ls /home`, `ls -R`, `ls -l`                              |
| Printing            | `echo you should be aware of the plagiarism policy`             |
| Counting / search   | `wc -l fib.c`, `wc -c fib.c`, `grep printf helloworld.c`        |
| Local executables   | `./fib 40`, `./helloworld`                                      |
| Text processing     | `sort fib.c`, `uniq file.txt`                                   |
| Pipelines           | `cat fib.c \| wc -l`, `cat helloworld.c \| grep print \| wc -l` |
| Built-ins           | `history`, `exit`                                               |

---

## How It Works

The program is a classic read–parse–execute loop. At a high level:

1. **Read** — `read_line()` reads one full line from `stdin` using `getline()`,
   which grows its buffer automatically so commands of any length are accepted.
   If `getline()` hits end-of-file (`Ctrl-D`), the shell prints the summary and exits.

2. **Dispatch** — `shell_loop()` checks the line. If it starts with `history` or
   `exit`, the corresponding built-in runs. Empty lines are ignored. Everything
   else is handed to the pipeline executor.

3. **Parse** — `parse_line()` splits the line on the pipe character `|` into a
   list of stage strings. Each stage is later split on spaces/newlines by
   `split_args()` into an `argv[]`-style array for `execvp`.

4. **Execute** — `execute_pipeline()` forks one child per stage, connects the
   stages with pipes, lets each child `execvp()` its command, and the parent
   waits for all children to finish.

### The pipeline mechanism (`execute_pipeline`)

This is the heart of the shell. For a line like `cat file | grep x | wc -l`,
`parse_line` produces three stages and the function loops once per stage:

- A fresh pipe `fd[2]` is created with `pipe()` (read end `fd[0]`, write end `fd[1]`).
- `fork()` creates a child. The PID of the **first** child is saved (`first_pid`)
  and later stored as the pipeline's representative PID in history.
- **In the child:**
  - If there was a previous stage, its read end (`prev`) is connected to `STDIN`
    with `dup2()`, so this stage reads the previous stage's output.
  - If this is **not** the last stage (`commands[i+1] != NULL`), the current
    pipe's write end is connected to `STDOUT`, so this stage's output flows into
    the next stage.
  - Both raw pipe descriptors are closed, then `execvp()` replaces the child
    image with the requested program. If `execvp` fails, `perror` reports it and
    the child exits.
- **In the parent:** the write end is closed, the previous read end is closed
  (it is no longer needed), and `prev` is updated to the current read end so the
  next iteration can hook into it.

After launching every stage, the parent calls `wait(NULL)` once per child to
reap them, then records the timing and PID into the history array.

Timing uses `clock_gettime(CLOCK_MONOTONIC, …)` around the whole pipeline so that
even very fast commands get a meaningful, sub-second duration (plain `time()`
would round most of them to zero).

### Signal handling

`main()` installs a SIGINT handler with `sigaction`. Pressing `Ctrl-C` triggers
`signal_handler()`, which prints the exit summary, frees the history, and exits
cleanly — so termination always ends with a full report rather than an abrupt kill.

---

## Built-in Commands

These are handled inside the shell itself (not via `fork`/`exec`):

- **`history`** — prints every command entered at the prompt during the current
  session, in order, with its arguments. The `history` command itself is also
  recorded (with PID `0`, since it does not spawn a process).
- **`exit`** — ends the session and prints the summary.

---

## Exit Summary

When the shell terminates (via `exit`, `Ctrl-C`, or `Ctrl-D`), it prints a report
of everything in the current session's history, for example:

```
SimpleShell Summary
total commands executed: 2

Command 1: cat file.txt | sort | uniq
  PID: 554
  Execution Time: Tue Jun  2 14:36:24 2026
  Duration: 0.0217 seconds

Command 2: history
  PID: 0
  Execution Time: Tue Jun  2 14:36:24 2026
  Duration: 0.0000 seconds

Thank you! Onto the next assignment!!
```

Only commands from the **current** invocation are shown; nothing is persisted
across runs.

---

## Limitations

By design (and per the assignment's simplifying restrictions), the shell does
**not** support the following. None of these are bugs — they are intentional
scope boundaries:

- **`cd` (change directory)** — `cd` is a shell *built-in*, not a standalone
  program. There is no `/usr/bin/cd` binary for `execvp` to run, and even if there
  were, it would change the directory of the short-lived child process rather than
  the shell, so it would have no lasting effect. Implementing it would require a
  dedicated `chdir()` built-in.
- **Quotes and backslashes** — input is split purely on whitespace and `|`, so
  arguments cannot be grouped with `"…"`/`'…'` or escaped with `\` (per the
  assignment's input restrictions).
- **I/O redirection** (`>`, `<`, `>>`) — these tokens are not interpreted; they
  would be passed to the program as literal arguments.
- **Background execution** (`&`) and **command sequencing** (`;`, `&&`, `||`).
- **Environment-variable expansion** (`$VAR`) and **globbing/wildcards** (`*`, `?`),
  which a full shell normally expands before `exec`.

---

## Contributors

| Member         | Roll No. | Responsibilities                                                   |
| -------------- | -------- | ------------------------------------------------------------------ |
| Divanshu Jain  | 2024198  | `execute_pipeline`, SIGINT signal handler, `shell_loop`            |
| Parth Choyal   | 2024403  | reading / parsing / arg-splitting, `history`, PID & time tracking  |

