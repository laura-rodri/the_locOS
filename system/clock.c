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
    int capacity;
    int size;
};

struct Timer {
    int id;          // Unique identifier for the timer
    int interval;    // Interrupt interval in clock ticks
    int ticks;       // Current tick count
    int last_tick;   // Last processed clock tick
    pthread_t thread; // Thread handling the timer
};

// Global frequency of the system clock in microseconds
int frequency = 1000000; // Default to 1 MHz

// Synchronization primitives
static pthread_mutex_t clk_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t clk_cond1 = PTHREAD_COND_INITIALIZER;
static pthread_cond_t clk_cond2 = PTHREAD_COND_INITIALIZER;

static int clk_counter = 0;

// Timer waits for clock ticks and generates interruptions
void* timer_function(void* arg) {
    struct Timer* timer = (struct Timer*)arg;
    pthread_mutex_lock(&clk_mutex);
    while (1) {
        clk_counter++;
        pthread_cond_signal(&clk_cond1);
        // Check if it's time to generate an interrupt
        while (clk_counter - timer->last_tick < timer->interval) {
            pthread_cond_wait(&clk_cond2, &clk_mutex);
        }
        // Generate timer interrupt
        timer->last_tick = clk_counter;
        printf("Timer %d interrupt at tick %d\n", timer->id, clk_counter);
    }
    
}

/**
 * System clock thread
 * Generates periodic clock ticks at specified frequency
 */
void* clock_function(void* arg) {
    while (1) {
        pthread_mutex_lock(&clk_mutex);
        while(clk_counter < frequency) {
            pthread_cond_wait(&clk_cond1, &clk_mutex);
        }
        clk_counter=0;
        pthread_cond_broadcast(&clk_cond2);
        pthread_mutex_unlock(&clk_mutex);
        printf("Clock tick\n");
        usleep(frequency);
    }
    return NULL;
}



// Initialize a new timer with given parameters
struct Timer* create_timer(int id, int interval) {
    struct Timer* timer = malloc(sizeof(struct Timer));
    
    timer->id = id;
    timer->interval = interval;
    timer->ticks = 0;
    timer->last_tick = -1;
    
    int ret = pthread_create(&timer->thread, NULL, timer_function, (void*)timer);
    if (ret != 0) {
        printf("Error creating timer thread: %s\n", strerror(ret));
        free(timer);
        return NULL;
    }
    
    return timer;
}

// Clean up system resources
void cleanup_system(pthread_t clock_thread, pthread_t* timer_threads, int num_timers) {
    pthread_cancel(clock_thread);
    pthread_join(clock_thread, NULL);
    
    for(int i=0; i<num_timers; i++) {
        pthread_cancel(timer_threads[i]);
        pthread_join(timer_threads[i], NULL);
    }
    
    pthread_mutex_destroy(&clk_mutex);
    pthread_cond_destroy(&clk_cond1);
    pthread_cond_destroy(&clk_cond2);
}

int main(int argc, char *argv[]) {
    double process_interval;
    // Parse command line arguments
    if( argc==2 && argv[1]=="-help"){
        printf("Usage: %s [frequency] [process_interval]\n", argv[0]);
        return 0;
    }
    frequency = (argc > 1) ? 1000000/atof(argv[1]) : 1000000;
    process_interval = (argc > 2) ? atof(argv[2]) : 5;
    
   
    pthread_t clk_thread;
    // Initialize system clock
    pthread_create(&clk_thread, NULL, (void*)clock_function, NULL);
    
    // Create timer threads
    const int NUM_TIMERS = 2;
    struct Timer *timers[NUM_TIMERS];
    for(int i=0; i<NUM_TIMERS; i++) {
        if ((timers[i] = create_timer(i, (int)(process_interval*1000)))== NULL) {
            return 1;
        }
    }
    
    // Run system for specified duration
    printf("System running at %d Hz. Press Ctrl+C to exit...\n", frequency);
    sleep(10);
    
    // Cleanup and exit
    cleanup_system(clk_thread, (pthread_t*)timers, NUM_TIMERS);
    for(int i=0; i<NUM_TIMERS; i++) { 
        free(timers[i]);
    }
    printf("System shutdown complete\n");
    
    return 0;
}