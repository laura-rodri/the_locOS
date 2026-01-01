#ifndef CLOCK_SYS_H
#define CLOCK_SYS_H

#include <pthread.h>

// Forward declaration
typedef struct Machine Machine;

// Global clock frequency (Hz)
extern int CLOCK_FREQUENCY_HZ;

// Global tick counter
extern volatile int clk_counter;

// Global flag to control system execution
extern volatile int running;

// Synchronization primitives
extern pthread_mutex_t clk_mutex;
extern pthread_cond_t clk_cond;

// Machine reference for TTL decrement
extern Machine* clock_machine_ref;

// Function declarations
void* clock_function(void* arg);
int start_clock(pthread_t* clock_thread);
void stop_clock(pthread_t clock_thread);
int get_current_tick(void);
void set_clock_machine(Machine* machine);

#endif // CLOCK_SYS_H
