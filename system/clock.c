#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

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
};


// Synchronization primitives
static pthread_mutex_t clk_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t clk_tick = PTHREAD_COND_INITIALIZER;
static pthread_cond_t clk_aux_cond = PTHREAD_COND_INITIALIZER;


// System state
static int clk_counter = 0;




// Initialize a new timer with given parameters
struct Timer* create_timer(int id, int interval) {
    struct Timer* timer = malloc(sizeof(struct Timer));
    
    timer->id = id;
    timer->interval = interval;
    timer->ticks = 0;
    timer->last_tick = -1;
    
    return timer;
}

// Timer waits for clock ticks and generates interruptions
void timer_function(Timer* timer) {

    pthread_mutex_lock(&clk_mutex);
    while (1) {
        clk_counter++;
        pthread_cond_signal(&clk_aux_cond);
        // Check if it's time to generate an interrupt
        while (clk_counter - timer->last_tick < timer->interval) {
            pthread_cond_wait(&clk_tick, &clk_mutex);
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
void clock_function(int freq) {
    // Frequency in microseconds
    int sleep_time = 1000000 / freq;
    
    while (1) {
        pthread_mutex_lock(&clk_mutex);
        while(clk_counter < freq) {
            pthread_cond_wait(&clk_aux_cond, &clk_mutex);
        }
        clk_counter=0;
        pthread_cond_broadcast(&clk_tick);
        pthread_mutex_unlock(&clk_mutex);
        printf("Clock tick\n");
        usleep(sleep_time);
    }

}



/**
 * Create and start a timer with specified parameters
 */
void start_timer(pthread_t* thread, int timer_id, int interval) {
    struct Timer* timer = create_timer(timer_id, interval);
    
    if (pthread_create(thread, NULL, timer_function, timer) != 0) {
        free(timer);
        perror("Failed to create timer thread");
    }
}

/**
 * Clean up system resources
 */
void cleanup_system(pthread_t clock_thread, pthread_t* timer_threads, int num_timers) {
    // Signal threads to exit
    should_exit = 1;
    
    if (pthread_mutex_lock(&clock_mutex) != 0) {
        perror("Failed to acquire mutex during cleanup");
    }
    
    if (pthread_cond_broadcast(&clock_tick) != 0) {
        pthread_mutex_unlock(&clock_mutex);
        perror("Failed to broadcast exit signal");
    }
    
    if (pthread_mutex_unlock(&clock_mutex) != 0) {
        perror("Failed to release mutex during cleanup");
    }
    
    // Wait for threads to finish
    if (pthread_join(clock_thread, NULL) != 0) {
        perror("Failed to join clock thread");
    }
    
    for (int i = 0; i < num_timers; i++) {
        if (pthread_join(timer_threads[i], NULL) != 0) {
            perror("Failed to join timer thread");
        }
    }
    
    // Destroy synchronization primitives
    if (pthread_mutex_destroy(&clock_mutex) != 0) {
        perror("Failed to destroy mutex");
    }
    
    if (pthread_cond_destroy(&clock_tick) != 0) {
        perror("Failed to destroy condition variable");
    }
}

int main(int argc, char *argv[]) {
    // Parse command line arguments
    int frequency = (argc > 1) ? atoi(argv[1]) : 1;
   
    
    // Initialize system clock
    pthread_t clock_thread;
    pthread_create(&clock_thread, NULL, clock_function, NULL);
    
    // Create timer threads
    const int NUM_TIMERS = 2;
    pthread_t timer_threads[NUM_TIMERS];
    
    // Timer 1: Interrupt every clock tick
    start_timer(&timer_threads[0], 1, 1);
    
    // Timer 2: Interrupt every 2 clock ticks
    start_timer(&timer_threads[1], 2, 2);
    
    // Run system for specified duration
    printf("System running at %d Hz. Press Ctrl+C to exit...\n", frequency);
    sleep(10);
    
    // Cleanup and exit
    cleanup_system(clock_thread, timer_threads, NUM_TIMERS);
    printf("System shutdown complete\n");
    
    return EXIT_SUCCESS;
}