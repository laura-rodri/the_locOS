#ifndef CLOCK_SYS_H
#define CLOCK_SYS_H

#include <pthread.h>

// Global clock frequency (Hz)
extern int CLOCK_FREQUENCY_HZ;

// Global tick counter
extern volatile int clk_counter;

// Synchronization primitives
extern pthread_mutex_t clk_mutex;
extern pthread_cond_t clk_cond;

// Function declarations
void* clock_function(void* arg);
int start_clock(pthread_t* clock_thread);
void stop_clock(pthread_t clock_thread);
int get_current_tick(void);

#endif // CLOCK_SYS_H
