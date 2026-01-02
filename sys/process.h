#ifndef PROCESS_H
#define PROCESS_H

#include <pthread.h>

// Global flag to control system execution
extern volatile int running;

// Forward declarations
typedef struct Machine Machine;
typedef struct Scheduler Scheduler;

// Process states
#define RUNNING 0
#define WAITING 1
#define TERMINATED 2

// Priority range
#define MIN_PRIORITY -20  // Highest priority
#define MAX_PRIORITY 19   // Lowest priority
#define NUM_PRIORITY_LEVELS 40  // From -20 to 19

// Memory Management structure for each process
typedef struct {
    void* code;             // Virtual address of code segment start
    void* data;             // Virtual address of data segment start
    void* pgb;              // Physical address of page table base
} MemoryManagement;

// Process Control Block
typedef struct {
    int pid;
    int state;
    int priority;           // Priority: -20 (highest) to 19 (lowest)
    int ttl;                // Time to live (current)
    int initial_ttl;        // Initial TTL value (for reset)
    int quantum_counter;    // Current quantum usage
    int virtual_deadline;   // Virtual deadline for BFS scheduling
    MemoryManagement mm;    // Memory management information
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
    Scheduler* scheduler;    // Scheduler to check priority queues
    int max_processes;       // Maximum total processes (ready + executing + in priority queues)
    pthread_t thread;        // Generator thread
    volatile int running;    // Flag to control generator execution
    volatile int next_pid;   // Next process ID to assign
    volatile int total_generated; // Total processes generated
} ProcessGenerator;

// Scheduler policies
#define SCHED_POLICY_ROUND_ROBIN 0      // Round robin sin prioridades (default)
#define SCHED_POLICY_BFS 1              // Brain Fuck Scheduler
#define SCHED_POLICY_PREEMPTIVE_PRIO 2  // Expulsora por evento con prioridades estáticas

// Scheduler synchronization modes
#define SCHED_SYNC_CLOCK 0    // Sincronizado con el reloj global
#define SCHED_SYNC_TIMER 1    // Sincronizado con un timer

// Scheduler configuration
typedef struct Scheduler {
    int quantum;                     // Quantum (max ticks per process)
    int policy;                      // Política de planificación
    int sync_mode;                   // Modo de sincronización (CLOCK o TIMER)
    void* sync_source;               // Timer* si sync_mode==TIMER, NULL si CLOCK
    ProcessQueue* ready_queue;       // Queue of ready processes (for RR and BFS)
    ProcessQueue** priority_queues;  // Array of queues for priority scheduling (one per priority level)
    Machine* machine;                // Machine with CPUs and cores
    pthread_t thread;                // Scheduler thread
    volatile int running;            // Flag to control scheduler execution
    volatile int total_completed;    // Total processes completed
    pthread_mutex_t sched_mutex;     // Mutex for scheduler activation
    pthread_cond_t sched_cond;       // Condition variable for scheduler activation
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
                                           Machine* machine, Scheduler* scheduler,
                                           int max_processes);
void start_process_generator(ProcessGenerator* pg);
void stop_process_generator(ProcessGenerator* pg);
void destroy_process_generator(ProcessGenerator* pg);
void* process_generator_function(void* arg);

// Scheduler with quantum
Scheduler* create_scheduler(int quantum, ProcessQueue* ready_queue, Machine* machine);
Scheduler* create_scheduler_with_policy(int quantum, int policy, int sync_mode, 
                                        void* sync_source, ProcessQueue* ready_queue, 
                                        Machine* machine);
void start_scheduler(Scheduler* sched);
void stop_scheduler(Scheduler* sched);
void destroy_scheduler(Scheduler* sched);
void* scheduler_function(void* arg);

// Preemptive priority helper functions
int get_lowest_priority_executing(Machine* machine, int* cpu_idx, int* core_idx, int* thread_idx);
void preempt_lower_priority_processes(Scheduler* sched, PCB* new_pcb);
int count_processes_in_priority_queues(Scheduler* sched);

#endif // PROCESS_H
