#include "clock_sys.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

// Global frequency of the clock. Default 1 Hz
int CLOCK_FREQUENCY_HZ = 1;

pthread_mutex_t clk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clk_cond = PTHREAD_COND_INITIALIZER;
volatile int clk_counter = 0;

// Clock increments clk_counter at desired frequency and signals waiting threads
void* clock_function(void* arg) {
    (void)arg; // Unused parameter
    
    while (1) {
        usleep(1000000 / CLOCK_FREQUENCY_HZ); // Wait one clock period
        
        pthread_mutex_lock(&clk_mutex);
        clk_counter++;
        pthread_cond_broadcast(&clk_cond);
        
        printf("Clock tick %d\n", clk_counter);
        fflush(stdout); // Force output immediately

        pthread_mutex_unlock(&clk_mutex);
    }
    return NULL;
}

// Start the system clock
int start_clock(pthread_t* clock_thread) {
    int ret = pthread_create(clock_thread, NULL, clock_function, NULL);
    if (ret != 0) {
        fprintf(stderr, "Error creating clock thread: %s\n", strerror(ret));
        return -1;
    }
    return 0;
}

// Stop the system clock
void stop_clock(pthread_t clock_thread) {
    pthread_cancel(clock_thread);
    pthread_join(clock_thread, NULL);
}

// Get current tick count (thread-safe)
int get_current_tick(void) {
    int tick;
    pthread_mutex_lock(&clk_mutex);
    tick = clk_counter;
    pthread_mutex_unlock(&clk_mutex);
    return tick;
}
