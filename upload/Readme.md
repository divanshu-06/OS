# SimpleLoader — A From-Scratch 32-bit ELF Loader in C

A minimal program loader written in C that does, by hand, what the operating
system does every time you run a binary: read an ELF executable off disk, lay
its segments out in memory, resolve the entry point, and transfer control to it
— **without using any library that parses ELF.** Everything is done by reading
raw bytes and interpreting the ELF header structures directly.

The included test binary computes `fib(40)`; a correct run prints its return
value, `102334155`.

> Built as a systems-programming exercise to understand process loading, the ELF
> format, and virtual memory at the level the kernel works with them.

---

## Concepts demonstrated

- **ELF format parsing** — reading and validating the ELF header and program
  header table from raw file bytes.
- **Process loading / virtual memory** — placing `PT_LOAD` segments into memory
  with `mmap`, and reasoning about virtual vs. mapped addresses.
- **Low-level systems programming in C** — `open`/`read`/`lseek`/`mmap`/`munmap`,
  manual memory management, and calling into loaded code via a function pointer.
- **Toolchain awareness** — building a shared library, controlling the link with
  `rpath`, and compiling a freestanding (`-nostdlib`, `-no-pie`) test binary.

---

## How it works

The loader runs the following pipeline (`loader/loader.c`):

1. **Read the ELF header.** The header sits at offset 0 and is the map of the
   whole file. It's read into a heap buffer.
2. **Validate the magic number.** Confirms the first four bytes are
   `0x7F 'E' 'L' 'F'` before trusting anything else in the file.
3. **Read the program header table.** Uses `e_phoff`, `e_phentsize`, and
   `e_phnum` to locate and read every segment descriptor.
4. **Map the loadable segments.** For each `PT_LOAD` header, `mmap`s a region
   (`RWX`, anonymous, private) and copies the segment bytes from the file into
   it, tracking each mapping for later cleanup.
5. **Resolve the entry point (rebasing).** Because segments are mapped at a
   kernel-chosen address rather than at their linked virtual address, the loader
   can't jump to `e_entry` directly. It finds the segment whose virtual range
   contains `e_entry`, computes the offset within that segment, and adds it to
   the segment's *actual* mapped base to get the real address of `_start`.
6. **Run it.** The resolved address is cast to a function pointer and called,
   and its return value is printed.

A cleanup routine then `munmap`s every segment, frees the buffers, and closes
the file.

The launcher (`launcher/launch.c`) is a thin front end: it validates the input
path, then hands off to the loader (built as `lib_simpleloader.so`) and triggers
cleanup.

---

## Build & run

Requires a Linux environment with `gcc`, GNU `make`, and 32-bit build support
(`sudo apt install gcc-multilib` on Ubuntu/WSL).

```bash
make                  # builds the loader (.so), launcher, and test binary
cd bin
./launch ../test/fib
```

Expected output:

```
ELF execution started
User _start return value = 102334155
ELF execution completed
```

---

## Known limitations & design trade-offs

These are deliberate scope cuts, not accidents — and they're the most
interesting part of how this differs from a real loader:

- **PC-relative code only.** Segments are mapped at an arbitrary address rather
  than at the binary's linked virtual address, so the test program runs only
  because its code is position-independent by construction (relative calls, no
  globals, no absolute relocations). A general loader would map at the intended
  virtual address (e.g. `MAP_FIXED`) or apply relocations.
- **`.bss` is not handled.** The loader copies `p_memsz` bytes from the file
  rather than copying `p_filesz` bytes and zero-filling the rest, so segments
  with uninitialized data wouldn't be laid out correctly.
- **32-bit ELF only.** All structures are `Elf32_*` and the build targets `-m32`.
