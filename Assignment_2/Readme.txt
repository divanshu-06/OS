
SIMPLE SHELL ASSIGNMENT:

Divanshu Jain(2024198):  execute_ pipeline, signal handler, shell_loop
Parth Choyal(2024401):  read, parse and split args, history, pid and time tracking.



parse_line and split_args:

/First we initialize buffer size and dynamically allocate memory for tokens double pointer
/Then we split the command that is given as parameter(*line) on "|" using the srtok method and point it to a pointer
/Then we loop over each token and store it in dynamically allocated array
/If the size of the array is insufficient the we use realloc which doubles the size of the array
/So we finally return an array with each command splited on "|"
/For split_args intead of splitting on "|" we split on " " and "\n".



execute_pipeline:

execute pipeline helps in executing any number of commands with pipes even a single command. We initialize a variable prev=0 to store the read end of the previous pipe to direct to the write end of the next pipe. We run a for loop to run any number of pipes. We use dup command to duplicate read/write ends to STDIN/STDOUT. We have also done some error handling in it.



shell_loop:

shell_loop implements the infinite loop of a shell .It repeatedly asks the user for input, parses commands. It implements self defined commands like history and exit, all other input commands are parsed and executed by using a execute pipeline function. The loop also handles error checks like empty line. At the end it does Cleanup using cleanup_history() function.




Implementation of read, parse and split args, history, pid and time tracking:

1. We define a struct history_entry and create an array history to record command start times, duration, pid. For pipe commands, we record PID of first command among the various piped command. Also, since we have built-in history command, we assign it pid zero.

2. To record time duration, we utilize clock_gettime function, we use this as if we just use time(), for most processes time will be zero, so we want nano second precision.

3. Later, we implement print_exit_summary to print the summary upon exiting from shell.  We create a handler function for Ctrl-C(sigint), it just calls the print_exit_summary and  exits.




 
