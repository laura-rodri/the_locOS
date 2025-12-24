#ifndef MACHINE_H
#define MACHINE_H

#include "process.h"

// Machine -> CPU -> Core (PCBs of kernel threads)

// Forward declaration
struct Machine;
struct CPU;
struct Core;

// Core: contains kernel threads (PCBs)
typedef struct Core {
    int num_kernel_threads;     // Maximum number of kernel threads
    int current_pcb_count;       // Current number of PCBs in this core
    PCB* pcbs;
} Core;

// CPU: contains multiple cores
typedef struct CPU {
    int num_cores;
    Core* cores;
} CPU;

// Machine: contains multiple CPUs
typedef struct Machine {
    int num_CPUs;
    CPU* cpus;
} Machine;

// Function declarations
Core* create_core(int num_kernel_threads);
CPU* create_cpu(int num_cores, int num_kernel_threads);
Machine* create_machine(int num_cpus, int num_cores, int num_kernel_threads);
void destroy_core(Core* core);
void destroy_cpu(CPU* cpu);
void destroy_machine(Machine* machine);
int can_cpu_execute_process(Machine* machine);  // Returns 1 if any core has space, 0 otherwise
int assign_process_to_core(Machine* machine, PCB* pcb);  // Assign process to first available core
int remove_process_from_core(Machine* machine, int pid);  // Remove process from core by PID
int count_executing_processes(Machine* machine);  // Count total executing processes

#endif // MACHINE_H
