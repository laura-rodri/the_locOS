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
static Machine* machine_global = NULL;
static volatile int running = 1;  // Flag to control main loop

// Clean up system resources
void cleanup_system(pthread_t clock_thread, Timer** timers, int num_timers) {
    printf("Stopping scheduler...\n");
    fflush(stdout);
    
    // Stop scheduler and print all running processes
    if (scheduler_global) {
        stop_scheduler(scheduler_global);
        
        // Print all processes currently executing in the machine
        if (machine_global) {
            int total_executing = count_executing_processes(machine_global);
            printf("\tTotal processes executing: %d\n", total_executing);
            
            if (total_executing > 0) {
                printf("\tProcesses by CPU, Core, and Kernel Thread:\n");
                for (int i = 0; i < machine_global->num_CPUs; i++) {
                    for (int j = 0; j < machine_global->cpus[i].num_cores; j++) {
                        Core* core = &machine_global->cpus[i].cores[j];
                        if (core->current_pcb_count > 0) {
                            printf("\t  CPU%d - Core%d (%d/%d threads used):\n", 
                                   i, j, core->current_pcb_count, core->num_kernel_threads);
                            for (int k = 0; k < core->current_pcb_count; k++) {
                                PCB* pcb = &core->pcbs[k];
                                printf("\t    Thread%d: PID=%d (TTL=%d, State=%d, Quantum=%d)\n", 
                                       k, pcb->pid, pcb->ttl, pcb->state, pcb->quantum_counter);
                            }
                        }
                    }
                }
            }
        } else {
            printf("\tNo processes were running\n");
        }
        fflush(stdout);
        
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
    
    // Destroy machine (processes in cores are not dynamically allocated, just struct copies)
    if (machine_global) {
        printf("Destroying machine...\n");
        fflush(stdout);
        destroy_machine(machine_global);
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
    int num_cpus = 1;             // Default number of CPUs
    int num_cores = 2;            // Default number of cores per CPU
    int num_threads = 4;          // Default number of kernel threads per core
    int sched_policy = SCHED_POLICY_ROUND_ROBIN;  // Default scheduler policy
    int sched_sync = SCHED_SYNC_CLOCK;            // Default sync with global clock
    
    // Parse command line arguments
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        printf("Usage:\n   %s [flags]\n", argv[0]);
        printf("Flags:\n");
        printf("   -f <hz>           Clock frequency in Hz (default: 1)\n");
        printf("   -q <ticks>        Scheduler quantum (max ticks per process) (default: 5)\n");
        printf("   -policy <num>     Scheduler policy: 0=RR, 1=BFS, 2=PreemptivePrio (default: 0)\n");
        printf("   -sync <mode>      Sync mode: 0=Clock, 1=Timer (default: 0)\n");
        printf("   -pmin <ticks>     Min interval for process generation in ticks (default: 3)\n");
        printf("   -pmax <ticks>     Max interval for process generation in ticks (default: 10)\n");
        printf("   -tmin <ticks>     Min TTL for processes in ticks (default: 10)\n");
        printf("   -tmax <ticks>     Max TTL for processes in ticks (default: 50)\n");
        printf("   -qsize <num>      Ready queue size (default: 100)\n");
        printf("   -cpus <num>       Number of CPUs (default: 1)\n");
        printf("   -cores <num>      Number of cores per CPU (default: 2)\n");
        printf("   -threads <num>    Number of kernel threads per core (default: 4)\n");
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
                } else if (strcmp(argv[i], "-policy")==0) {
                    i++;
                    int policy = atoi(argv[i]);
                    if (policy >= 0 && policy <= 2) {
                        sched_policy = policy;
                    }
                } else if (strcmp(argv[i], "-sync")==0) {
                    i++;
                    int sync = atoi(argv[i]);
                    if (sync >= 0 && sync <= 1) {
                        sched_sync = sync;
                    }
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
                } else if (strcmp(argv[i], "-cpus")==0) {
                    i++;
                    num_cpus = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 1;
                } else if (strcmp(argv[i], "-cores")==0) {
                    i++;
                    num_cores = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 2;
                } else if (strcmp(argv[i], "-threads")==0) {
                    i++;
                    num_threads = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 4;
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

    // Create timer if using timer synchronization
    Timer* scheduler_timer = NULL;
    if (sched_sync == SCHED_SYNC_TIMER) {
        // Create one timer with quantum interval
        timers_global = malloc(sizeof(Timer*) * 1);
        if (!timers_global) {
            fprintf(stderr, "Failed to allocate timer array\n");
            stop_clock(clk_thread);
            return 1;
        }
        
        scheduler_timer = create_timer(0, quantum, NULL, NULL);
        if (!scheduler_timer) {
            fprintf(stderr, "Failed to create scheduler timer\n");
            free(timers_global);
            stop_clock(clk_thread);
            return 1;
        }
        
        timers_global[0] = scheduler_timer;
        num_timers_global = 1;
    } else {
        // No timers needed - scheduler syncs with global clock
        timers_global = NULL;
        num_timers_global = 0;
    }
    
    // Create ready queue for processes
    ready_queue_global = create_process_queue(ready_queue_size);
    if (!ready_queue_global) {
        fprintf(stderr, "Failed to create ready queue\n");
        stop_clock(clk_thread);
        return 1;
    }
    
    // Create machine with CPUs, cores, and kernel threads
    machine_global = create_machine(num_cpus, num_cores, num_threads);
    if (!machine_global) {
        fprintf(stderr, "Failed to create machine\n");
        destroy_process_queue(ready_queue_global);
        stop_clock(clk_thread);
        return 1;
    }
    
    // Calculate maximum usable kernel threads (limited by qsize)
    int total_threads = num_cpus * num_cores * num_threads;
    int max_usable_threads = (total_threads < ready_queue_size) ? total_threads : ready_queue_size;
    
    // Create process generator with machine reference and max_processes limit
    proc_gen_global = create_process_generator(proc_gen_min, proc_gen_max, 
                                               proc_ttl_min, proc_ttl_max,
                                               ready_queue_global, machine_global, 
                                               ready_queue_size);
    if (!proc_gen_global) {
        fprintf(stderr, "Failed to create process generator\n");
        destroy_machine(machine_global);
        destroy_process_queue(ready_queue_global);
        stop_clock(clk_thread);
        return 1;
    }
    
    // Start process generator
    start_process_generator(proc_gen_global);
    
    // Create scheduler with policy, sync mode, and optional timer
    scheduler_global = create_scheduler_with_policy(quantum, sched_policy, sched_sync, 
                                                     scheduler_timer, ready_queue_global, 
                                                     machine_global);
    if (!scheduler_global) {
        fprintf(stderr, "Failed to create scheduler\n");
        if (scheduler_timer) {
            destroy_timer(scheduler_timer);
            free(timers_global);
        }
        stop_process_generator(proc_gen_global);
        destroy_process_generator(proc_gen_global);
        destroy_machine(machine_global);
        destroy_process_queue(ready_queue_global);
        stop_clock(clk_thread);
        return 1;
    }
    
    // Start scheduler
    start_scheduler(scheduler_global);
    
    const char* policy_names[] = {"Round Robin", "BFS", "Preemptive Priority"};
    const char* sync_names[] = {"Global Clock", "Timer"};
    
    printf("\n=== System Configuration ===\n");
    printf("Clock frequency:      %d Hz\n", CLOCK_FREQUENCY_HZ);
    printf("Scheduler:\n");
    printf("  - Quantum:          %d ticks\n", quantum);
    printf("  - Policy:           %s\n", policy_names[sched_policy]);
    printf("  - Sync mode:        %s\n", sync_names[sched_sync]);
    if (sched_sync == SCHED_SYNC_TIMER) {
        printf("  - Timer interval:   %d ticks\n", quantum);
    }
    printf("Process gen interval: %d-%d ticks\n", proc_gen_min, proc_gen_max);
    printf("Process TTL range:    %d-%d ticks\n", proc_ttl_min, proc_ttl_max);
    printf("Max processes:        %d (queue size limit)\n", ready_queue_size);
    printf("Machine topology:\n");
    printf("  - CPUs:             %d\n", num_cpus);
    printf("  - Cores per CPU:    %d\n", num_cores);
    printf("  - Threads per core: %d\n", num_threads);
    printf("  - Total threads:    %d\n", total_threads);
    printf("  - Usable threads:   %d (limited by max_processes)\n", max_usable_threads);
    printf("============================\n");
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
