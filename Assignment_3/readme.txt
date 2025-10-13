OS ASSIGNMENT 3-  SCHEDULER

Parth Choyal(2024403): queue and round-robin implementation and cpu management.

Divanshu Jain(2024198): shell-scheduler communication, signal handling.

GITHUB LINK:https://github.com/divanshu-06/OS/tree/main/Assignment_3 

## dummy_main.h: wrapper header that replaces a user program’s main() with custom entry function that waits for the scheduler’s signal before execution, ensures submitted programs start in a stopped state so that the scheduler has full control from the moment of creation.

We have implemented two pipes:

1. Shell -> Scheduler: the shell creates new processes (via fork()) when user types submit ./program, informs the scheduler. messages sent are "ADD <pid>" and "EXIT".

2. Scheduler->Shell: The scheduler reports back what it's doing. the shell needs to track how many quanta each job actually ran. This is essential for calculating wait time in the final summary.

## simple_shell: CLI for job submission
->SIGCHLD handler for process reaping, 
->it also keeps track of jobs with timing info. 
->On exit, the shell waits for all jobs to finish and then prints a completion summary table including: 
job name,pid,completion time (in multiples of tslice), wait time (in multiples of tslice).

## simple_scheduler:  
-> circulary array queue implementation, O(1) enqueue and dequeue, expands when capacity exceeded, implement round-robin scheduling policy.
->non-blocking command reading with poll() to MINIMIZE CPU usage.
->automatic cleanup of terminated processes.
->process liveness checking before scheduling.

## Scheduling Algo:
->wait for jobs using poll() when idle.
->select upto NCPU from ready queue.
->send SIGCONT to selected processes and report "RUN".
->sleep for TSLICE ms, then send sigstop to running processes.
->re-enqueue still alive processes and also check for new processes that arrive.