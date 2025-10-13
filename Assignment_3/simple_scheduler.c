#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <poll.h>

typedef struct{
    pid_t *data;
    int cap; //allocated cap
    int head;
    int tail;
    int size;
} Queue;

//queue implementation

static void q_init(Queue *q, int cap){
    q->data =(pid_t*)malloc(sizeof(pid_t)* cap);
    if(!q->data){
        perror("malloc");
        exit(1);
    }
    q->cap= cap;
    q->head= q->tail= q->size = 0;
}

static void q_free(Queue *q) {
    free(q->data); 
}


static int q_empty(Queue *q) {
    return q->size == 0; 
}

static void q_grow(Queue *q){
    int ncap= q->cap> 0 ? q->cap*2 : 64;
    pid_t *nd= (pid_t*)malloc(sizeof(pid_t)* ncap);
    if(!nd){
        perror("malloc");
        exit(1);
    }
    for(int i= 0;i < q->size;i++)
        nd[i]= q->data[(q->head + i) % q->cap];

    free(q->data);
    q->data= nd;
    q->cap= ncap;
    q->head= 0;
    q->tail= q->size;
}

static void q_push(Queue *q, pid_t v){
    if(q->size == q->cap){
        q_grow(q);
    }
    q->data[q->tail]= v;
    q->tail= (q->tail + 1) % q->cap;
    q->size++;
}

static pid_t q_pop(Queue *q) {
    if(q->size == 0){
        return -1;
    }
    pid_t v= q->data[q->head];
    q->head= (q->head + 1) % q->cap;
    q->size--;
    return v;
}

//utility fucntions
static int fd_nonblock(int fd) { //makes read non blocking, it instead returns EAGAIN
    int flags= fcntl(fd, F_GETFL, 0);
    if(flags== -1){ 
        return -1;
    }
    return fcntl(fd,F_SETFL,flags |O_NONBLOCK);
}

static int alive(pid_t p){  //checks if process alive, useful for scheduler
    if(p <= 0) {
        return 0;
    }
    if(kill(p, 0) == 0){
        return 1;
    }

    if(errno == ESRCH){
        return 0;
    }

    return 0;
}

static void sleep_ms(int ms) { //helps simulate scheduling, periodically scheduler wakes
    struct timespec ts;
    ts.tv_sec= ms / 1000;
    ts.tv_nsec= (ms % 1000) * 1000000L;
    while(nanosleep(&ts, &ts)== -1 &&errno== EINTR) {}
}

//read-commands
static void drain_new_jobs(int rfd, Queue *ready, int *exit_requested){
    static char partial[512];
    static int partial_length= 0;
    char buf[256];

    for(;;){ //repeatedly reads
        ssize_t n= read(rfd, buf, sizeof(buf));
        if (n < 0){
            if (errno== EAGAIN || errno== EWOULDBLOCK) break;
            perror("read");
            break;
        }
        if (n == 0) break; // EOF

        //Append new bytes to partial buffer, checks for overflow also
        if (partial_length + n >= (int)sizeof(partial)) {
            // Drop overflow
            partial_length = 0;
        }
        memcpy(partial+ partial_length, buf, n); //copy to buffer
        partial_length+= n; //buffer size track

        int processed = 0;
        
        for (int i = 0; i < partial_length; i++){
            if(partial[i]== '\n'){
                partial[i]= '\0';
                char *line=partial + processed;

                if(strncmp(line, "ADD ", 4) == 0) { //if ADD in the content from shell
                    pid_t p= (pid_t)strtol(line + 4, NULL, 10); //check for pid if first 4 are ADD
                    if(p > 0 && alive(p)){
                        q_push(ready, p);
                    }
                }

                else if (strcmp(line, "EXIT") == 0){
                    *exit_requested= 1; //if user says exit
                }

                processed= i + 1;
            }
        }

        // Move remaining partial data to front
        if (processed > 0) {
            int rem = partial_length - processed;
            if (rem > 0) memmove(partial, partial + processed, rem);
            partial_length = rem;
        }
    }
}

int main(int argc, char **argv){
    if(argc < 5){
        fprintf(stderr, "Usage: %s <NCPU> <TSLICE_MS> <read_fd> <report_fd>\n", argv[0]);
        return 1;
    }

    int NCPU= atoi(argv[1]);
    int TSLICE= atoi(argv[2]);
    int rfd= atoi(argv[3]); //shell-> scheduler pipe
    int wfd= atoi(argv[4]); //scheduler-> shell pipe

    if(NCPU < 1){
        NCPU = 1;
    }

    if(NCPU > 256){
        NCPU = 256;
    }

    if(TSLICE < 1){
        TSLICE = 1;
    }

    if(fd_nonblock(rfd)== -1){
        perror("fd_nonblock(rfd)");
    }

    Queue ready;
    q_init(&ready, 64);
    int exit_requested = 0;

    while (1){
        // Drain any queued ADD/EXIT commands
        drain_new_jobs(rfd, &ready, &exit_requested);

        // If no processes to run
        if (q_empty(&ready)) {
            if(exit_requested){ 
                break;
            }
            struct pollfd pfd;        //ensures schdeuler does not waste cpu cycles when waiting as mention in assignment
            pfd.fd= rfd;  //waits for input on file descriptor rfd
            pfd.events= POLLIN; //ready-to-read events
            int rc= poll(&pfd, 1, -1); // block until data
            if(rc < 0){
                if(errno== EINTR){
                    continue;
                }
                perror("poll");
                break;
            }
            drain_new_jobs(rfd, &ready, &exit_requested);
            continue;
        }

        //pick upto NCPU runnable pids
        pid_t runset[256];
        int runN= 0; 
        int scanned = 0;
        int qsize = ready.size;

        while (runN < NCPU && scanned < qsize && !q_empty(&ready)) {
            pid_t p = q_pop(&ready);
            scanned++;
            if (alive(p)) runset[runN++] = p;
        }

        //starts selected processes and reports
        for (int i = 0; i < runN; i++) {
            pid_t p = runset[i];
            if (kill(p, SIGCONT) == -1 && errno != ESRCH)
                perror("kill(SIGCONT)");

            if (dprintf(wfd, "RUN %d\n", (int)p) < 0)
                perror("dprintf");
        }

        //run for one quantum
        sleep_ms(TSLICE);

        //stop and re-enqueue remaining processes
        for (int i = 0; i < runN; i++){
            pid_t p= runset[i];
            if(!alive(p)){
                continue;
            }
            if(kill(p, SIGSTOP) == -1 && errno != ESRCH){
                perror("kill(SIGSTOP)");
            }
            else{
                q_push(&ready, p);
            }
        }

        // Check for new arrivals (they join next quantum)
        drain_new_jobs(rfd, &ready, &exit_requested);

        if(exit_requested && q_empty(&ready))
            break;
    }

    q_free(&ready);
    return 0;
}
