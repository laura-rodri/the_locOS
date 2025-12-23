#include "process.h"
#include "clock_sys.h"
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
    
    while (pg->running) {
        pthread_mutex_lock(&clk_mutex);
        
        // Wait until next generation time
        while (pg->running && clk_counter < next_generation_tick) {
            pthread_cond_wait(&clk_cond, &clk_mutex);
        }
        
        if (!pg->running) {
            pthread_mutex_unlock(&clk_mutex);
            break;
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
            
            printf("[Process Generator] Created process PID=%d TTL=%d (total=%d)\n", 
                   pending_pcb->pid, pending_pcb->ttl, pg->total_generated);
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
                                          ProcessQueue* ready_queue) {
    if (!ready_queue || min_interval < 1 || max_interval < min_interval || 
        min_ttl < 1 || max_ttl < min_ttl) {
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
    pg->running = 0;
    pg->next_pid = 1;  // PIDs start from 1
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

// Scheduler thread function - manages process execution with fixed quantum
void* scheduler_function(void* arg) {
    Scheduler* sched = (Scheduler*)arg;
    int last_tick = 0;
    
    while (sched->running) {
        pthread_mutex_lock(&clk_mutex);
        
        // Wait for next clock tick
        while (sched->running && clk_counter == last_tick) {
            pthread_cond_wait(&clk_cond, &clk_mutex);
        }
        
        if (!sched->running) {
            pthread_mutex_unlock(&clk_mutex);
            break;
        }
        
        last_tick = clk_counter;
        
        // If we have a running process, decrement its TTL
        if (sched->current_process) {
            int new_ttl = decrement_pcb_ttl(sched->current_process);
            sched->quantum_counter++;
            
            printf("[Scheduler] Process PID=%d executing (TTL=%d, quantum=%d/%d)\n", 
                   sched->current_process->pid, new_ttl, sched->quantum_counter, sched->quantum);
            fflush(stdout);
            
            // Check if process completed or quantum expired
            if (new_ttl <= 0) {
                // Process completed - destroy it
                printf("[Scheduler] Process PID=%d COMPLETED - destroying\n", sched->current_process->pid);
                fflush(stdout);
                __sync_fetch_and_add(&sched->total_completed, 1);
                destroy_pcb(sched->current_process);
                sched->current_process = NULL;
                sched->quantum_counter = 0;
            } else if (sched->quantum_counter >= sched->quantum) {
                // Quantum expired - return to ready queue
                printf("[Scheduler] Process PID=%d quantum expired - moving to READY\n", 
                       sched->current_process->pid);
                fflush(stdout);
                sched->current_process->state = WAITING;
                enqueue_process(sched->ready_queue, sched->current_process);
                sched->current_process = NULL;
                sched->quantum_counter = 0;
            }
        }
        
        // If no current process, try to get next one from ready queue
        if (sched->current_process == NULL && sched->ready_queue->current_size > 0) {
            sched->current_process = dequeue_process(sched->ready_queue);
            if (sched->current_process) {
                sched->current_process->state = RUNNING;
                sched->quantum_counter = 0;
                printf("[Scheduler] Process PID=%d selected for execution (TTL=%d)\n", 
                       sched->current_process->pid, sched->current_process->ttl);
                fflush(stdout);
            }
        }
        
        // If still no process, CPU is idle (wait for process generator)
        if (sched->current_process == NULL) {
            // Do nothing - just wait for next tick and check again
        }
        
        pthread_mutex_unlock(&clk_mutex);
    }
    
    printf("[Scheduler] Thread terminated\n");
    return NULL;
}

// Create a new scheduler
Scheduler* create_scheduler(int quantum, ProcessQueue* ready_queue, ProcessQueue* active_queue) {
    if (!ready_queue || !active_queue || quantum < 1) {
        fprintf(stderr, "Invalid scheduler parameters\n");
        return NULL;
    }
    
    Scheduler* sched = malloc(sizeof(Scheduler));
    if (!sched) return NULL;
    
    sched->quantum = quantum;
    sched->ready_queue = ready_queue;
    sched->active_queue = active_queue;
    sched->running = 0;
    sched->total_completed = 0;
    sched->current_process = NULL;
    sched->quantum_counter = 0;
    
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
        printf("[Scheduler] Started with quantum=%d ticks\n", sched->quantum);
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
        free(sched);
    }
}

// Get and clear the current process from scheduler (for cleanup)
PCB* scheduler_get_current_process(Scheduler* sched) {
    if (!sched) return NULL;
    
    PCB* proc = sched->current_process;
    sched->current_process = NULL;
    sched->quantum_counter = 0;
    return proc;
}

