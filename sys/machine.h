#ifndef MACHINE_H
#define MACHINE_H

#include "process.h"

// Machine -> CPU -> Core (PCBs of kernel threads)

// Core: contains kernel threads (PCBs)
typedef struct {
    int num_kernel_threads;
    PCB* pcbs;
} Core;

// CPU: contains multiple cores
typedef struct {
    int num_cores;
    Core* cores;
} CPU;

// Machine: contains multiple CPUs
typedef struct {
    int num_CPUs;
    CPU* cpus;
} Machine;

// Function declarations
Core* create_core(int num_kernel_threads);
CPU* create_cpu(int num_cores);
Machine* create_machine(int num_cpus);
void destroy_core(Core* core);
void destroy_cpu(CPU* cpu);
void destroy_machine(Machine* machine);

#endif // MACHINE_H
