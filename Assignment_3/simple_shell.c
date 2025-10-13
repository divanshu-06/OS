#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>

typedef struct {
    pid_t pid;
    char  name[256];
    long  submit_ms;
    long  finish_ms;  // 0 until finished
    long  run_quanta; // count of RUN events reported by scheduler
    int   in_use;
} Job;

static Job *jobs=NULL;
static int jobs_cap=0;
static int jobs_count=0;

static int ncpu=1;
static int tslice_ms=100;

static int pipe_cmd[2];  // shell -> scheduler
static int pipe_rep[2];  // scheduler -> shell
static pid_t scheduler_pid=-1;

static long now_ms(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return ts.tv_sec*1000L+ts.tv_nsec/1000000L;
}

static int fd_set_nonblock(int fd){
    int flags=fcntl(fd,F_GETFL,0);
    if(flags==-1) return -1;
    return fcntl(fd,F_SETFL,flags | O_NONBLOCK);
}

static Job* add_job(pid_t pid,const char *name){
    if(jobs_count==jobs_cap){
        int ncap;

        if (jobs_cap != 0) {
            ncap = jobs_cap * 2; //double it
        } else {
            //if jobs_cap is zero (meaning it's the first time),
            // set the initial capacity to 64.
            ncap = 64;
        }
        Job *nj=(Job*)realloc(jobs,sizeof(Job)*ncap);
        if(!nj){
            perror("realloc");
            return NULL;
        }
        jobs=nj;
        for(int i=jobs_cap;i<ncap;i++) 
            jobs[i].in_use=0;
        jobs_cap=ncap;
    }
    for(int i=0;i<jobs_cap;i++){
        if(!jobs[i].in_use){
            jobs[i].in_use=1;
            jobs[i].pid=pid;
            strncpy(jobs[i].name,name ? name : "(unknown)",sizeof(jobs[i].name)-1);
            jobs[i].name[sizeof(jobs[i].name)-1]='\0';
            jobs[i].submit_ms=now_ms();
            jobs[i].finish_ms=0;
            jobs[i].run_quanta=0;
            jobs_count++;
            return &jobs[i];
        }
    }
    return NULL;
}

static Job* find_job(pid_t pid){
    for(int i=0;i<jobs_cap;i++){
        if(jobs[i].in_use&&jobs[i].pid==pid) return &jobs[i];
    }
    return NULL;
}

static volatile sig_atomic_t sigchld_flag=0;
static void sigchld_handler(int sig){
    (void)sig;
    sigchld_flag=1;
}

static void reap_children(void){
    int st;
    while(1){
        pid_t p=waitpid(-1,&st,WNOHANG);
        if(p<=0) 
            break;
        Job *j=find_job(p);
        if(j&&j->finish_ms==0){
            j->finish_ms=now_ms();
        }
    }
}

static int all_jobs_finished(void){
    int finished=0,total=0;
    for(int i=0;i<jobs_cap;i++){
        if(jobs[i].in_use){
            total++;
            if(jobs[i].finish_ms>0) 
                finished++;
        }
    }
    return (total>0&&finished==total)||(total==0);
}





// format  "RUN <pid>\n"
// Accumulator This is the workspace. Since a read() might only give you half a message (eg "RUN 12" instead of the full "RUN 1234\n"),this buffer "accumulates" the pieces until a complete line is formed.
static void report_reader_drain(void){
    static char accum[1024];
    static int a_len=0;
    char buf[512];

    while(1){
        ssize_t n=read(pipe_rep[0],buf,sizeof(buf));
        if(n<0){
            if(errno==EAGAIN||errno==EWOULDBLOCK) //pipe empty for not
                break;  
            if(errno==EINTR) // read interrupted
                continue; 
            perror("read(report pipe)");
            break;
        }
        if(n==0) // EOF (scheduler closed)
            break;


        if(a_len+n >= (int)sizeof(accum)){
            // overflow protection: reset accumulator (avoid corruption)
            a_len=0;
        }
        memcpy(accum+a_len,buf,n);
        a_len += n;

        int processed=0;
        for(int i=0;i<a_len;i++){
            if(accum[i]=='\n'){
                accum[i]='\0';
                char *line=accum+processed;


if(strncmp(line,"RUN ",4)==0){
    pid_t p=(pid_t)strtol(line+4,NULL,10); //gets pid by slipping first few characters
    Job *j=find_job(p);
    if(j&&j->finish_ms==0) {  // Only count if still running
        j->run_quanta++; //counting no. of slices given
    }
}
            processed=i+1;
            }
        }
        if(processed>0){
            int rem=a_len-processed;  //rem calculates the number of remaining 
            //bytes in the buffer (which would be part of an incomplete line).
            if(rem>0) //mpves the unprocesses data to the start
                memmove(accum,accum+processed,rem); 
            a_len=rem;
        }
    }
}

static void print_summary_and_exit(void){
    printf("\n Job Summary (times in multiples of %d ms) \n",tslice_ms);
    for(int i=0;i<jobs_cap;i++){
        if(!jobs[i].in_use) 
            continue;

        Job *j=&jobs[i];

        long end_time_ms;

        if (j->finish_ms>0) {
            // If the job has a valid finish time 
            end_time_ms=j->finish_ms;
        } else {
            // the job is still running so use the current time.
            end_time_ms=now_ms();
        }

        // Then,calculate the total duration.
        long dur_ms=end_time_ms-j->submit_ms;

        
        if(dur_ms<0) 
        dur_ms=0;
        long completion_q=(dur_ms+tslice_ms-1) / tslice_ms;
        if(completion_q<1) 
            completion_q=1;
        if(j->run_quanta<0) 
            j->run_quanta=0;
        long wait_q=completion_q-j->run_quanta;
        if(wait_q<0) 
            wait_q=0;
        printf("%s  pid=%d  completion=%ld  wait=%ld\n",
               j->name,(int)j->pid,
               completion_q * (long)tslice_ms,
               wait_q * (long)tslice_ms);
    }
}

static void launch_scheduler(int ncpu,int tslice){
    if(pipe(pipe_cmd)==-1||pipe(pipe_rep)==-1){
        perror("pipe");
        exit(1);
    }

    // Make the report-read end non-blocking so we can poll/select it.
    if(fd_set_nonblock(pipe_rep[0])==-1){
        perror("fd_set_nonblock(pipe_rep[0])");
    }

    scheduler_pid=fork();
    if(scheduler_pid<0){
        perror("fork");
        exit(1);
    }

    if(scheduler_pid==0){
        // Child exec simple_scheduler 
        close(pipe_cmd[1]);   // child reads from cmd[0] 
        close(pipe_rep[0]);   // child writes to rep[1] 

        char ncpu_s[32],tslice_s[32],rfd_s[32],wfd_s[32];
        snprintf(ncpu_s,sizeof(ncpu_s),"%d",ncpu);
        snprintf(tslice_s,sizeof(tslice_s),"%d",tslice);
        snprintf(rfd_s,sizeof(rfd_s),"%d",pipe_cmd[0]);
        snprintf(wfd_s,sizeof(wfd_s),"%d",pipe_rep[1]);

        execl("./simple_scheduler","./simple_scheduler",ncpu_s,tslice_s,rfd_s,wfd_s,(char*)NULL);
     
        perror("execl simple_scheduler");
        _exit(127);
    } else {
        // Parent (shell) 
        close(pipe_cmd[0]);/* shell writes to cmd[1] */
        close(pipe_rep[1]);/* shell reads from rep[0] */

        // to check scheduler didn't immediately die (common exec error) 
        for(int i=0;i<10;i++){
            if(kill(scheduler_pid,0)==0) 
                break;// alive

            // if kill fails it sets a global var names errno which stores the error code
            if(errno==ESRCH){ //  error searching no such process
                // Scheduler died cleanup
                int st;
                waitpid(scheduler_pid,&st,0);//remove zombie put error repote in st
                fprintf(stderr,"scheduler process terminated unexpectedly\n");
                exit(1);
            }

            // if neither alive nor dead waiit
            usleep(1000);//pause for microsec
            
        }
    }
}

static void send_cmd(const char *fmt,...){
    char line[256];
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(line,sizeof(line),fmt,ap);
    va_end(ap);
    if(dprintf(pipe_cmd[1],"%s\n",line)<0){
        perror("dprintf(send_cmd)");
    }
}

static void prompt(void){
    fflush(stdout);//Clears any pending output from stdout buffer
    printf("SimpleShell$ ");
    fflush(stdout);
}

int main(int argc,char **argv){
    if(argc != 3){
        fprintf(stderr,"Usage: %s <NCPU> <TSLICE_MS>\n",argv[0]);
        return 1;
    }
    //ASCII string to integer
    ncpu=atoi(argv[1]);
    tslice_ms=atoi(argv[2]);
    if(ncpu<1) ncpu=1;
    if(tslice_ms<1) tslice_ms=1;

    // Setup SIGCHLD handler to reap children 
    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler=sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=SA_RESTART | SA_NOCLDSTOP;
    if(sigaction(SIGCHLD,&sa,NULL)==-1){
        perror("sigaction");
        return 1;
    }

    launch_scheduler(ncpu,tslice_ms);

    /* Prepare non-blocking stdin and report pipe */
    int fd_stdin=fileno(stdin);
    if(fd_set_nonblock(fd_stdin)==-1){
        perror("fd_set_nonblock(stdin)");
    }
    if(fd_set_nonblock(pipe_rep[0])==-1){
        perror("fd_set_nonblock(pipe_rep[0])");
    }

    prompt();

    char line[512];
    while(1){
        /* Handle SIGCHLD */
        if(sigchld_flag){
            sigchld_flag=0;
            reap_children();
        }

        // Drain any incoming RUN reports from scheduler 
        report_reader_drain();

        // Setup select on stdin and report pipe 
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd_stdin,&rfds);
        FD_SET(pipe_rep[0],&rfds);
        int maxfd=(fd_stdin>pipe_rep[0]) ? fd_stdin : pipe_rep[0];

        struct timeval tv;
        tv.tv_sec=0;
        tv.tv_usec=200000;/* 200ms */

        int rv=select(maxfd+1,&rfds,NULL,NULL,&tv);//tells the os to resume the execution either if data is ready or after 200ma which ever happens first

        if(rv<0){
            if(errno==EINTR) continue;
            perror("select");
            break;
        }

        if(FD_ISSET(pipe_rep[0],&rfds)){  // if scheduler sent a message 
            report_reader_drain();
        }

        if(FD_ISSET(fd_stdin,&rfds)){    // if user types something
         
            if(!fgets(line,sizeof(line),stdin)){  //is ctrl d is pressed the fgets returns Null
               
                strcpy(line,"exit\n");
            }
            /* Trim trailing whitespace/newline */
            size_t L=strlen(line);
            while(L>0&&(line[L-1]=='\n' ||line[L-1]==' '||line[L-1]=='\t')) 
                line[--L]='\0';

            if(strcmp(line,"exit")==0){ 
                //Tell scheduler to drain and stop 
                send_cmd("EXIT");

                // Wait until all submitted children are finished 
                while(!all_jobs_finished()){
                    report_reader_drain();
                    if(sigchld_flag){
                        sigchld_flag=0;
                        reap_children();
                    }
                    usleep(50000);
                }

                //Wait for scheduler process to exit
                int st;
                if(waitpid(scheduler_pid,&st,0)==-1){
                    perror("waitpid(scheduler)");
                }

                print_summary_and_exit();

                break;
            } else if(strncmp(line,"submit ",7)==0){
                char *path=line+7;
                while(*path==' '||*path=='\t') 
                    path++;
                if(*path=='\0'){
                    fprintf(stderr,"Usage: submit <executable>\n");
                    prompt();
                    continue;
                }
                pid_t c=fork();
                if(c<0){
                    perror("fork");
                } else if(c==0){
                    // Child execute job also use dummy
                    execl(path,path,(char*)NULL);
                    perror("exec");
                    _exit(127);
                } else {
                    if(!add_job(c,path)){
                        fprintf(stderr,"Failed to record job for pid %d\n",(int)c);
                    }
                    send_cmd("ADD %d",(int)c);
                }
            } else if(line[0] != '\0'){
                fprintf(stderr,"Unknown command. Use submit <path>  or  exit\n");
            }
            prompt();
        }
    }

    // Cleanup 
    close(pipe_cmd[1]);
    close(pipe_rep[0]);
    free(jobs);
    return 0;
}


