#ifndef TIMER_H
#define TIMER_H

#include <pthread.h>

// Timer structure
typedef struct {
    int id;
    int interval;    // Interval of clock ticks after which interrupts
    int last_tick;   // Last processed clock tick
    pthread_t thread;
    volatile int running;  // Flag to control timer execution
} Timer;

// Function declarations
Timer* create_timer(int id, int interval);
void destroy_timer(Timer* timer);
void* timer_function(void* arg);

#endif // TIMER_H
