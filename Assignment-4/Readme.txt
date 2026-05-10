Github link: https://github.com/divanshu-06/OS/tree/main/Assignment-4

Divanshu Jain  2024198	SIGSEGV handler, page fault logic, mmap-based allocation 
Parth Choyal   2024198	SIGSEGV handler, ELF parsing, program header management, cleanup routines

loader.c implementation: 
This is a lazy loading ELF32 program loader.
It does not map all program segments upfront.
It allocates and loads pages on demand when a segmentation fault occurs like the Linux demand paging mechanism.

Handles ELF32 executables without glibc dependencies

Implements lazy loading using SIGSEGV signal handler

Allocates memory page-by-page (4KB) using mmap()

At the end prints

Total page faults handled
Total page allocations
Internal fragmentation (in KB)

Launcher (launcher.c)

The main function first performs all necessary checks on the arguments passed to ensure that the ELF file exists, is readable, and can be opened. Then it passes the arguments to the loader for carrying out the loading and execution. Finally, it invokes the cleanup function of the loader to free all heap memory.
Note: The launcher does not itself implement ELF loading; it only forwards the work to the lib_simpleloader.so through the functions declared in loader.h.

Makefile (launcher):

The all target builds the launch executable inside the bin/ folder.
-m32 tells to execute 32 bit architecture
-L adds bin to linkers search directory for libraries (link time)
-l tells linker to look for link with library named lib<name>.so (link time)
-WL,<option> it tells to pass the options directly to the linker and see where the binary lives (during runtime)
here $$ORIGIN is bin folder


The clean target deletes the generated bin/launch so we can rebuild it.

Makefile (test):

This Makefile builds the binary of the test program (e.g., fib).
The required flags -m32 -no-pie -nostdlib are used to execute 32 bit architecture ,disable position-independent executables (use fixed addresses) , no library linking

The clean method removes the generated binary so that a new binary can be built for different code.

Makefile (loader):

This Makefile creates the shared object file lib_simpleloader.so using 32-bit architecture.
The clean method removes the shared object file.



Outer Makefile:

invokes all the other make files in the order loader launcher and test
clean method invokes the clean methods of other make files 


Purpose: The outer Makefile invokes the Makefiles inside the loader, launcher, and test directories in the correct order.


all

Invokes make inside loader, launcher, and test directories.
The binaries lib_simpleloader.so and launch are placed inside the bin/ directory.



fib
Explicitly builds only the test program (fib) by invoking the test Makefile.

clean

Removes all binaries from the bin/ folder and deletes the test/fib executable.


clean-test

Cleans only the test folder (test/fib) by invoking the clean target in the test Makefile.



Instructions for running the code:

Open the folder with bonus directory
first run make clean  // if some of binary files are present they will be deleted
then run make  // creates all the needed binary files
the run bin/launch test/fib  // runs the binary file of fib with the help of the loader binary file(launch)

