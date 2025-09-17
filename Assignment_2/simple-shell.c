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


// cleanup
void free_tokens(char **commands){
    if (!commands) return;
    for(int i=0; commands[i] != NULL; i++){
        free(commands[i]);
    }
    free(commands);
}



void execute_pipeline(char *line){
    int prev=0;
    char *line_copy= strdup(line);
    char **commands=parse_line(line);
    int fd[2];
    pid_t pid;
    pid_t first_pid=-1;
    int i;
    struct timespec start,end;
    
    // we use this as if we just use time(),for most processes it will be zero,so we use nano sec precision
    clock_gettime(CLOCK_MONOTONIC,&start);

    for (i=0;commands[i]!=NULL;i++){       
        if (pipe(fd)==-1) {
        perror("pipe");
        free_tokens(commands);
        free(line_copy); 
        return;   // abort this pipeline cleanly
        }
        pid=fork();

        if (pid==-1) {
        perror("fork");
        free_tokens(commands);
        free(line_copy); 
        close(fd[0]);
        close(fd[1]);
        return;   // abort this pipeline cleanly
        }
        if(first_pid==-1){   //in case of pipe cmds,we store the first child's pid
            first_pid=pid;
        }
    if (pid==0){
        if (prev != 0) {
                dup2(prev,STDIN_FILENO);
                close(prev);
            }
        if (commands[i+1] != NULL) {
                dup2(fd[1],STDOUT_FILENO);
            }
        close(fd[0]);
        close(fd[1]);

       char **argv=split_args(commands[i]);
            execvp(argv[0],argv);
            perror("execvp");
            free_tokens(argv);
            exit(EXIT_FAILURE);
        }
        else {
            // Parent process


            close(fd[1]);
            if (prev != 0) {
                close(prev);
            } 
            prev=fd[0];
        }
    }
    for (int j=0; j < i; j++) {
        wait(NULL);
    }

    clock_gettime(CLOCK_MONOTONIC,&end);
    double duration= (end.tv_sec-start.tv_sec)+ (end.tv_nsec-start.tv_nsec)/ 1e9; // nanosec precision

    if (history_count< MAX_HISTORY){
        history[history_count].command=line_copy;
        history[history_count].pid= first_pid;
        history[history_count].start_time= time(NULL);
        history[history_count].duration= duration;
        history_count++;
    }
    free_tokens(commands);
}



void shell_loop() {
    char *line;
    int status;

    do {
        printf("simpleshell> ");
        line=read_line();

        if (line[0]=='\0') {  // ignore empty input
            free(line);
            continue;
        }

        if (strncmp(line,"history",7)==0) {
            print_history();
            status=1;

            if (history_count < MAX_HISTORY) {
                history[history_count].command=strdup(line);
                history[history_count].pid=0;
                history[history_count].start_time=time(NULL);
                history[history_count].duration=0.0;
                history_count++;
            }

        }

        else if (strncmp(line,"exit",4)==0) {
            status=0;
            print_exit_summary();
        } 
        
        else {
            // Always run via pipeline executor (works for 1 or many commands)
            execute_pipeline(line);
            status=1;
        }

        free(line);
    } while (status);
}



int main() {
    struct sigaction sig;
    memset(&sig,0,sizeof(sig));
    sig.sa_handler=signal_handler; 
    sig.sa_flags=SA_RESTART;
    sigaction(SIGINT,&sig,NULL);
    

    shell_loop();
    cleanup_history();
    return 0;
}
