#include "process.h"
#include "clock_sys.h"
#include "machine.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

// ============================================================================
// PCB Management
// ============================================================================

// Create a new PCB with given PID and default values
PCB* create_pcb(int pid) {
    PCB* pcb = malloc(sizeof(PCB));
    if (!pcb) return NULL;
    
    pcb->pid = pid;
    pcb->state = WAITING;  // New process starts in WAITING state
    pcb->priority = 0;     // Default priority
    pcb->ttl = 0;          // Default time to live
    pcb->initial_ttl = 0;  // Default initial TTL
    pcb->quantum_counter = 0; // Initialize quantum counter
    pcb->virtual_deadline = 0; // Initialize virtual deadline
    
    // Initialize memory management fields
    pcb->mm.code = NULL;
    pcb->mm.data = NULL;
    pcb->mm.pgb = NULL;
    
    // Initialize execution context
    pcb->context.pc = 0;
    pcb->context.instruction = 0;
    for (int i = 0; i < 16; i++) {
        pcb->context.registers[i] = 0;
    }
    
    return pcb;
}

void set_pcb_priority(PCB* pcb, int priority) {
    if (pcb) {
        pcb->priority = priority;
    }
}

void set_pcb_ttl(PCB* pcb, int ttl) {
    if (pcb) {
        pcb->ttl = ttl;
        pcb->initial_ttl = ttl;  // Save initial value
    }
}

int get_pcb_ttl(PCB* pcb) {
    if (pcb) {
        return pcb->ttl;
    }
    return -1;
}

int decrement_pcb_ttl(PCB* pcb) {
    if (pcb && pcb->ttl > 0) {
        pcb->ttl--;
    }
    return pcb ? pcb->ttl : -1;
}

void reset_pcb_ttl(PCB* pcb) {
    if (pcb) {
        pcb->ttl = pcb->initial_ttl;
    }
}

// Destroy a PCB and free memory
void destroy_pcb(PCB* pcb) {
    if (pcb) {
        free(pcb);
    }
}

// ============================================================================
// Process Queue Management
// ============================================================================

// Create a new process queue with given capacity
ProcessQueue* create_process_queue(int capacity) {
    ProcessQueue* pq = malloc(sizeof(ProcessQueue));
    if (!pq) return NULL;
    
    pq->queue = malloc(sizeof(PCB*) * capacity);
    if (!pq->queue) {
        free(pq);
        return NULL;
    }
    
    pq->front = 0;
    pq->rear = -1;
    pq->max_capacity = capacity;
    pq->current_size = 0;
    
    return pq;
}

// Destroy process queue and free memory
void destroy_process_queue(ProcessQueue* pq) {
    if (pq) {
        free(pq->queue);
        free(pq);
    }
}

// Add process to queue
int enqueue_process(ProcessQueue* pq, PCB* pcb) {
    if (pq->current_size >= pq->max_capacity) {
        return -1; // Queue full
    }
    
    pq->rear = (pq->rear + 1) % pq->max_capacity;
    pq->queue[pq->rear] = pcb;
    pq->current_size++;
    
    return 0;
}

// Remove and return process from queue
PCB* dequeue_process(ProcessQueue* pq) {
    if (pq->current_size == 0) {
        return NULL; // Queue empty
    }
    
    PCB* pcb = pq->queue[pq->front];
    pq->front = (pq->front + 1) % pq->max_capacity;
    pq->current_size--;
    
    return pcb;
}

// ============================================================================
// Process Generator
// ============================================================================

// Process generator thread function
void* process_generator_function(void* arg) {
    ProcessGenerator* pg = (ProcessGenerator*)arg;
    int next_generation_tick = 0;
    int interval;
    int waiting_for_space = 0;  // Flag to track if we're waiting for queue space
    PCB* pending_pcb = NULL;    // PCB waiting to be enqueued
    
    // Seed random number generator with current time + thread ID
    srand(time(NULL) ^ (unsigned long)pthread_self());
    
    while (pg->running && running) {
        pthread_mutex_lock(&clk_mutex);
        
        // Wait until next generation time
        while (pg->running && running && clk_counter < next_generation_tick) {
            pthread_cond_wait(&clk_cond, &clk_mutex);
        }
        
        if (!pg->running || !running) {
            pthread_mutex_unlock(&clk_mutex);
            break;
        }
        
        // Calculate current total processes (ready queue + executing + priority queues)
        int executing_count = pg->machine ? count_executing_processes(pg->machine) : 0;
        int priority_queues_count = pg->scheduler ? count_processes_in_priority_queues(pg->scheduler) : 0;
        int total_processes = pg->ready_queue->current_size + executing_count + priority_queues_count;
        
        // Check if we have space for new process
        if (total_processes >= pg->max_processes) {
            // No space available
            if (!waiting_for_space) {
                printf("[Process Generator] Maximum process limit reached (%d/%d)! Waiting for space...\n",
                       total_processes, pg->max_processes);
                fflush(stdout);
                waiting_for_space = 1;
            }
            pthread_mutex_unlock(&clk_mutex);
            continue;
        }
        
        // Generate new process only if we don't have a pending one
        if (!pending_pcb) {
            int new_pid = __sync_fetch_and_add(&pg->next_pid, 1);
            pending_pcb = create_pcb(new_pid);
            
            if (pending_pcb) {
                // Set random TTL for the process
                int ttl = pg->min_ttl;
                if (pg->max_ttl > pg->min_ttl) {
                    ttl += rand() % (pg->max_ttl - pg->min_ttl + 1);
                }
                set_pcb_ttl(pending_pcb, ttl);
                
                // Set random priority in range [-20, 19]
                pending_pcb->priority = MIN_PRIORITY + (rand() % NUM_PRIORITY_LEVELS);
            } else {
                printf("[Process Generator] Failed to allocate PCB for PID=%d\n", new_pid);
                __sync_fetch_and_sub(&pg->next_pid, 1);
                pthread_mutex_unlock(&clk_mutex);
                continue;
            }
        }
        
        // Try to add pending PCB to ready queue
        int enqueue_result = enqueue_process(pg->ready_queue, pending_pcb);
        if (enqueue_result == 0) {
            // Successfully enqueued
            __sync_fetch_and_add(&pg->total_generated, 1);
            
            // If we were waiting, indicate we resumed
            if (waiting_for_space) {
                printf("[Process Generator] Space available - resuming process generation\n");
                fflush(stdout);
                waiting_for_space = 0;
            }
            
            // Print priority only if policy uses it (BFS and Preemptive Priority)
            if (pg->scheduler && pg->scheduler->policy != SCHED_POLICY_ROUND_ROBIN) {
                printf("[Process Generator] Created process PID=%d TTL=%d Priority=%d (created_total=%d, in_system=%d/%d)\n", 
                       pending_pcb->pid, pending_pcb->ttl, pending_pcb->priority, pg->total_generated, 
                       total_processes + 1, pg->max_processes);
            } else {
                printf("[Process Generator] Created process PID=%d TTL=%d (created_total=%d, in_system=%d/%d)\n", 
                       pending_pcb->pid, pending_pcb->ttl, pg->total_generated, 
                       total_processes + 1, pg->max_processes);
            }
            fflush(stdout);
            
            // Clear pending PCB and calculate next generation time
            pending_pcb = NULL;
            interval = pg->min_interval;
            if (pg->max_interval > pg->min_interval) {
                interval += rand() % (pg->max_interval - pg->min_interval + 1);
            }
            next_generation_tick = clk_counter + interval;
        } else {
            // Queue is full - print message only once
            if (!waiting_for_space) {
                printf("[Process Generator] Ready queue full! Waiting for space...\n");
                fflush(stdout);
                waiting_for_space = 1;
            }
            // Keep pending_pcb for next attempt, don't update next_generation_tick
        }
        
        pthread_mutex_unlock(&clk_mutex);
    }
    
    // Cleanup pending PCB if any
    if (pending_pcb) {
        destroy_pcb(pending_pcb);
    }
    
    printf("[Process Generator] Thread terminated\n");
    return NULL;
}

// Create a new process generator
ProcessGenerator* create_process_generator(int min_interval, int max_interval, 
                                          int min_ttl, int max_ttl,
                                          ProcessQueue* ready_queue,
                                          Machine* machine, Scheduler* scheduler,
                                          int max_processes, int start_pid) {
    if (!ready_queue || min_interval < 1 || max_interval < min_interval || 
        min_ttl < 1 || max_ttl < min_ttl || max_processes < 1) {
        fprintf(stderr, "Invalid process generator parameters\n");
        return NULL;
    }
    
    ProcessGenerator* pg = malloc(sizeof(ProcessGenerator));
    if (!pg) return NULL;
    
    pg->min_interval = min_interval;
    pg->max_interval = max_interval;
    pg->min_ttl = min_ttl;
    pg->max_ttl = max_ttl;
    pg->ready_queue = ready_queue;
    pg->machine = machine;
    pg->scheduler = scheduler;
    pg->max_processes = max_processes;
    pg->running = 0;
    pg->next_pid = start_pid;  // Start from provided PID
    pg->total_generated = 0;
    
    return pg;
}

// Start the process generator thread
void start_process_generator(ProcessGenerator* pg) {
    if (!pg || pg->running) return;
    
    pg->running = 1;
    int ret = pthread_create(&pg->thread, NULL, process_generator_function, pg);
    if (ret != 0) {
        fprintf(stderr, "Error creating process generator thread: %s\n", strerror(ret));
        pg->running = 0;
    } else {
        printf("[Process Generator] Started (interval: %d-%d ticks, TTL: %d-%d)\n",
               pg->min_interval, pg->max_interval, 
               pg->min_ttl, pg->max_ttl);
    }
}

// Stop the process generator thread
void stop_process_generator(ProcessGenerator* pg) {
    if (!pg) return;
    
    pthread_mutex_lock(&clk_mutex);
    pg->running = 0;
    pthread_cond_broadcast(&clk_cond);  // Wake up the generator
    pthread_mutex_unlock(&clk_mutex);
    
    pthread_join(pg->thread, NULL);
}

// Destroy the process generator and free memory
void destroy_process_generator(ProcessGenerator* pg) {
    if (pg) {
        if (pg->running) {
            stop_process_generator(pg);
        }
        free(pg);
    }
}

// ============================================================================
// Scheduler with Quantum
// ============================================================================

// Helper function: Check if there are processes ready to be scheduled
static int has_ready_processes(Scheduler* sched) {
    if (sched->policy == SCHED_POLICY_PREEMPTIVE_PRIO) {
        if (!sched->priority_queues) return 0;
        for (int i = 0; i < NUM_PRIORITY_LEVELS; i++) {
            if (sched->priority_queues[i] && sched->priority_queues[i]->current_size > 0) {
                return 1;
            }
        }
        return 0;
    } else {
        return sched->ready_queue && sched->ready_queue->current_size > 0;
    }
}

// Helper function: Select next process based on policy
static PCB* select_next_process(Scheduler* sched) {
    switch (sched->policy) {
        case SCHED_POLICY_ROUND_ROBIN:
            // Simple FIFO - dequeue from front
            if (sched->ready_queue->current_size == 0) return NULL;
            return dequeue_process(sched->ready_queue);
            
        case SCHED_POLICY_BFS: {
            // Brain Fuck Scheduler - select process with lowest virtual deadline
            if (sched->ready_queue->current_size == 0) return NULL;
            
            int min_deadline = -1;
            int min_idx = -1;
            int idx = sched->ready_queue->front;
            
            for (int i = 0; i < sched->ready_queue->current_size; i++) {
                PCB* pcb = (PCB*)sched->ready_queue->queue[idx];
                if (min_deadline == -1 || pcb->virtual_deadline < min_deadline) {
                    min_deadline = pcb->virtual_deadline;
                    min_idx = idx;
                }
                idx = (idx + 1) % sched->ready_queue->max_capacity;
            }
            
            if (min_idx == -1) return NULL;
            
            // Remove selected process from queue
            PCB* selected = (PCB*)sched->ready_queue->queue[min_idx];
            
            // Shift elements to fill the gap
            int current_idx = min_idx;
            for (int i = 0; i < sched->ready_queue->current_size - 1; i++) {
                int next_idx = (current_idx + 1) % sched->ready_queue->max_capacity;
                sched->ready_queue->queue[current_idx] = sched->ready_queue->queue[next_idx];
                current_idx = next_idx;
            }
            
            sched->ready_queue->current_size--;
            sched->ready_queue->rear = (sched->ready_queue->rear - 1 + sched->ready_queue->max_capacity) 
                                       % sched->ready_queue->max_capacity;
            
            return selected;
        }
            
        case SCHED_POLICY_PREEMPTIVE_PRIO: {
            // Preemptive with static priorities - use multiple priority queues
            // Search from highest priority (-20) to lowest (+19)
            if (!sched->priority_queues) return NULL;
            
            for (int prio = MIN_PRIORITY; prio <= MAX_PRIORITY; prio++) {
                int queue_idx = prio - MIN_PRIORITY;  // Convert priority to array index (0-39)
                ProcessQueue* pq = sched->priority_queues[queue_idx];
                
                if (pq && pq->current_size > 0) {
                    // Found a non-empty queue at this priority level
                    PCB* selected = dequeue_process(pq);
                    if (selected && pq->current_size > 0) {
                        printf("[Scheduler] PRIORITY SELECTION: PID=%d (prio=%d) selected, %d more waiting at same priority\n",
                               selected->pid, selected->priority, pq->current_size);
                        fflush(stdout);
                    }
                    return selected;
                }
            }
            
            // No processes in any queue
            return NULL;
        }
            
        default:
            if (sched->ready_queue && sched->ready_queue->current_size > 0) {
                return dequeue_process(sched->ready_queue);
            }
            return NULL;
    }
}

// Helper function: Enqueue process to appropriate queue based on policy
static int enqueue_to_scheduler(Scheduler* sched, PCB* pcb) {
    if (sched->policy == SCHED_POLICY_PREEMPTIVE_PRIO) {
        // Use priority queues
        if (!sched->priority_queues) return -1;
        
        // Validate priority range
        if (pcb->priority < MIN_PRIORITY || pcb->priority > MAX_PRIORITY) {
            fprintf(stderr, "Invalid priority %d for process PID=%d\n", pcb->priority, pcb->pid);
            return -1;
        }
        
        int queue_idx = pcb->priority - MIN_PRIORITY;
        return enqueue_process(sched->priority_queues[queue_idx], pcb);
    } else {
        // Use single ready queue for RR and BFS
        return enqueue_process(sched->ready_queue, pcb);
    }
}

// Get the lowest priority process currently executing and its location
// Returns the priority value, or MAX_PRIORITY+1 if no processes executing
int get_lowest_priority_executing(Machine* machine, int* cpu_idx, int* core_idx, int* thread_idx) {
    if (!machine) return MAX_PRIORITY + 1;
    
    int lowest_priority = MAX_PRIORITY + 1;
    *cpu_idx = -1;
    *core_idx = -1;
    *thread_idx = -1;
    
    for (int i = 0; i < machine->num_CPUs; i++) {
        for (int j = 0; j < machine->cpus[i].num_cores; j++) {
            Core* core = &machine->cpus[i].cores[j];
            for (int k = 0; k < core->current_pcb_count; k++) {
                PCB* pcb = &core->pcbs[k];
                // Lower priority value = higher importance
                // We want highest priority number (lowest importance)
                if (pcb->priority > lowest_priority || lowest_priority == MAX_PRIORITY + 1) {
                    lowest_priority = pcb->priority;
                    *cpu_idx = i;
                    *core_idx = j;
                    *thread_idx = k;
                }
            }
        }
    }
    
    return lowest_priority;
}

// Count total processes in all priority queues
int count_processes_in_priority_queues(Scheduler* sched) {
    if (!sched || !sched->priority_queues) return 0;
    
    int total = 0;
    for (int i = 0; i < NUM_PRIORITY_LEVELS; i++) {
        if (sched->priority_queues[i]) {
            total += sched->priority_queues[i]->current_size;
        }
    }
    return total;
}

// Preempt processes of lower priority if a higher priority process arrives
// This implements event-driven preemptive scheduling
void preempt_lower_priority_processes(Scheduler* sched, PCB* new_pcb) {
    if (!sched || !sched->machine || !new_pcb) return;
    if (sched->policy != SCHED_POLICY_PREEMPTIVE_PRIO) return;
    
    // Find if there's a process executing with lower priority (higher number)
    int cpu_idx, core_idx, thread_idx;
    int lowest_prio = get_lowest_priority_executing(sched->machine, &cpu_idx, &core_idx, &thread_idx);
    
    // If new process has higher priority (lower number), preempt the lowest priority one
    if (lowest_prio != MAX_PRIORITY + 1 && new_pcb->priority < lowest_prio) {
        Core* core = &sched->machine->cpus[cpu_idx].cores[core_idx];
        PCB* preempted_pcb = &core->pcbs[thread_idx];
        
        printf("[Scheduler] PREEMPTION: Process PID=%d (prio=%d) preempting PID=%d (prio=%d) on CPU%d-Core%d-Thread%d\n",
               new_pcb->pid, new_pcb->priority, preempted_pcb->pid, preempted_pcb->priority, 
               cpu_idx, core_idx, thread_idx);
        fflush(stdout);
        
        // Save the preempted process state
        PCB* saved_pcb = create_pcb(preempted_pcb->pid);
        if (saved_pcb) {
            saved_pcb->state = WAITING;
            saved_pcb->priority = preempted_pcb->priority;
            saved_pcb->ttl = preempted_pcb->ttl;
            saved_pcb->initial_ttl = preempted_pcb->initial_ttl;
            saved_pcb->quantum_counter = 0;  // Reset quantum when preempted
            saved_pcb->virtual_deadline = preempted_pcb->virtual_deadline;
            
            // Return to appropriate priority queue
            enqueue_to_scheduler(sched, saved_pcb);
        }
        
        // Remove preempted process from core
        for (int l = thread_idx; l < core->current_pcb_count - 1; l++) {
            core->pcbs[l] = core->pcbs[l + 1];
        }
        core->current_pcb_count--;
    }
}

// Scheduler thread function - manages process execution with fixed quantum
// CRITICAL: The scheduler is now ONLY activated by Timer interrupts (for SCHED_SYNC_TIMER)
// or by clock ticks (for SCHED_SYNC_CLOCK). The clock itself decrements TTL.
void* scheduler_function(void* arg) {
    Scheduler* sched = (Scheduler*)arg;
    int last_tick = 0;
    
    while (sched->running && running) {
        if (sched->sync_mode == SCHED_SYNC_TIMER) {
            // Wait for scheduler activation signal from timer
            pthread_mutex_lock(&sched->sched_mutex);
            pthread_cond_wait(&sched->sched_cond, &sched->sched_mutex);
            pthread_mutex_unlock(&sched->sched_mutex);
            
            if (!sched->running || !running) {
                break;
            }
            
            printf("[Scheduler] Activated by Timer at tick %d\n", get_current_tick());
            fflush(stdout);
        } else {
            // SCHED_SYNC_CLOCK: Wait for clock ticks
            pthread_mutex_lock(&clk_mutex);
            
            // Wait for next clock tick
            while (sched->running && running && clk_counter == last_tick) {
                pthread_cond_wait(&clk_cond, &clk_mutex);
            }
            
            if (!sched->running || !running) {
                pthread_mutex_unlock(&clk_mutex);
                break;
            }
            
            last_tick = clk_counter;
            // DON'T unlock here - keep mutex for processing below
        }
        
        // Now process all executing processes to check for completion or quantum expiration
        // NOTE: TTL decrement is done by the clock, not here!
        // For TIMER mode, we need to lock. For CLOCK mode, we already have the lock
        if (sched->sync_mode == SCHED_SYNC_TIMER) {
            pthread_mutex_lock(&clk_mutex);
        }
        
        // Process all currently executing processes in all cores
        if (sched->machine && running) {
            for (int i = 0; i < sched->machine->num_CPUs && running; i++) {
                for (int j = 0; j < sched->machine->cpus[i].num_cores && running; j++) {
                    Core* core = &sched->machine->cpus[i].cores[j];
                    
                    // Process each hardware thread in this core (iterate backwards to safely remove)
                    for (int k = core->current_pcb_count - 1; k >= 0 && running; k--) {
                        HardwareThread* hw_thread = &core->hw_threads[k];
                        PCB* pcb = hw_thread->pcb;
                        
                        // Skip if no PCB assigned
                        if (!pcb) continue;
                        
                        // Increment quantum counter (TTL is decremented by clock!)
                        pcb->quantum_counter++;
                        
                        printf("[Scheduler] CPU%d-Core%d-Thread%d: Process PID=%d (TTL=%d, quantum=%d/%d)\n", 
                               i, j, k, pcb->pid, pcb->ttl, pcb->quantum_counter, sched->quantum);
                        fflush(stdout);
                        
                        // Check if process terminated (by EXIT instruction or TTL reached 0)
                        if (pcb->state == TERMINATED || pcb->ttl <= 0) {
                            // Process completed
                            const char* reason = (pcb->state == TERMINATED) ? "EXIT" : "TTL=0";
                            printf("[Scheduler] Process PID=%d COMPLETED (%s) - removing from CPU%d-Core%d-Thread%d\n", 
                                   pcb->pid, reason, i, j, k);
                            fflush(stdout);
                            __sync_fetch_and_add(&sched->total_completed, 1);
                            
                            // Free the PCB and its resources (page table, etc.)
                            if (pcb->mm.pgb) {
                                // Note: In a real system, we should free the page table and allocated frames
                                // For now, just mark as freed (memory leak, but acceptable for simulation)
                                pcb->mm.pgb = NULL;
                            }
                            
                            // Destroy the PCB
                            destroy_pcb(pcb);
                            
                            // Clear hardware thread
                            hw_thread->pcb = NULL;
                            hw_thread->PTBR = NULL;
                            hw_thread->PC = 0;
                            hw_thread->IR = 0;
                            hw_thread->mmu.page_table_base = NULL;
                            hw_thread->mmu.enabled = 0;
                            
                            // Shift remaining hardware threads and PCBs
                            for (int l = k; l < core->current_pcb_count - 1; l++) {
                                core->hw_threads[l] = core->hw_threads[l + 1];
                                core->pcbs[l] = core->pcbs[l + 1];
                            }
                            
                            // Clear the last one
                            core->hw_threads[core->current_pcb_count - 1].pcb = NULL;
                            core->hw_threads[core->current_pcb_count - 1].PTBR = NULL;
                            
                            core->current_pcb_count--;
                            
                        } else if (pcb->quantum_counter >= sched->quantum) {
                            // Quantum expired - return to ready queue
                            printf("[Scheduler] Process PID=%d quantum expired - moving from CPU%d-Core%d-Thread%d to READY\n", 
                                   pcb->pid, i, j, k);
                            fflush(stdout);
                            
                            // Save hardware thread context to PCB BEFORE clearing
                            HardwareThread* hw_thread = &core->hw_threads[k];
                            pcb->context.pc = hw_thread->PC;
                            pcb->context.instruction = hw_thread->IR;
                            for (int r = 0; r < 16; r++) {
                                pcb->context.registers[r] = hw_thread->registers[r];
                            }
                            
                            // Move the ORIGINAL PCB back to ready queue (don't create a copy)
                            // This preserves all memory management state
                            pcb->state = WAITING;
                            pcb->quantum_counter = 0;  // Reset quantum counter
                            
                            // Calculate virtual deadline for BFS
                            if (sched->policy == SCHED_POLICY_BFS) {
                                // deadline = T_actual + offset
                                // offset = rodaja_de_tiempo * prioridad / 100
                                int offset = (sched->quantum * pcb->priority) / 100;
                                int current_tick = clk_counter;  // Use clk_counter directly since we have mutex
                                pcb->virtual_deadline = current_tick + offset;
                                printf("[Scheduler] BFS: Process PID=%d virtual_deadline=%d (tick=%d, offset=%d, prio=%d)\n",
                                       pcb->pid, pcb->virtual_deadline, current_tick, offset, pcb->priority);
                                fflush(stdout);
                            }
                            
                            enqueue_to_scheduler(sched, pcb);
                            
                            // EVENT: Process returned to queue - this is an event
                            // For preemptive priority, check if higher priority processes are waiting
                            if (sched->policy == SCHED_POLICY_PREEMPTIVE_PRIO) {
                                // Signal that queue state changed - scheduler will handle reassignment
                                pthread_cond_broadcast(&clk_cond);
                            }
                            
                            // Remove from core by clearing the hardware thread
                            hw_thread->pcb = NULL;
                            hw_thread->PTBR = NULL;
                            hw_thread->PC = 0;
                            hw_thread->IR = 0;
                            hw_thread->mmu.page_table_base = NULL;
                            hw_thread->mmu.enabled = 0;
                            
                            // Shift remaining hardware threads
                            for (int l = k; l < core->current_pcb_count - 1; l++) {
                                core->hw_threads[l] = core->hw_threads[l + 1];
                                core->pcbs[l] = core->pcbs[l + 1];
                            }
                            
                            // Clear the last one
                            core->hw_threads[core->current_pcb_count - 1].pcb = NULL;
                            core->hw_threads[core->current_pcb_count - 1].PTBR = NULL;
                            
                            core->current_pcb_count--;
                        }
                    }
                }
            }
        }
        
        // Try to assign processes from ready queue to available cores
        // First, for PREEMPTIVE_PRIO, transfer processes from ready_queue to priority_queues
        // This is event-driven: when new processes arrive, check for preemption
        if (sched->policy == SCHED_POLICY_PREEMPTIVE_PRIO && sched->ready_queue->current_size > 0) {
            while (running && sched->ready_queue->current_size > 0) {
                PCB* pcb = dequeue_process(sched->ready_queue);
                if (pcb) {
                    // EVENT: New process arrived - check if it should preempt a running process
                    // This makes the scheduler event-driven and preemptive
                    if (!can_cpu_execute_process(sched->machine)) {
                        // No free slots - check if we should preempt
                        preempt_lower_priority_processes(sched, pcb);
                    }
                    
                    // Enqueue to appropriate priority queue
                    if (enqueue_to_scheduler(sched, pcb) != 0) {
                        // Queue full, put back in ready_queue
                        enqueue_process(sched->ready_queue, pcb);
                        break;
                    }
                } else {
                    break;
                }
            }
        }
        
        while (running && has_ready_processes(sched) && can_cpu_execute_process(sched->machine)) {
            PCB* pcb = select_next_process(sched);
            if (pcb) {
                pcb->state = RUNNING;
                pcb->quantum_counter = 0;  // Reset quantum counter for new execution
                
                // Calculate virtual deadline for BFS when assigning for first time
                if (sched->policy == SCHED_POLICY_BFS && pcb->virtual_deadline == 0) {
                    int offset = (sched->quantum * pcb->priority) / 100;
                    int current_tick = clk_counter;  // Use clk_counter directly since we have mutex
                    pcb->virtual_deadline = current_tick + offset;
                    printf("[Scheduler] BFS: Process PID=%d initial virtual_deadline=%d (tick=%d, offset=%d, prio=%d)\n",
                           pcb->pid, pcb->virtual_deadline, current_tick, offset, pcb->priority);
                    fflush(stdout);
                }
                
                if (assign_process_to_core(sched->machine, pcb)) {
                    // Print priority only if policy uses it (BFS and Preemptive Priority)
                    if (sched->policy != SCHED_POLICY_ROUND_ROBIN) {
                        printf("[Scheduler] Process PID=%d assigned to execution (TTL=%d, Priority=%d)\n", 
                               pcb->pid, pcb->ttl, pcb->priority);
                    } else {
                        printf("[Scheduler] Process PID=%d assigned to execution (TTL=%d)\n", 
                               pcb->pid, pcb->ttl);
                    }
                    fflush(stdout);
                    
                    // DO NOT free the PCB - it's still being used by the hardware thread
                    // The PCB will be freed when the process completes or is removed
                } else {
                    // Couldn't assign - put back in queue
                    enqueue_to_scheduler(sched, pcb);
                    break;
                }
            } else {
                break;
            }
        }
        
        pthread_mutex_unlock(&clk_mutex);
    }
    
    printf("[Scheduler] Thread terminated\n");
    return NULL;
}

// Create a new scheduler with default policy (Round Robin, clock sync)
Scheduler* create_scheduler(int quantum, ProcessQueue* ready_queue, Machine* machine) {
    return create_scheduler_with_policy(quantum, SCHED_POLICY_ROUND_ROBIN, 
                                       SCHED_SYNC_CLOCK, NULL, ready_queue, machine);
}

// Create a new scheduler with specific policy and synchronization
Scheduler* create_scheduler_with_policy(int quantum, int policy, int sync_mode, 
                                       void* sync_source, ProcessQueue* ready_queue, 
                                       Machine* machine) {
    if (!ready_queue || quantum < 1) {
        fprintf(stderr, "Invalid scheduler parameters\n");
        return NULL;
    }
    
    // Validate policy
    if (policy < SCHED_POLICY_ROUND_ROBIN || policy > SCHED_POLICY_PREEMPTIVE_PRIO) {
        fprintf(stderr, "Invalid scheduler policy: %d\n", policy);
        return NULL;
    }
    
    // Validate sync mode
    if (sync_mode < SCHED_SYNC_CLOCK || sync_mode > SCHED_SYNC_TIMER) {
        fprintf(stderr, "Invalid scheduler sync mode: %d\n", sync_mode);
        return NULL;
    }
    
    // Note: If using timer sync, sync_source will be set later after timer creation
    // (timer needs scheduler reference, so we create scheduler first)
    
    Scheduler* sched = malloc(sizeof(Scheduler));
    if (!sched) return NULL;
    
    sched->quantum = quantum;
    sched->policy = policy;
    sched->sync_mode = sync_mode;
    sched->sync_source = sync_source;
    sched->ready_queue = ready_queue;
    sched->machine = machine;
    sched->running = 0;
    sched->total_completed = 0;
    sched->priority_queues = NULL;
    
    // Initialize scheduler mutex and condition variable
    pthread_mutex_init(&sched->sched_mutex, NULL);
    pthread_cond_init(&sched->sched_cond, NULL);
    
    // Create priority queues if using PREEMPTIVE_PRIO policy
    if (policy == SCHED_POLICY_PREEMPTIVE_PRIO) {
        sched->priority_queues = malloc(sizeof(ProcessQueue*) * NUM_PRIORITY_LEVELS);
        if (!sched->priority_queues) {
            fprintf(stderr, "Failed to allocate priority queues array\n");
            free(sched);
            return NULL;
        }
        
        // Create a queue for each priority level
        // Capacity: floor(max_capacity / 40) ensures total ≤ max_capacity
        int queue_capacity = ready_queue->max_capacity / NUM_PRIORITY_LEVELS;
        
        // Minimum capacity must be at least 2 to be useful
        if (queue_capacity < 2) {
            queue_capacity = 2;
        }
        
        for (int i = 0; i < NUM_PRIORITY_LEVELS; i++) {
            sched->priority_queues[i] = create_process_queue(queue_capacity);
            if (!sched->priority_queues[i]) {
                fprintf(stderr, "Failed to create priority queue %d\n", i);
                // Clean up previously created queues
                for (int j = 0; j < i; j++) {
                    destroy_process_queue(sched->priority_queues[j]);
                }
                free(sched->priority_queues);
                free(sched);
                return NULL;
            }
        }
    }
    
    return sched;
}

// Start the scheduler thread
void start_scheduler(Scheduler* sched) {
    if (!sched || sched->running) return;
    
    sched->running = 1;
    int ret = pthread_create(&sched->thread, NULL, scheduler_function, sched);
    if (ret != 0) {
        fprintf(stderr, "Error creating scheduler thread: %s\n", strerror(ret));
        sched->running = 0;
    } else {
        const char* policy_names[] = {"Round Robin", "BFS", "Preemptive Priority"};
        const char* sync_names[] = {"Global Clock", "Timer"};
        
        printf("[Scheduler] Started with:\n");
        printf("  - Quantum: %d ticks\n", sched->quantum);
        printf("  - Policy: %s\n", policy_names[sched->policy]);
        printf("  - Sync: %s\n", sync_names[sched->sync_mode]);
    }
}

// Stop the scheduler thread
void stop_scheduler(Scheduler* sched) {
    if (!sched) return;
    
    pthread_mutex_lock(&clk_mutex);
    sched->running = 0;
    pthread_cond_broadcast(&clk_cond);  // Wake up the scheduler
    pthread_mutex_unlock(&clk_mutex);
    
    pthread_join(sched->thread, NULL);
}

// Destroy the scheduler and free memory
void destroy_scheduler(Scheduler* sched) {
    if (sched) {
        if (sched->running) {
            stop_scheduler(sched);
        }
        
        // Clean up priority queues if they exist
        if (sched->priority_queues) {
            for (int i = 0; i < NUM_PRIORITY_LEVELS; i++) {
                if (sched->priority_queues[i]) {
                    // Free any remaining processes in the queue
                    PCB* pcb;
                    while ((pcb = dequeue_process(sched->priority_queues[i])) != NULL) {
                        destroy_pcb(pcb);
                    }
                    destroy_process_queue(sched->priority_queues[i]);
                }
            }
            free(sched->priority_queues);
        }
        
        // Destroy mutex and condition variable
        pthread_mutex_destroy(&sched->sched_mutex);
        pthread_cond_destroy(&sched->sched_cond);
        
        free(sched);
    }
}
