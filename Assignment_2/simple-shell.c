#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>


#define MAX_HISTORY 1000

struct history_entry{
    char *command;
    pid_t pid;
    
    time_t start_time;
    double duration;
};

struct history_entry history[MAX_HISTORY];
int history_count= 0;

void cleanup_history(){
    for (int i=0; i <history_count; i++) {
        free(history[i].command);
    }
}

void print_history(){
    printf("Command History:\n");
    for (int i=0; i < history_count; i++){
        printf("%d: %s\n",i + 1,history[i].command);
    }
}

void print_exit_summary() {
    printf("\nSimpleShell Summary\n");
    printf("total commands executed: %d\n\n",history_count);
    
    for (int i=0; i < history_count; i++) {
        printf("Command %d: %s\n",i + 1,history[i].command);
        printf("  PID: %d\n",history[i].pid);
        printf("  Execution Time: %s",ctime(&history[i].start_time));
        printf("  Duration: %.4f seconds\n\n",history[i].duration);
    }
    
    printf("Thank you! Onto the next assignment!!\n");
}

// to read the commands and assigns to a pointer
char *read_line(void) {
    char *line=NULL;
    size_t bufsize=0;

    if(getline(&line,&bufsize,stdin)==-1) {
        printf("\n");
        print_exit_summary();
        cleanup_history();
        exit(0);
    }

    return line;
}

// this function parses the commands
char **parse_line(char *line){
    int bufsize=64,pos=0;
    char **commands= malloc(bufsize * sizeof(char*));
    char *command;
  
        command= strtok(line,"|");
        while (command != NULL){
            commands[pos++]= strdup(command);
            if(pos >= bufsize) {
                bufsize*= 2;
                commands= realloc(commands,bufsize * sizeof(char*));
                if(!commands) {
                    perror("realloc");
                    exit(EXIT_FAILURE);
                }
            }

            command= strtok(NULL,"|");   //helps in going to the next pipeline command
        }
        commands[pos]= NULL;
        return commands;
}





//for sigint
void signal_handler(int sig){
    if(sig==SIGINT){
        printf("\n");
        print_exit_summary();
        cleanup_history();
        exit(0);
    }
}


//splits a single cmnd string into argv[] style
char **split_args(char *line){
    int bufsize=64,pos=0;

    char **commands=malloc(bufsize * sizeof(char *));
    char *command;

    if (!commands) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    command=strtok(line," \n");
    while (command != NULL) {
        commands[pos++]=strdup(command);
        if (pos >= bufsize) {
            bufsize *= 2;
            commands=realloc(commands,bufsize * sizeof(char *));
            if (!commands) {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
        }
        command=strtok(NULL," \n");
    }
    commands[pos]=NULL;
    return commands;
}

