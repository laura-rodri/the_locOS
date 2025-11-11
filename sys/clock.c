#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>


#define RUNNING 0
#define WAITING 1
#define TERMINATED 2

typedef struct {
    int pid;
    int state;
    // etc
} PCB;

typedef struct {
    struct PCB** queue;
    int front;
    int rear;
    int max_capacity;
    int current_size;
} ProcessQueue;

// Machine -> CPU -> Core (PCBs of kernel threads)
typedef struct {
    int num_kernel_threads;
    PCB* pcbs;
} Core;
typedef struct {
    int num_cores;
    Core* cores;
} CPU;
typedef struct {
    int num_CPUs;
    CPU* cpus;
} Machine;



typedef struct {
    int id;
    int interval;    // Interval of clock ticks after which interrupts
    int last_tick;   // Last processed clock tick
    pthread_t thread;
} Timer;


// Global frequency of the clock. Default 1 Hz
int CLOCK_FREQUENCY_HZ = 1;

static pthread_mutex_t clk_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t clk_cond = PTHREAD_COND_INITIALIZER;  // For signaling clock ticks
static pthread_cond_t timer_cond = PTHREAD_COND_INITIALIZER; // For signaling timer jobs. Currently unaffect the program
static volatile int clk_counter = 0;  // Global tick counter

// Global variables for cleanup
pthread_t clk_thread_global;
Timer** timers_global;
int num_timers_global;

// Timer waits for {interval} clock ticks and generates an interruption
void* timer_function(void* arg) {
    Timer* timer = (Timer*)arg;
    
    while (1) {
        pthread_mutex_lock(&clk_mutex);
        
        // Wait for enough clock ticks to pass since last interruption
        while ((clk_counter - timer->last_tick) < timer->interval) {
            pthread_cond_wait(&clk_cond, &clk_mutex);
        }
        
        // Generate timer interruption
        timer->last_tick = clk_counter;
        printf("Timer %d interrupted at tick %d (interval=%d)\n", timer->id, clk_counter, timer->interval);
        fflush(stdout);
        
        pthread_cond_signal(&timer_cond);
        pthread_mutex_unlock(&clk_mutex);
    }
    return NULL;
}

// Clock increments clk_counter at desired frequency and signals waiting threads
void* clock_function() {
    while (1) {
        usleep(1000000 / CLOCK_FREQUENCY_HZ); // Wait one clock period
        
        pthread_mutex_lock(&clk_mutex);
        clk_counter++;
        pthread_cond_broadcast(&clk_cond);
        
        printf("Clock tick %d\n", clk_counter);
        fflush(stdout); // Force output immediately

        //pthread_cond_wait(&timer_cond, &clk_mutex);
        pthread_mutex_unlock(&clk_mutex);
    }
    return NULL;
}

// Create a new timer struct (including thread) with given parameters
Timer* create_timer(int id, int interval) {
    Timer* timer = malloc(sizeof(Timer));

    timer->id = id;
    timer->interval = interval;
    timer->last_tick = clk_counter;

    int ret = pthread_create(&timer->thread, NULL, timer_function, (void*)timer);
    if (ret != 0) {
        printf("Error creating timer thread: %s\n", strerror(ret));
        free(timer);
        return NULL;
    }
    
    return timer;
}

// Clean up system resources
void cleanup_system(pthread_t clock_thread, Timer** timers, int num_timers) {
    pthread_cancel(clock_thread);

    pthread_join(clock_thread, NULL);
    
    for (int i = 0; i < num_timers; i++) {
        if (timers[i] != NULL){
        pthread_cancel(timers[i]->thread);
        pthread_join(timers[i]->thread, NULL);
        }
    }
    
    pthread_mutex_destroy(&clk_mutex);
    pthread_cond_destroy(&clk_cond);
    pthread_cond_destroy(&timer_cond);
}

// Catch Ctrl+C to clean up and exit successfully
void handle_sigint(int sig) {
    printf("\nCaught signal %d, shutting down...\n", sig);
    cleanup_system(clk_thread_global, timers_global, num_timers_global);
    for(int i=0; i<num_timers_global; i++) { 
        free(timers_global[i]);
    }
    printf("System shutdown complete\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    // 
    int process_interval;
    // Parse command line arguments
    if (argc == 2 && strcmp(argv[1], "-help") == 0) {
        printf("Usage: %s [frequency_in_hz] [process_interval]\n", argv[0]);
        return 0;
    }
    if (argc > 1)
        CLOCK_FREQUENCY_HZ = (atoi(argv[1])>0) ? atoi(argv[1]) : 1;

    process_interval = (argc > 2) ? atoi(argv[2]) : 5; // Default 5 ticks (not milliseconds!)
    
    // Set up signal handler for Ctrl+C
    signal(SIGINT, handle_sigint);
   
    pthread_t clk_thread;
    // Create clock thread
    int ret = pthread_create(&clk_thread, NULL, clock_function, NULL);
    if (ret != 0) {
        fprintf(stderr, "Error creating clock thread: %s\n", strerror(ret));
        return 1;
    }
    clk_thread_global = clk_thread;
    
    // Create timers
    int NUM_TIMERS = 2;
    Timer *timers[NUM_TIMERS];
    for(int i=0; i<NUM_TIMERS; i++) {
        if ((timers[i] = create_timer(i, process_interval)) == NULL) {
            fprintf(stderr, "Error creating timer %d\n", i);
            return 1;
        }
    }
    timers_global = timers;
    num_timers_global = NUM_TIMERS;
    
    printf("System running at %d Hz. Press Ctrl+C to exit...\n", CLOCK_FREQUENCY_HZ);
    
    // Wait for a signal
    pause();

    // This part is now unreachable because of the signal handler
    return 0;
}