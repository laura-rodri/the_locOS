#ifndef TIMER_H
#define TIMER_H

#include <pthread.h>

// Callback function type for timer interrupts
typedef void (*timer_callback_t)(int timer_id, void* user_data);

// Timer structure
typedef struct {
    int id;
    int interval;    // Interval of clock ticks after which interrupts
    int last_tick;   // Last processed clock tick
    pthread_t thread;
    volatile int running;  // Flag to control timer execution
    timer_callback_t callback;  // Callback function to execute on interrupt
    void* user_data;            // User data to pass to callback
} Timer;

// Function declarations
Timer* create_timer(int id, int interval, timer_callback_t callback, void* user_data);
void destroy_timer(Timer* timer);
void* timer_function(void* arg);

#endif // TIMER_H
