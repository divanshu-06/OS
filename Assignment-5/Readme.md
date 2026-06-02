# SimpleMultithreader

A header-only C++ library that hides the boilerplate of POSIX threads behind a single `parallel_for` call. You write your loop body as a C++11 lambda, say how many threads to use, and the library handles the work splitting, thread creation, joining, and timing for you.

Built for **CSE 231: Operating Systems (Assignment 5)** at IIIT-Delhi.

> Motivation: a hand-written Pthreads version of a simple array sum runs about **3× the lines of code** of its sequential version. `SimpleMultithreader` collapses that overhead into one call so the parallel version looks almost identical to the sequential one.

---

## What it does

Parallelizing a loop with raw Pthreads means defining an argument struct, writing a worker function with the right signature, manually slicing the index range, creating each thread, and joining them all. `SimpleMultithreader` packages all of that into two overloaded functions:

```cpp
// 1D: parallelize a single loop over [low, high)
void parallel_for(int low, int high,
                  std::function<void(int)> &&lambda,
                  int numThreads);

// 2D: parallelize a nested loop (outer + inner)
void parallel_for(int low1, int high1, int low2, int high2,
                  std::function<void(int, int)> &&lambda,
                  int numThreads);
```

Sequential vs. parallel, side by side:

```cpp
// Sequential
for (int i = 0; i < size; i++)
    C[i] = A[i] + B[i];

// Parallel — same body, wrapped in a lambda
parallel_for(0, size, [&](int i) {
    C[i] = A[i] + B[i];
}, numThreads);
```

---

## Quick start

```bash
# Build both example programs
make

# Parallel vector addition:  ./vector <numThreads> <size>
./vector 4 48000000

# Parallel matrix multiplication:  ./matrix <numThreads> <size>
./matrix 8 1024

# Remove the binaries
make clean
```

The library is **header-only** — just `#include "simple-multithreader.h"` in any C++11 program to use it.

---

## How it works

The implementation rests on five pieces:

**1. Transparent `main` wrapping.** The header defines its own `main()` and ends with `#define main user_main`. When a program writes `int main(...)`, the preprocessor silently rewrites it to `user_main(...)`. The real entry point becomes the header's `main()`, which prints a welcome banner, calls the user's code, then prints a closing banner — all without the user changing a line.

**2. Argument structs.** Because `pthread_create` accepts only a single `void*`, each thread's index range and a pointer to the lambda are bundled into a small POD struct (`ThreadArgs1D` / `ThreadArgs2D`).

**3. Worker functions.** `worker_1d` and `worker_2d` cast the `void*` back to the struct and invoke the lambda across the slice of the loop assigned to that thread.

**4. Work distribution with load balancing.** The range is split as `chunk_size = total / numThreads` with `remainder = total % numThreads`, and the first `remainder` threads each receive one extra element. This spreads leftover work evenly instead of overloading a single thread.

**5. The main thread participates.** Only `numThreads − 1` worker threads are spawned; the main thread computes the final chunk itself. This guarantees **exactly `numThreads`** threads of execution, including main.

Lambdas are stored by pointer rather than copied. This is safe because every thread is joined before `parallel_for` returns, so the lambda always outlives the threads that reference it.

---


## Project structure

```
Assignment-5/
├── simple-multithreader.h   # the library (header-only)
├── vector.cpp               # example: parallel vector addition
├── matrix.cpp               # example: parallel matrix multiplication
├── Makefile                 # build rules
└── README.md
```

The two example programs are used as-is to exercise the 1D and 2D APIs.

---

## Requirements

- A C++11-capable compiler (`g++`)
- POSIX threads (`-lpthread`)
- A Unix-like environment (Linux or WSL)

Built with: `g++ -O3 -std=c++11 -lpthread`

---

## Authors

- **Divanshu Jain** — 2024198
- **Parth Choyal** — 2024403
