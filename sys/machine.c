#include "machine.h"
#include <stdlib.h>
#include <stdio.h>

// Create a new core with given capacity for kernel threads
Core* create_core(int num_kernel_threads) {
    Core* core = malloc(sizeof(Core));
    if (!core) return NULL;
    
    core->num_kernel_threads = num_kernel_threads;
    core->current_pcb_count = 0;
    
    // Legacy PCB array (kept for compatibility)
    core->pcbs = malloc(sizeof(PCB) * num_kernel_threads);
    if (!core->pcbs) {
        free(core);
        return NULL;
    }
    
    // Initialize hardware threads
    core->hw_threads = malloc(sizeof(HardwareThread) * num_kernel_threads);
    if (!core->hw_threads) {
        free(core->pcbs);
        free(core);
        return NULL;
    }
    
    // Initialize each hardware thread
    for (int i = 0; i < num_kernel_threads; i++) {
        core->hw_threads[i].PC = 0;
        core->hw_threads[i].IR = 0;
        core->hw_threads[i].PTBR = NULL;
        core->hw_threads[i].mmu.page_table_base = NULL;
        core->hw_threads[i].mmu.enabled = 0;
        core->hw_threads[i].pcb = NULL;
        
        // Initialize TLB
        for (int j = 0; j < TLB_SIZE; j++) {
            core->hw_threads[i].tlb.entries[j].virtual_page = 0;
            core->hw_threads[i].tlb.entries[j].physical_frame = 0;
            core->hw_threads[i].tlb.entries[j].valid = 0;
        }
        core->hw_threads[i].tlb.next_replace = 0;
    }
    
    return core;
}

// Create a new CPU with given number of cores
CPU* create_cpu(int num_cores, int num_kernel_threads) {
    CPU* cpu = malloc(sizeof(CPU));
    if (!cpu) return NULL;
    
    cpu->num_cores = num_cores;
    cpu->cores = malloc(sizeof(Core) * num_cores);
    if (!cpu->cores) {
        free(cpu);
        return NULL;
    }
    
    // Initialize each core
    for (int i = 0; i < num_cores; i++) {
        Core* new_core = create_core(num_kernel_threads);
        if (!new_core) {
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                destroy_core(&cpu->cores[j]);
            }
            free(cpu->cores);
            free(cpu);
            return NULL;
        }
        cpu->cores[i] = *new_core;
        free(new_core);  // Free the wrapper, we copied the contents
    }
    
    return cpu;
}

// Create a new machine with given number of CPUs
Machine* create_machine(int num_cpus, int num_cores, int num_kernel_threads) {
    Machine* machine = malloc(sizeof(Machine));
    if (!machine) return NULL;
    
    machine->num_CPUs = num_cpus;
    machine->cpus = malloc(sizeof(CPU) * num_cpus);
    if (!machine->cpus) {
        free(machine);
        return NULL;
    }
    
    // Initialize each CPU
    for (int i = 0; i < num_cpus; i++) {
        machine->cpus[i].num_cores = num_cores;
        machine->cpus[i].cores = malloc(sizeof(Core) * num_cores);
        if (!machine->cpus[i].cores) {
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                for (int k = 0; k < machine->cpus[j].num_cores; k++) {
                    free(machine->cpus[j].cores[k].pcbs);
                }
                free(machine->cpus[j].cores);
            }
            free(machine->cpus);
            free(machine);
            return NULL;
        }
        
        // Initialize each core in this CPU
        for (int j = 0; j < num_cores; j++) {
            Core* new_core = create_core(num_kernel_threads);
            if (!new_core) {
                // Cleanup on failure
                for (int k = 0; k < j; k++) {
                    destroy_core(&machine->cpus[i].cores[k]);
                }
                free(machine->cpus[i].cores);
                for (int k = 0; k < i; k++) {
                    for (int l = 0; l < machine->cpus[k].num_cores; l++) {
                        destroy_core(&machine->cpus[k].cores[l]);
                    }
                    free(machine->cpus[k].cores);
                }
                free(machine->cpus);
                free(machine);
                return NULL;
            }
            machine->cpus[i].cores[j] = *new_core;
            free(new_core);  // Free the wrapper, we copied the contents
        }
    }
    
    return machine;
}

// Destroy core and free memory
void destroy_core(Core* core) {
    if (core) {
        free(core->pcbs);
        free(core->hw_threads);
        // Note: don't free 'core' itself when it's part of an array
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
            // Free cores' PCB arrays
            for (int j = 0; j < machine->cpus[i].num_cores; j++) {
                free(machine->cpus[i].cores[j].pcbs);
            }
            free(machine->cpus[i].cores);
        }
        free(machine->cpus);
        free(machine);
    }
}

// Check if any CPU can execute a process (has at least one core with available space)
int can_cpu_execute_process(Machine* machine) {
    if (!machine) return 0;
    
    for (int i = 0; i < machine->num_CPUs; i++) {
        for (int j = 0; j < machine->cpus[i].num_cores; j++) {
            Core* core = &machine->cpus[i].cores[j];
            if (core->current_pcb_count < core->num_kernel_threads) {
                return 1;  // Found a core with available space
            }
        }
    }
    
    return 0;  // No core has available space
}

// Assign a process to the first available kernel thread in any core
// Returns 1 if successful, 0 if no space available
int assign_process_to_core(Machine* machine, PCB* pcb) {
    if (!machine || !pcb) return 0;
    
    for (int i = 0; i < machine->num_CPUs; i++) {
        for (int j = 0; j < machine->cpus[i].num_cores; j++) {
            Core* core = &machine->cpus[i].cores[j];
            if (core->current_pcb_count < core->num_kernel_threads) {
                // Found available space - assign the process
                core->pcbs[core->current_pcb_count] = *pcb;
                core->current_pcb_count++;
                return 1;
            }
        }
    }
    
    return 0;  // No space available
}

// Remove a completed or expired process from cores
// Returns 1 if found and removed, 0 otherwise
int remove_process_from_core(Machine* machine, int pid) {
    if (!machine) return 0;
    
    for (int i = 0; i < machine->num_CPUs; i++) {
        for (int j = 0; j < machine->cpus[i].num_cores; j++) {
            Core* core = &machine->cpus[i].cores[j];
            for (int k = 0; k < core->current_pcb_count; k++) {
                if (core->pcbs[k].pid == pid) {
                    // Found the process - remove it by shifting
                    for (int l = k; l < core->current_pcb_count - 1; l++) {
                        core->pcbs[l] = core->pcbs[l + 1];
                    }
                    core->current_pcb_count--;
                    return 1;
                }
            }
        }
    }
    
    return 0;  // Process not found
}

// Count total number of processes currently executing in machine
int count_executing_processes(Machine* machine) {
    if (!machine) return 0;
    
    int count = 0;
    for (int i = 0; i < machine->num_CPUs; i++) {
        for (int j = 0; j < machine->cpus[i].num_cores; j++) {
            count += machine->cpus[i].cores[j].current_pcb_count;
        }
    }
    
    return count;
}
