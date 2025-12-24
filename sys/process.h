#ifndef PROCESS_H
#define PROCESS_H

#include <pthread.h>

// Forward declaration for Machine
typedef struct Machine Machine;

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
    int quantum_counter; // Current quantum usage
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
    ProcessQueue* ready_queue; // Queue where new processes are added
    Machine* machine;        // Machine to check executing processes
    int max_processes;       // Maximum total processes (ready + executing)
    pthread_t thread;        // Generator thread
    volatile int running;    // Flag to control generator execution
    volatile int next_pid;   // Next process ID to assign
    volatile int total_generated; // Total processes generated
} ProcessGenerator;

// Scheduler configuration
typedef struct {
    int quantum;                     // Quantum (max ticks per process)
    ProcessQueue* ready_queue;       // Queue of ready processes
    Machine* machine;                // Machine with CPUs and cores
    pthread_t thread;                // Scheduler thread
    volatile int running;            // Flag to control scheduler execution
    volatile int total_completed;    // Total processes completed
} Scheduler;

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
                                           ProcessQueue* ready_queue,
                                           Machine* machine, int max_processes);
void start_process_generator(ProcessGenerator* pg);
void stop_process_generator(ProcessGenerator* pg);
void destroy_process_generator(ProcessGenerator* pg);
void* process_generator_function(void* arg);

// Scheduler with quantum
Scheduler* create_scheduler(int quantum, ProcessQueue* ready_queue, Machine* machine);
void start_scheduler(Scheduler* sched);
void stop_scheduler(Scheduler* sched);
void destroy_scheduler(Scheduler* sched);
void* scheduler_function(void* arg);

#endif // PROCESS_H
