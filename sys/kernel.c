#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include "machine.h"
#include "process.h"
#include "clock_sys.h"
#include "timer.h"
#include "memory.h"
#include "loader.h"

// Global variables for cleanup
static pthread_t clk_thread_global;
static Timer** timers_global = NULL;
static int num_timers_global = 0;
static ProcessGenerator* proc_gen_global = NULL;
static Scheduler* scheduler_global = NULL;
static ProcessQueue* ready_queue_global = NULL;
static Machine* machine_global = NULL;
static PhysicalMemory* physical_memory_global = NULL;
static Loader* loader_global = NULL;
volatile int running = 1;  // Flag to control main loop and system components

// Timer callback function - activates the scheduler
void timer_scheduler_callback(int timer_id, void* user_data) {
    Scheduler* sched = (Scheduler*)user_data;
    if (sched) {
        // Signal the scheduler to wake up and do its work
        pthread_mutex_lock(&sched->sched_mutex);
        pthread_cond_signal(&sched->sched_cond);
        pthread_mutex_unlock(&sched->sched_mutex);
    }
}

// Clean up system resources
void cleanup_system(pthread_t clock_thread, Timer** timers, int num_timers) {
    int scheduler_policy = SCHED_POLICY_ROUND_ROBIN;  // Default policy
    
    printf("Stopping scheduler...\n");
    fflush(stdout);
    
    // Stop scheduler and print all running processes
    if (scheduler_global) {
        scheduler_policy = scheduler_global->policy;  // Save policy before destroying
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
        
        // For preemptive priority policy, print processes in priority queues BEFORE destroying scheduler
        if (scheduler_global->policy == SCHED_POLICY_PREEMPTIVE_PRIO) {
            printf("\tPriority queues content:\n");
            fflush(stdout);
            
            if (scheduler_global->priority_queues) {
                int total_in_priority_queues = 0;
                
                // Iterate through all priority levels
                for (int prio = MIN_PRIORITY; prio <= MAX_PRIORITY; prio++) {
                    int queue_idx = prio - MIN_PRIORITY;
                    ProcessQueue* pq = scheduler_global->priority_queues[queue_idx];
                    
                    if (pq && pq->current_size > 0) {
                        printf("\t  Priority %d: %d process(es)\n", prio, pq->current_size);
                        
                        int idx = pq->front;
                        for (int i = 0; i < pq->current_size; i++) {
                            PCB* pcb = (PCB*)pq->queue[idx];
                            printf("\t    PID=%d (TTL=%d)\n", pcb->pid, pcb->ttl);
                            idx = (idx + 1) % pq->max_capacity;
                        }
                        
                        total_in_priority_queues += pq->current_size;
                    }
                }
                
                if (total_in_priority_queues == 0) {
                    printf("\t  (empty - all priority queues are empty)\n");
                } else {
                    printf("\t  Total processes in priority queues: %d\n", total_in_priority_queues);
                }
            } else {
                printf("\t  (priority queues not initialized)\n");
            }
            fflush(stdout);
        }
        
        destroy_scheduler(scheduler_global);
    }
    
    // ProcessGenerator disabled - using only .elf programs
    /*
    printf("Stopping process generator...\n");
    fflush(stdout);
    
    // Stop process generator
    if (proc_gen_global) {
        stop_process_generator(proc_gen_global);
        destroy_process_generator(proc_gen_global);
    }
    */
    
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
        printf("\tNumber of processes in ready_queue: %d\n", ready_queue_global->current_size);
        if (ready_queue_global->current_size > 0) {
            printf("\tProcesses in ready_queue:\n");
            int count = ready_queue_global->current_size;
            int idx = ready_queue_global->front;
            for (int i = 0; i < count; i++) {
                PCB* pcb = (PCB*)ready_queue_global->queue[idx];
                // Print priority only if policy uses it (BFS and Preemptive Priority)
                if (scheduler_policy != SCHED_POLICY_ROUND_ROBIN) {
                    printf("\t  PID=%d (TTL=%d, Priority=%d)\n", pcb->pid, pcb->ttl, pcb->priority);
                } else {
                    printf("\t  PID=%d (TTL=%d)\n", pcb->pid, pcb->ttl);
                }
                idx = (idx + 1) % ready_queue_global->max_capacity;
            }
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
    
    // Destroy loader
    if (loader_global) {
        printf("Destroying loader...\n");
        fflush(stdout);
        destroy_loader(loader_global);
    }
    
    // Destroy physical memory
    if (physical_memory_global) {
        printf("Destroying physical memory...\n");
        fflush(stdout);
        
        // Show memory usage statistics before destroying
        printf("\n=== Memory Usage Statistics ===\n");
        int used_frames = 0;
        for (int i = 0; i < TOTAL_FRAMES; i++) {
            if (is_frame_allocated(physical_memory_global, i)) {
                used_frames++;
            }
        }
        printf("Total frames: %d (%.2f MB)\n", TOTAL_FRAMES, (TOTAL_FRAMES * PAGE_SIZE) / (1024.0 * 1024.0));
        printf("Used frames: %d (%.2f MB)\n", used_frames, (used_frames * PAGE_SIZE) / (1024.0 * 1024.0));
        printf("Free frames: %d (%.2f MB)\n", TOTAL_FRAMES - used_frames, ((TOTAL_FRAMES - used_frames) * PAGE_SIZE) / (1024.0 * 1024.0));
        printf("Memory utilization: %.2f%%\n", (used_frames * 100.0) / TOTAL_FRAMES);
        printf("==============================\n\n");
        fflush(stdout);
        
        destroy_physical_memory(physical_memory_global);
    }
    
    printf("Destroying mutexes...\n");
    fflush(stdout);
    pthread_mutex_destroy(&clk_mutex);
    pthread_cond_destroy(&clk_cond);
}

// Catch Ctrl+C to clean up and exit successfully
void handle_sigint(int sig) {
    printf("\n\n\033[31mCaught signal %d, shutting down...", sig);
    running = 0;  // Signal main loop to exit
    
    // Wake up all waiting threads immediately
    pthread_mutex_lock(&clk_mutex);
    pthread_cond_broadcast(&clk_cond);
    pthread_mutex_unlock(&clk_mutex);
    
    // Wake up scheduler if it's waiting on its own condition variable
    if (scheduler_global && scheduler_global->sync_mode == SCHED_SYNC_TIMER) {
        pthread_mutex_lock(&scheduler_global->sched_mutex);
        pthread_cond_broadcast(&scheduler_global->sched_cond);
        pthread_mutex_unlock(&scheduler_global->sched_mutex);
    }
}

int main(int argc, char *argv[]) {
    int quantum = 3;              // Default quantum (ticks per process)
    int num_timers = 1;           // Default number of timers
    int timer_interval = 5;       // Default timer interval (ticks)
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
        printf("   -f <hz>            Clock frequency in Hz (default: 1)\n");
        printf("   -q <ticks>         Scheduler quantum (max ticks per process) (default: 3)\n");
        printf("   -t <num>           Number of timers (default: 1)\n");
        printf("   -timeri <ticks>    Interval for timer interruptions in ticks (default: 5)\n");
        printf("   -policy <num>      Scheduler policy: 0=RR, 1=BFS, 2=PreemptivePrio (default: 0)\n");
        printf("   -sync <mode>       Sync mode: 0=Clock, 1=Timer (default: 0)\n");
        printf("   -pgenmin <ticks>   Min interval for process generation in ticks (default: 3)\n");
        printf("   -pgenmax <ticks>   Max interval for process generation in ticks (default: 10)\n");
        printf("   -ttlmin <ticks>    Min TTL for processes in ticks (default: 10)\n");
        printf("   -ttlmax <ticks>    Max TTL for processes in ticks (default: 50)\n");
        printf("   -qsize <num>       Ready queue size (default: 100)\n");
        printf("   -cpus <num>        Number of CPUs (default: 1)\n");
        printf("   -cores <num>       Number of cores per CPU (default: 2)\n");
        printf("   -threads <num>     Number of kernel threads per core (default: 4)\n");
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
                } else if (strcmp(argv[i], "-t")==0) {
                    i++;
                    num_timers = (atoi(argv[i]) >= 0) ? atoi(argv[i]) : 1;
                } else if (strcmp(argv[i], "-timert")==0) {
                    i++;
                    timer_interval = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 5;
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
                } else if (strcmp(argv[i], "-pgenmin")==0) {
                    i++;
                    proc_gen_min = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 3;
                } else if (strcmp(argv[i], "-pgenmax")==0) {
                    i++;
                    proc_gen_max = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 10;
                } else if (strcmp(argv[i], "-ttlmin")==0) {
                    i++;
                    proc_ttl_min = (atoi(argv[i]) > 0) ? atoi(argv[i]) : 10;
                } else if (strcmp(argv[i], "-ttlmax")==0) {
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
    
    // Set machine reference in clock so it can decrement TTL
    set_clock_machine(machine_global);
    
    // Create physical memory (FASE 2: Required for instruction execution)
    printf("Creating physical memory...\n");
    physical_memory_global = create_physical_memory();
    if (!physical_memory_global) {
        fprintf(stderr, "Failed to create physical memory\n");
        destroy_machine(machine_global);
        destroy_process_queue(ready_queue_global);
        stop_clock(clk_thread);
        return 1;
    }
    
    // Set physical memory in clock for instruction execution
    set_clock_physical_memory(physical_memory_global);
    
    // Calculate maximum usable kernel threads (limited by qsize)
    int total_threads = num_cpus * num_cores * num_threads;
    int max_usable_threads = (total_threads < ready_queue_size) ? total_threads : ready_queue_size;
    
    // Create scheduler with policy and sync mode (without timer yet for TIMER mode)
    scheduler_global = create_scheduler_with_policy(quantum, sched_policy, sched_sync, 
                                                     NULL, ready_queue_global, 
                                                     machine_global);
    if (!scheduler_global) {
        fprintf(stderr, "Failed to create scheduler\n");
        destroy_machine(machine_global);
        destroy_process_queue(ready_queue_global);
        stop_clock(clk_thread);
        return 1;
    }
    
    // Create timers
    Timer* scheduler_timer = NULL;
    
    // Determine how many timers to create:
    // - If sync mode is TIMER: use num_timers, at least 1
    // - If sync mode is CLOCK: create num_timers but they won't affect execution
//    int timers_to_create = (sched_sync == SCHED_SYNC_TIMER && num_timers < 1) ? 1 : num_timers;
    int timers_to_create = 0;
    if (sched_sync == SCHED_SYNC_TIMER) {
        timers_to_create = (num_timers < 1) ? 1 : num_timers+1;  // +1 for scheduler timer
    } else {
        timers_to_create = num_timers;
    }

    if (timers_to_create > 0) {
        timers_global = malloc(sizeof(Timer*) * timers_to_create);
        if (!timers_global) {
            fprintf(stderr, "Failed to allocate timer array\n");
            destroy_scheduler(scheduler_global);
            destroy_machine(machine_global);
            destroy_process_queue(ready_queue_global);
            stop_clock(clk_thread);
            return 1;
        }
        
        // Create all timers
        for (int i = 0; i < timers_to_create; i++) {
            Timer* timer = NULL;
            
            if (i == 0 && sched_sync == SCHED_SYNC_TIMER) {
                // First timer for scheduler synchronization (uses quantum as interval)
                timer = create_timer(i, quantum, timer_scheduler_callback, scheduler_global);
                scheduler_timer = timer;
            } else {
                // Additional timers without callback (use timer_interval for verification)
                timer = create_timer(i, timer_interval, NULL, NULL);
            }
            
            if (!timer) {
                fprintf(stderr, "Failed to create timer %d\n", i);
                // Clean up previously created timers
                for (int j = 0; j < i; j++) {
                    if (timers_global[j]) {
                        destroy_timer(timers_global[j]);
                    }
                }
                free(timers_global);
                destroy_scheduler(scheduler_global);
                destroy_machine(machine_global);
                destroy_process_queue(ready_queue_global);
                stop_clock(clk_thread);
                return 1;
            }
            
            timers_global[i] = timer;
        }
        
        num_timers_global = timers_to_create;
        
        // Update scheduler with timer reference if using timer sync
        if (sched_sync == SCHED_SYNC_TIMER && scheduler_timer) {
            scheduler_global->sync_source = scheduler_timer;
        }
    } else {
        // No timers needed
        timers_global = NULL;
        num_timers_global = 0;
    }
    
    // Create loader for .elf programs (FASE 2)
    printf("Creating loader...\n");
    loader_global = create_loader(physical_memory_global, ready_queue_global,
                                  machine_global, scheduler_global);
    if (!loader_global) {
        fprintf(stderr, "Failed to create loader\n");
        destroy_scheduler(scheduler_global);
        destroy_physical_memory(physical_memory_global);
        destroy_machine(machine_global);
        destroy_process_queue(ready_queue_global);
        if (timers_global) {
            for (int i = 0; i < num_timers_global; i++) {
                if (timers_global[i]) {
                    destroy_timer(timers_global[i]);
                }
            }
            free(timers_global);
        }
        stop_clock(clk_thread);
        return 1;
    }
    
    // Load .elf programs from ~/the_locOS/programs/ directory
    // Each .elf file becomes ONE complete process with executable code
    printf("Loading .elf programs from ~/the_locOS/programs/...\n");
    const char* programs_dir = "./../programs";
    DIR* dir = opendir(programs_dir);
    if (dir) {
        struct dirent* entry;
        int programs_loaded = 0;
        
        while ((entry = readdir(dir)) != NULL) {
            // Check if file ends with .elf
            if (strlen(entry->d_name) > 4 && strcmp(entry->d_name + strlen(entry->d_name) - 4, ".elf") == 0) {
                // Build full path
                char filepath[512];
                snprintf(filepath, sizeof(filepath), "%s/%s", programs_dir, entry->d_name);
                
                // Load program and create process
                printf("  Loading %s...\n", entry->d_name);
                Program* prog = load_program_from_elf(filepath);
                if (prog) {
                    PCB* pcb = create_process_from_program(loader_global, prog);
                    if (pcb) {
                        // Add to ready queue
                        if (enqueue_process(ready_queue_global, pcb) == 0) {
                            programs_loaded++;
                            printf("  %s  -> Process %d added to ready queue\n", entry->d_name, pcb->pid);
                        } else {
                            fprintf(stderr, "    -> Failed to enqueue process\n");
                            destroy_pcb(pcb);
                        }
                    }
                    destroy_program(prog);
                } else {
                    fprintf(stderr, "    -> Failed to load program\n");
                }
            }
        }
        closedir(dir);
        
        printf("[Loader] %d programs loaded from .elf files\n", programs_loaded);
    } else {
        fprintf(stderr, "Warning: Could not open programs directory '%s'\n", programs_dir);
        fprintf(stderr, "No .elf programs will be loaded\n");
    }
    
    // Process generator DISABLED - Using only .elf programs with real executable code
    // ProcessGenerator creates dummy processes without memory (PTBR not initialized)
    // Only .elf files create complete processes with code, data, and page tables
    /*
    int start_pid = loader_global ? loader_global->next_pid : 1;
    proc_gen_global = create_process_generator(proc_gen_min, proc_gen_max, 
                                               proc_ttl_min, proc_ttl_max,
                                               ready_queue_global, machine_global,
                                               scheduler_global, ready_queue_size,
                                               start_pid);
    if (!proc_gen_global) {
        fprintf(stderr, "Failed to create process generator\n");
        destroy_scheduler(scheduler_global);
        destroy_machine(machine_global);
        destroy_process_queue(ready_queue_global);
        if (timers_global) {
            for (int i = 0; i < num_timers_global; i++) {
                if (timers_global[i]) {
                    destroy_timer(timers_global[i]);
                }
            }
            free(timers_global);
        }
        stop_clock(clk_thread);
        return 1;
    }
    */
    proc_gen_global = NULL;  // No process generator - only .elf programs
    printf("Process creation: .elf programs only (ProcessGenerator disabled)\n");
    
    // Print system configuration BEFORE starting components
    const char* policy_names[] = {"Round Robin", "BFS", "Preemptive Priority"};
    const char* sync_names[] = {"Global Clock", "Timer"};
    
    printf("\n\033[34m=== System Configuration ===\n");
    printf("Clock frequency:      %d Hz\n", CLOCK_FREQUENCY_HZ);
    printf("Scheduler:\n");
    printf("  - Quantum:          %d ticks\n", quantum);
    printf("  - Policy:           %s\n", policy_names[sched_policy]);
    printf("  - Sync mode:        %s\n", sync_names[sched_sync]);
    if (num_timers_global > 0) {
        printf("Timers:               %d\n", num_timers_global);
        if (sched_sync == SCHED_SYNC_TIMER) {
            printf("  - Timer 0:          syncs scheduler (interval: %d ticks)\n", quantum);
            if (num_timers_global > 1) {
                printf("  - Timers 1-%d:       interval: %d ticks (no effect on execution)\n", 
                       num_timers_global - 1, timer_interval);
            }
        } else {
            printf("  - All timers:       interval: %d ticks (no effect on execution)\n", timer_interval);
        }
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
    printf("============================\033[0m");
    printf("\nPress Ctrl+C to exit...\n\n");
    
    // Print warnings and info about scheduler creation (if using priority queues)
    if (sched_policy == SCHED_POLICY_PREEMPTIVE_PRIO) {
        int queue_capacity = ready_queue_size / 40;  // NUM_PRIORITY_LEVELS = 40
        if (queue_capacity < 2) {
            fprintf(stderr, "Warning: Max processes (%d) too small for 40 priority levels. ",
                    ready_queue_size);
            fprintf(stderr, "Recommend at least 80. Using capacity 2 anyway.\n");
            queue_capacity = 2;
        }
        int total_capacity = queue_capacity * 40;
        printf("[Scheduler] Created 40 priority queues (capacity %d each, total %d/%d)\n", 
               queue_capacity, total_capacity, ready_queue_size);
    }
    
    // Start scheduler only - ProcessGenerator disabled (using .elf programs only)
    // start_process_generator(proc_gen_global);
    start_scheduler(scheduler_global);

    printf("\n\033[32m=== Running system ===\033[0m\n");
    fflush(stdout);
    
    // Wait for signal using pause() which will be interrupted by SIGINT
    while (running) {
        pause();
    }
    
    // Cleanup
    printf("\n=== System cleanup and shutdown ===\n");
    cleanup_system(clk_thread_global, timers_global, num_timers_global);
    free(timers_global);
    printf("=== System shutdown complete ===\033[0m\n");

    return 0;
}
