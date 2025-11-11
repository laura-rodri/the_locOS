#include "machine.h"
#include <stdlib.h>
#include <stdio.h>

// Create a new core with given capacity for kernel threads
Core* create_core(int num_kernel_threads) {
    Core* core = malloc(sizeof(Core));
    if (!core) return NULL;
    
    core->num_kernel_threads = num_kernel_threads;
    core->pcbs = malloc(sizeof(PCB) * num_kernel_threads);
    if (!core->pcbs) {
        free(core);
        return NULL;
    }
    
    return core;
}

// Create a new CPU with given number of cores
CPU* create_cpu(int num_cores) {
    CPU* cpu = malloc(sizeof(CPU));
    if (!cpu) return NULL;
    
    cpu->num_cores = num_cores;
    cpu->cores = malloc(sizeof(Core) * num_cores);
    if (!cpu->cores) {
        free(cpu);
        return NULL;
    }
    
    return cpu;
}

// Create a new machine with given number of CPUs
Machine* create_machine(int num_cpus) {
    Machine* machine = malloc(sizeof(Machine));
    if (!machine) return NULL;
    
    machine->num_CPUs = num_cpus;
    machine->cpus = malloc(sizeof(CPU) * num_cpus);
    if (!machine->cpus) {
        free(machine);
        return NULL;
    }
    
    return machine;
}

// Destroy core and free memory
void destroy_core(Core* core) {
    if (core) {
        free(core->pcbs);
        free(core);
    }
}

// Destroy CPU and free memory
void destroy_cpu(CPU* cpu) {
    if (cpu) {
        for (int i = 0; i < cpu->num_cores; i++) {
            free(cpu->cores[i].pcbs);
        }
        free(cpu->cores);
        free(cpu);
    }
}

// Destroy machine and free memory
void destroy_machine(Machine* machine) {
    if (machine) {
        for (int i = 0; i < machine->num_CPUs; i++) {
            destroy_cpu(&machine->cpus[i]);
        }
        free(machine->cpus);
        free(machine);
    }
}
