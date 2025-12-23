#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "machine.h"
#include "process.h"
#include "clock_sys.h"
#include "timer.h"

// Global variables for cleanup
static pthread_t clk_thread_global;
static Timer** timers_global = NULL;
static int num_timers_global = 0;
static ProcessGenerator* proc_gen_global = NULL;
static Scheduler* scheduler_global = NULL;
static ProcessQueue* ready_queue_global = NULL;
static ProcessQueue* active_queue_global = NULL;
static volatile int running = 1;  // Flag to control main loop

// Clean up system resources
void cleanup_system(pthread_t clock_thread, Timer** timers, int num_timers) {
    printf("Stopping scheduler...\n");
    fflush(stdout);
    
    // Stop scheduler and get current process if any
    PCB* current_proc = NULL;
    if (scheduler_global) {
        stop_scheduler(scheduler_global);
        current_proc = scheduler_get_current_process(scheduler_global);
        
        if (current_proc) {
            printf("\tCurrent running process: PID=%d (TTL=%d)\n", 
                   current_proc->pid, current_proc->ttl);
            fflush(stdout);
        } else {
            printf("\tNo process was running\n");
            fflush(stdout);
        }
        
        destroy_scheduler(scheduler_global);
    }
    
    printf("Stopping process generator...\n");
    fflush(stdout);
    
    // Stop process generator
    if (proc_gen_global) {
        stop_process_generator(proc_gen_global);
        destroy_process_generator(proc_gen_global);
    }
    
    printf("Stopping clock...\n");
    fflush(stdout);
    stop_clock(clock_thread);
    
    printf("Stopping timers...\n");
    fflush(stdout);
    
    // Wake up all waiting threads before canceling
    pthread_mutex_lock(&clk_mutex);
    pthread_cond_broadcast(&clk_cond);
    pthread_mutex_unlock(&clk_mutex);
    
    for (int i = 0; i < num_timers; i++) {
        if (timers[i] != NULL) {
            destroy_timer(timers[i]);
        }
    }
    
    printf("Cleaning ready queue...\n");
    fflush(stdout);
    // Clean up ready queue and remaining processes
    if (ready_queue_global) {
        printf("\tNumber of processes: %d\n", ready_queue_global->current_size);
        if (ready_queue_global->current_size > 0) {
            printf("\tProcess IDs: ");
            int count = ready_queue_global->current_size;
            int idx = ready_queue_global->front;
            for (int i = 0; i < count; i++) {
                PCB* pcb = (PCB*)ready_queue_global->queue[idx];
                printf("%d ", pcb->pid);
                idx = (idx + 1) % ready_queue_global->max_capacity;
            }
            printf("\n");
        }
        fflush(stdout);
        
        PCB* pcb;
        while ((pcb = dequeue_process(ready_queue_global)) != NULL) {
            destroy_pcb(pcb);
        }
        destroy_process_queue(ready_queue_global);
    }
    
    printf("Cleaning active queue...\n");
    fflush(stdout);
    // Clean up active queue and remaining processes
    if (active_queue_global) {
        printf("\tNumber of processes: %d\n", active_queue_global->current_size);
        if (active_queue_global->current_size > 0) {
            printf("\tProcess IDs: ");
            int count = active_queue_global->current_size;
            int idx = active_queue_global->front;
            for (int i = 0; i < count; i++) {
                PCB* pcb = (PCB*)active_queue_global->queue[idx];
                printf("%d ", pcb->pid);
                idx = (idx + 1) % active_queue_global->max_capacity;
            }
            printf("\n");
        }
        fflush(stdout);
        
        PCB* pcb;
        while ((pcb = dequeue_process(active_queue_global)) != NULL) {
            destroy_pcb(pcb);
        }
        destroy_process_queue(active_queue_global);
    }
    
    // Destroy current process from scheduler if it exists
    if (current_proc) {
        printf("Cleaning scheduler's current process...\n");
        printf("\tDestroying process PID=%d\n", current_proc->pid);
        fflush(stdout);
        destroy_pcb(current_proc);
    }
    
    printf("Destroying mutexes...\n");
    fflush(stdout);
    pthread_mutex_destroy(&clk_mutex);
    pthread_cond_destroy(&clk_cond);
}

// Catch Ctrl+C to clean up and exit successfully
void handle_sigint(int sig) {
    printf("\nCaught signal %d, shutting down...\n", sig);
    running = 0;  // Signal main loop to exit
}

int main(int argc, char *argv[]) {
    int quantum = 5;              // Default quantum (ticks per process)
    int proc_gen_min = 3;         // Default min interval for process generation (ticks)
    int proc_gen_max = 10;        // Default max interval for process generation (ticks)
    int proc_ttl_min = 10;        // Default min TTL for processes (ticks)
    int proc_ttl_max = 50;        // Default max TTL for processes (ticks)
    int ready_queue_size = 100;   // Default ready queue capacity
    
    // Parse command line arguments
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        printf("Usage:\n   %s [flags]\n", argv[0]);
        printf("Flags:\n");
        printf("   -f <hz>           Clock frequency in Hz (default: 1)\n");
        printf("   -q <ticks>        Scheduler quantum (max ticks per process) (default: 5)\n");
        printf("   -pmin <ticks>     Min interval for process generation in ticks (default: 3)\n");
        printf("   -pmax <ticks>     Max interval for process generation in ticks (default: 10)\n");
        printf("   -tmin <ticks>     Min TTL for processes in ticks (default: 10)\n");
        printf("   -tmax <ticks>     Max TTL for processes in ticks (default: 50)\n");
        printf("   -qsize <num>      Ready queue size (default: 100)\n");
        return 0;
    }

    if (argc > 1){
        for (int i = 1; i < argc; i++) {
            if(i+1<argc){
                if (strcmp(argv[i], "-f")==0) {
                    i++;
                    CLOCK_FREQUENCY_HZ = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 1;
                } else if (strcmp(argv[i], "-q")==0) {
                    i++;
                    quantum = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 5;
                } else if (strcmp(argv[i], "-pmin")==0) {
                    i++;
                    proc_gen_min = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 3;
                } else if (strcmp(argv[i], "-pmax")==0) {
                    i++;
                    proc_gen_max = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 10;
                } else if (strcmp(argv[i], "-tmin")==0) {
                    i++;
                    proc_ttl_min = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 10;
                } else if (strcmp(argv[i], "-tmax")==0) {
                    i++;
                    proc_ttl_max = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 50;
                } else if (strcmp(argv[i], "-qsize")==0) {
                    i++;
                    ready_queue_size = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 100;
                }
            }
        }
    }
    
    // Set up signal handler for Ctrl+C
    signal(SIGINT, handle_sigint);
   
    // Create clock thread
    pthread_t clk_thread;
    if (start_clock(&clk_thread) != 0) {
        fprintf(stderr, "Failed to start system clock\n");
        return 1;
    }
    clk_thread_global = clk_thread;

    // No timers needed - scheduler manages itself
    timers_global = NULL;
    num_timers_global = 0;
    
    // Create ready queue for processes
    ready_queue_global = create_process_queue(ready_queue_size);
    if (!ready_queue_global) {
        fprintf(stderr, "Failed to create ready queue\n");
        stop_clock(clk_thread);
        return 1;
    }
    
    // Create active queue for processes
    active_queue_global = create_process_queue(ready_queue_size);
    if (!active_queue_global) {
        fprintf(stderr, "Failed to create active queue\n");
        destroy_process_queue(ready_queue_global);
        stop_clock(clk_thread);
        return 1;
    }
    
    // Create process generator
    proc_gen_global = create_process_generator(proc_gen_min, proc_gen_max, 
                                               proc_ttl_min, proc_ttl_max,
                                               ready_queue_global);
    if (!proc_gen_global) {
        fprintf(stderr, "Failed to create process generator\n");
        destroy_process_queue(ready_queue_global);
        stop_clock(clk_thread);
        return 1;
    }
    
    // Start process generator
    start_process_generator(proc_gen_global);
    
    // Create scheduler with quantum
    scheduler_global = create_scheduler(quantum, ready_queue_global, active_queue_global);
    if (!scheduler_global) {
        fprintf(stderr, "Failed to create scheduler\n");
        stop_process_generator(proc_gen_global);
        destroy_process_generator(proc_gen_global);
        destroy_process_queue(active_queue_global);
        destroy_process_queue(ready_queue_global);
        stop_clock(clk_thread);
        return 1;
    }
    
    // Start scheduler
    start_scheduler(scheduler_global);
    
    printf("\n=== System Configuration ===\n");
    printf("Clock frequency:      %d Hz\n", CLOCK_FREQUENCY_HZ);
    printf("Scheduler quantum:    %d ticks\n", quantum);
    printf("Process gen interval: %d-%d ticks\n", proc_gen_min, proc_gen_max);
    printf("Process TTL range:    %d-%d ticks\n", proc_ttl_min, proc_ttl_max);
    printf("Ready queue size:     %d (max processes)\n============================\n", ready_queue_size);
    printf("\nPress Ctrl+C to exit...\n\n");
    
    // Wait for signal using pause() which will be interrupted by SIGINT
    while (running) {
        pause();  // Blocks until a signal is received
    }
    
    // Cleanup
    printf("\n=== System cleanup and shutdown ===\n");
    cleanup_system(clk_thread_global, timers_global, num_timers_global);
    free(timers_global);
    printf("=== System shutdown complete ===\n");

    return 0;
}
