#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

struct PCB {
    int pid;
    // Additional fields to be added
};

struct Machine {
    int num_kernel_threads;
    struct PCB* pcbs;  // Array of process control blocks
};

struct ProcessQueue {
    struct PCB** queue;
    int front;
    int rear;
    int max_capacity;
    int current_size;
};

struct Timer {
    int id;
    int interval;    // Interval of clock ticks after which interrupts
    int last_tick;   // Last processed clock tick
    pthread_t thread;
};

// Global frequency of the clock
int frequency = 1000000; // Default to 1 MHz

static pthread_mutex_t clk_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t clk_cond = PTHREAD_COND_INITIALIZER;  // For signaling clock ticks
static pthread_cond_t timer_cond = PTHREAD_COND_INITIALIZER; // For signaling timer jobs. Currently unaffect the program
static volatile int clk_counter = 0;  // Shared tick counter

// Timer waits for {interval} clock ticks and generates an interruption
void* timer_function(void* arg) {
    struct Timer* timer = (struct Timer*)arg;
    
    while (1) {
        pthread_mutex_lock(&clk_mutex);
        
        // Wait for enough clock ticks to pass since last interruption
        while ((clk_counter - timer->last_tick) < timer->interval) {
            pthread_cond_wait(&clk_cond, &clk_mutex);
        }
        
        // Generate timer interruption
        timer->last_tick = clk_counter;
        printf("Timer %d interrupted at tick %d (interval=%d)\n",
               timer->id, clk_counter, timer->interval);
        fflush(stdout);
        
        pthread_cond_signal(&timer_cond);
        pthread_mutex_unlock(&clk_mutex);
    }
    return NULL;
}

// Clock increments clk_counter at desired frequency and signals waiting threads
void* clock_function() {
    while (1) {
        usleep(frequency); // Wait one clock period
        
        pthread_mutex_lock(&clk_mutex);
        clk_counter++;
        pthread_cond_broadcast(&clk_cond);
        
        printf("Clock tick %d\n", clk_counter);
        fflush(stdout); // Force output immediately

        pthread_cond_wait(&timer_cond, &clk_mutex);
        pthread_mutex_unlock(&clk_mutex);
    }
    return NULL;
}



// Initialize a new timer with given parameters
struct Timer* create_timer(int id, int interval) {
    struct Timer* timer = malloc(sizeof(struct Timer));
    
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
void cleanup_system(pthread_t clock_thread, struct Timer** timers, int num_timers) {
    int ret;
    ret = pthread_cancel(clock_thread);
    if (ret != 0) {
        fprintf(stderr, "Error cancelling clock thread: %s\n", strerror(ret));
    }
    pthread_join(clock_thread, NULL);
    
    for (int i = 0; i < num_timers; i++) {
        if (timers[i] == NULL) continue;
        ret = pthread_cancel(timers[i]->thread);
        if (ret != 0) {
            fprintf(stderr, "Error cancelling timer %d: %s\n", i, strerror(ret));
        }
        pthread_join(timers[i]->thread, NULL);
    }
    
    ret = pthread_mutex_destroy(&clk_mutex);
    if (ret != 0) {
        fprintf(stderr, "Error destroying mutex: %s\n", strerror(ret));
    }
    
    ret = pthread_cond_destroy(&clk_cond);
    if (ret != 0) {
        fprintf(stderr, "Error destroying condition variable: %s\n", strerror(ret));
    }
}

int main(int argc, char *argv[]) {
    int process_interval;
    // Parse command line arguments
    if (argc == 2 && strcmp(argv[1], "-help") == 0) {
        printf("Usage: %s [frequency] [process_interval]\n", argv[0]);
        return 0;
    }
    frequency = (argc > 1) ? 1000000/atof(argv[1]) : 1000000; // Default 1s per tick
    process_interval = (argc > 2) ? atoi(argv[2]) : 5; // Default 5 ticks (not milliseconds!)
    
   
    pthread_t clk_thread;
    // Initialize system clock
    int ret = pthread_create(&clk_thread, NULL, clock_function, NULL);
    if (ret != 0) {
        fprintf(stderr, "Error creating clock thread: %s\n", strerror(ret));
        return 1;
    }
    
    // Create timer threads
    const int NUM_TIMERS = 2;
    struct Timer *timers[NUM_TIMERS];
    for(int i=0; i<NUM_TIMERS; i++) {
        if ((timers[i] = create_timer(i, i+1)) == NULL) {
            fprintf(stderr, "Error creating timer %d\n", i);
            return 1;
        }
    }
    
    // Run system for specified duration
    printf("System running at %d Hz. Press Ctrl+C to exit...\n", frequency);
    pause(); // Wait indefinitely until interrupted    
    // Cleanup and exit
    cleanup_system(clk_thread, timers, NUM_TIMERS);
    for(int i=0; i<NUM_TIMERS; i++) { 
        free(timers[i]);
    }
    printf("System shutdown complete\n");
    
    return 0;
}