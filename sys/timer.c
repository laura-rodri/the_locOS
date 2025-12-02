#include "timer.h"
#include "clock_sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Timer waits for {interval} clock ticks and generates an interruption
void* timer_function(void* arg) {
    Timer* timer = (Timer*)arg;
    
    while (timer->running) {
        pthread_mutex_lock(&clk_mutex);
        
        // Wait for enough clock ticks to pass since last interruption
        while (timer->running && (clk_counter - timer->last_tick) < timer->interval) {
            pthread_cond_wait(&clk_cond, &clk_mutex);
        }
        
        if (!timer->running) {
            pthread_mutex_unlock(&clk_mutex);
            break;
        }
        
        // Generate timer interruption
        timer->last_tick = clk_counter;
        printf("Timer %d interrupted at tick %d (interval=%d)\n", 
               timer->id, clk_counter, timer->interval);
        fflush(stdout);
        
        // Execute callback if provided
        if (timer->callback) {
            timer->callback(timer->id, timer->user_data);
        }
        
        pthread_mutex_unlock(&clk_mutex);
    }
    return NULL;
}

// Create a new timer struct (including thread) with given parameters
Timer* create_timer(int id, int interval, timer_callback_t callback, void* user_data) {
    Timer* timer = malloc(sizeof(Timer));
    if (!timer) return NULL;

    timer->id = id;
    timer->interval = interval;
    timer->last_tick = clk_counter;
    timer->running = 1;
    timer->callback = callback;
    timer->user_data = user_data;

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
        pthread_mutex_lock(&clk_mutex);
        timer->running = 0;
        pthread_cond_broadcast(&clk_cond);  // Wake up the timer
        pthread_mutex_unlock(&clk_mutex);
        
        pthread_join(timer->thread, NULL);
        free(timer);
    }
}
