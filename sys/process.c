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
    
    return pcb;
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
        
        // Check if we've reached the maximum number of processes
        if (pg->max_processes > 0 && pg->total_generated >= pg->max_processes) {
            pthread_mutex_unlock(&clk_mutex);
            printf("[Process Generator] Maximum processes (%d) reached. Stopping.\n", 
                   pg->max_processes);
            pg->running = 0;
            break;
        }
        
        // Generate new process
        int new_pid = __sync_fetch_and_add(&pg->next_pid, 1);
        PCB* new_pcb = create_pcb(new_pid);
        
        if (new_pcb) {
            // Try to add to ready queue
            if (enqueue_process(pg->ready_queue, new_pcb) == 0) {
                __sync_fetch_and_add(&pg->total_generated, 1);
                printf("[Process Generator] Created process PID=%d at tick %d (total=%d)\n", 
                       new_pid, clk_counter, pg->total_generated);
                fflush(stdout);
            } else {
                printf("[Process Generator] Ready queue full! Cannot create PID=%d\n", new_pid);
                destroy_pcb(new_pcb);
            }
        } else {
            printf("[Process Generator] Failed to allocate PCB for PID=%d\n", new_pid);
        }
        
        // Calculate next generation time with random interval
        interval = pg->min_interval;
        if (pg->max_interval > pg->min_interval) {
            interval += rand() % (pg->max_interval - pg->min_interval + 1);
        }
        next_generation_tick = clk_counter + interval;
        
        printf("[Process Generator] Next process in %d ticks (at tick %d)\n", 
               interval, next_generation_tick);
        fflush(stdout);
        
        pthread_mutex_unlock(&clk_mutex);
    }
    
    printf("[Process Generator] Thread terminated\n");
    return NULL;
}

// Create a new process generator
ProcessGenerator* create_process_generator(int min_interval, int max_interval, 
                                          int max_processes, ProcessQueue* ready_queue) {
    if (!ready_queue || min_interval < 1 || max_interval < min_interval) {
        fprintf(stderr, "Invalid process generator parameters\n");
        return NULL;
    }
    
    ProcessGenerator* pg = malloc(sizeof(ProcessGenerator));
    if (!pg) return NULL;
    
    pg->min_interval = min_interval;
    pg->max_interval = max_interval;
    pg->max_processes = max_processes;
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
        printf("[Process Generator] Started (interval: %d-%d ticks, max: %s)\n",
               pg->min_interval, pg->max_interval, 
               pg->max_processes > 0 ? "limited" : "unlimited");
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
