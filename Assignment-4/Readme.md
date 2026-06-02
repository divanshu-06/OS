# SimpleSmartLoader — A Lazy-Loading ELF Loader in C

A minimal **ELF32 program loader that implements demand paging from scratch** — no `glibc` startup, no upfront `mmap` of segments. The loader jumps straight to the program's entry point with nothing mapped, catches the resulting `SIGSEGV`, and maps + loads exactly the 4 KB page that faulted. The program then resumes as if nothing happened, just like the Linux page-fault path.

Built for CSE 231 (Operating Systems) as an upgrade to a classic eager loader.

---

## What it demonstrates

This project is essentially a hand-rolled version of the kernel's demand-paging mechanism, in user space:

- **Lazy loading** — a segment's pages are brought into memory only when first accessed, never upfront.
- **`SIGSEGV` as a page fault** — because no memory is pre-mapped, accessing a valid-but-unbacked address raises a segmentation fault. The loader treats that fault as a page fault and services it.
- **Page-by-page allocation** — memory is mapped one 4 KB page at a time, even within a single segment. A 5 KB segment causes two faults, not one.
- **Permission handling** — pages are mapped writable, the file contents are copied in, then `mprotect`ed down to the segment's real permissions (e.g. `R-X` for code).
- **Instrumentation** — on exit, the loader reports page faults handled, page allocations, and internal fragmentation.

---

## How it works

```
            load_and_run_elf()
                   |
   read ELF header + validate (0x7F 'E' 'L' 'F', ELFCLASS32)
                   |
   read program headers, record per-PT_LOAD metadata
   (page_count + page_alloc[] bitmap)  -- NO memory mapped yet
                   |
   install SIGSEGV handler (sigaction + SA_SIGINFO)
                   |
   entry_fn = (void(*)()) e_entry;  entry_fn();   <-- jumps into unmapped code
                   |
        +----------v-----------------------------+
        |        SIGSEGV handler (per fault)      |
        |  1. fault addr -> page-aligned base     |
        |  2. find owning PT_LOAD segment         |
        |  3. mmap 1 page (MAP_FIXED, RW)         |
        |  4. pread file bytes for that page      |
        |  5. zero the remainder (BSS)            |
        |  6. mprotect -> segment's real perms    |
        |  7. update fault/alloc/frag counters    |
        +----------+------------------------------+
                   |
   handler returns -> CPU re-executes the faulting instruction -> succeeds
                   |
        ...repeats for each newly-touched page...
                   |
            loader_cleanup()  -> munmap pages, free, print stats
```

The key insight: when a signal handler returns, the faulting instruction is **retried**. By mapping the missing page inside the handler, execution transparently continues.

---

## Project structure

```
.
├── Makefile              # top-level: builds loader -> launcher -> test
├── bin/                  # build output: lib_simpleloader.so, launch
├── loader/
│   ├── loader.c          # the loader: ELF parsing + SIGSEGV-based lazy paging
│   ├── loader.h          # API: load_and_run_elf(), loader_cleanup()
│   └── Makefile          # builds lib_simpleloader.so (32-bit shared lib)
├── launcher/
│   ├── launch.c          # CLI front-end: validates args, calls the loader
│   └── Makefile          # links launch against lib_simpleloader.so
└── test/
    ├── fib.c             # test program (-nostdlib, custom _start)
    └── Makefile          # builds fib with -m32 -no-pie -nostdlib
```

The loader is compiled as a shared library (`lib_simpleloader.so`); the launcher links against it and is the program you actually run.

---

## Build & run

> Requires a 32-bit toolchain. On Debian/Ubuntu: `sudo apt install gcc-multilib`.
> Do **not** build on macOS — this relies on Linux Unix APIs.

```bash
make clean      # remove any stale binaries
make            # builds lib_simpleloader.so, launch, and the test binary
./bin/launch test/fib
```

### Example output

```
ELF execution started
User _start returned
ELF execution completed
SimpleSmartLoader stats:
 page faults handled   : 3
 page allocations      : 3
 internal fragmentation : 7.889 KB
```

### Reading the stats (worked example)

For the `fib` test, only three pages are ever touched, so only three faults occur:

| Access            | Page mapped | Notes                          |
|-------------------|-------------|--------------------------------|
| entry point (code)| 1 page      | first instruction fetch faults |
| `A[1024]` (data)  | 1 page      | the write/read loop            |
| `sum` (data)      | 1 page      | sits on the next page          |

The two read-only `LOAD` segments are never accessed, so they're never loaded — that's lazy loading in action.

**Internal fragmentation** is the unused tail in the last allocated page of each touched segment (interior pages are always full). For this run that's 3986 B (code page) + 4092 B (`sum` page) = 8078 B = **7.889 KB**.

---

## Implementation notes

- **`segv_handler`** uses `SA_SIGINFO` to read the faulting address from `siginfo_t.si_addr`, then maps and loads the owning page. A `page_alloc[]` bitmap per segment prevents re-handling an already-mapped page (which would otherwise loop forever on a genuine bad access — that case is correctly forwarded to the default handler via `signal(SIGSEGV, SIG_DFL); raise(SIGSEGV);`).
- **Map-then-protect:** pages are mapped `PROT_READ | PROT_WRITE` so contents can be copied in, then `mprotect`ed to the segment's actual flags. This is required for execute-only code segments.
- **BSS:** any part of a page beyond `p_filesz` is zero-filled, so uninitialized data behaves correctly.
- **ELF validation:** checks the magic bytes and rejects anything that isn't `ELFCLASS32`.

---

## Limitations

- **ELF32 only**, statically linked, with no references to `glibc` APIs (the test program defines its own `_start` and is built with `-nostdlib`).
- **Page-index math assumes page-aligned segment starts.** `page_idx = (page_base - p_vaddr) / PAGE_SIZE` is computed in unsigned arithmetic; if a program accessed the sub-page region below the first page boundary of a segment whose `p_vaddr` is not page-aligned, the index would underflow. This never triggers for the provided test cases (their objects are page-aligned) but is a known edge case.
- The test program must not use blocking or libc-dependent calls.

---

## Tech

`C` · `mmap` / `mprotect` / `munmap` · `sigaction` (SIGSEGV) · `pread` · raw ELF32 parsing · GNU Make · 32-bit (`-m32`)

---

## Authors

- **Divanshu Jain** — SIGSEGV handler, page-fault logic, mmap-based allocation
- **Parth Choyal** — ELF parsing, program-header management, cleanup routines

> Course project for CSE 231: Operating Systems, IIIT-Delhi.
