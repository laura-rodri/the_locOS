#ifndef PROCESS_H
#define PROCESS_H

#include <pthread.h>

// Process states
#define RUNNING 0
#define WAITING 1
#define TERMINATED 2

// Process Control Block
typedef struct {
    int pid;
    int state;
    int priority;
    int ttl;            // Time to live (current)
    int initial_ttl;    // Initial TTL value (for reset)
    // etc - extend as needed
} PCB;

// Queue for managing processes
typedef struct {
    struct PCB** queue;
    int front;
    int rear;
    int max_capacity;
    int current_size;
} ProcessQueue;

// Process Generator configuration
typedef struct {
    int min_interval;        // Minimum ticks between process creation
    int max_interval;        // Maximum ticks between process creation
    int min_ttl;             // Minimum time to live for processes
    int max_ttl;             // Maximum time to live for processes
    int max_processes;       // Maximum number of processes to generate (0 = unlimited)
    ProcessQueue* ready_queue; // Queue where new processes are added
    pthread_t thread;        // Generator thread
    volatile int running;    // Flag to control generator execution
    volatile int next_pid;   // Next process ID to assign
    volatile int total_generated; // Total processes generated
} ProcessGenerator;

// Process Manager configuration
typedef struct {
    ProcessQueue* active_queue;  // Queue of active processes
    ProcessQueue* ready_queue;   // Queue to return finished processes
    pthread_t thread;            // Manager thread
    volatile int running;        // Flag to control manager execution
    volatile int total_terminated; // Total processes terminated
} ProcessManager;

// PCB management
PCB* create_pcb(int pid);
void destroy_pcb(PCB* pcb);
void set_pcb_priority(PCB* pcb, int priority);
void set_pcb_ttl(PCB* pcb, int ttl);
int get_pcb_ttl(PCB* pcb);
int decrement_pcb_ttl(PCB* pcb);  // Returns new TTL value
void reset_pcb_ttl(PCB* pcb);     // Reset TTL to initial value

// Queue management
ProcessQueue* create_process_queue(int capacity);
void destroy_process_queue(ProcessQueue* pq);
int enqueue_process(ProcessQueue* pq, PCB* pcb);
PCB* dequeue_process(ProcessQueue* pq);

// Process Generator
ProcessGenerator* create_process_generator(int min_interval, int max_interval, 
                                           int min_ttl, int max_ttl,
                                           int max_processes, ProcessQueue* ready_queue);
void start_process_generator(ProcessGenerator* pg);
void stop_process_generator(ProcessGenerator* pg);
void destroy_process_generator(ProcessGenerator* pg);
void* process_generator_function(void* arg);

// Process Manager
ProcessManager* create_process_manager(ProcessQueue* active_queue, ProcessQueue* ready_queue);
void start_process_manager(ProcessManager* pm);
void stop_process_manager(ProcessManager* pm);
void destroy_process_manager(ProcessManager* pm);
void* process_manager_function(void* arg);

#endif // PROCESS_H
