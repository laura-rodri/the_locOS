#include "clock_sys.h"
#include "machine.h"
#include "process.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

// Global frequency of the clock. Default 1 Hz
int CLOCK_FREQUENCY_HZ = 1;

pthread_mutex_t clk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clk_cond = PTHREAD_COND_INITIALIZER;
volatile int clk_counter = 0;

// Machine reference to decrement TTL of executing processes
Machine* clock_machine_ref = NULL;

// Clock increments clk_counter at desired frequency, decrements TTL of executing processes,
// and signals waiting threads
void* clock_function(void* arg) {
    (void)arg; // Unused parameter
    
    while (running) {
        usleep(1000000 / CLOCK_FREQUENCY_HZ); // Wait one clock period
        
        // Check again after sleep in case signal arrived during sleep
        if (!running) break;
        
        pthread_mutex_lock(&clk_mutex);
        clk_counter++;
        
        printf("\033[33mClock tick %d\033[0m\n", clk_counter);
        fflush(stdout); // Force output immediately
        
        // CRITICAL: The system clock "moves" the executing processes
        // by decrementing their TTL on each tick
        if (clock_machine_ref) {
            for (int i = 0; i < clock_machine_ref->num_CPUs; i++) {
                for (int j = 0; j < clock_machine_ref->cpus[i].num_cores; j++) {
                    Core* core = &clock_machine_ref->cpus[i].cores[j];
                    
                    // Decrement TTL for each executing process
                    for (int k = 0; k < core->current_pcb_count; k++) {
                        PCB* pcb = &core->pcbs[k];
                        int old_ttl = pcb->ttl;
                        int new_ttl = decrement_pcb_ttl(pcb);
                        
                        printf("[Clock] CPU%d-Core%d-Thread%d: PID=%d TTL: %d -> %d\n",
                               i, j, k, pcb->pid, old_ttl, new_ttl);
                        fflush(stdout);
                    }
                }
            }
        }
        
        pthread_cond_broadcast(&clk_cond);
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

// Set the machine reference for the clock to decrement TTL
void set_clock_machine(Machine* machine) {
    pthread_mutex_lock(&clk_mutex);
    clock_machine_ref = machine;
    pthread_mutex_unlock(&clk_mutex);
}
