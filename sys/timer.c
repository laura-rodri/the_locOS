#include "timer.h"
#include "clock_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        printf("Timer %d interrupted at tick %d (interval=%d)\n", 
               timer->id, clk_counter, timer->interval);
        fflush(stdout);
        
        pthread_mutex_unlock(&clk_mutex);
    }
    return NULL;
}

// Create a new timer struct (including thread) with given parameters
Timer* create_timer(int id, int interval) {
    Timer* timer = malloc(sizeof(Timer));
    if (!timer) return NULL;

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

// Destroy timer and free memory
void destroy_timer(Timer* timer) {
    if (timer) {
        pthread_cancel(timer->thread);
        pthread_join(timer->thread, NULL);
        free(timer);
    }
}
