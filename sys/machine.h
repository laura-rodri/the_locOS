#ifndef MACHINE_H
#define MACHINE_H

#include "process.h"
#include <stdint.h>

// Machine -> CPU -> Core -> Hardware Thread (PCBs + registers)

// Forward declaration
struct Machine;
struct CPU;
struct Core;
struct HardwareThread;

// TLB Entry structure
#define TLB_SIZE 16  // Number of entries in TLB
typedef struct {
    uint32_t virtual_page;   // Virtual page number
    uint32_t physical_frame; // Physical frame number
    uint8_t valid;           // Valid bit (1 = entry is valid)
} TLBEntry;

// Translation Lookaside Buffer (TLB)
typedef struct {
    TLBEntry entries[TLB_SIZE];
    int next_replace;  // Index for simple round-robin replacement
} TLB;

// Memory Management Unit (MMU) - simulated
typedef struct {
    void* page_table_base;  // Pointer to page table base (PTBR content)
    int enabled;            // MMU enabled flag
} MMU;

// Hardware Thread - represents execution context
typedef struct {
    // Execution registers
    uint32_t PC;            // Program Counter
    uint32_t IR;            // Instruction Register
    void* PTBR;             // Page Table Base Register
    
    // Memory management hardware
    MMU mmu;                // Memory Management Unit
    TLB tlb;                // Translation Lookaside Buffer
    
    // Associated PCB (if any)
    PCB* pcb;               // Pointer to currently executing PCB (NULL if idle)
} HardwareThread;

// Core: contains hardware threads
typedef struct Core {
    int num_kernel_threads;     // Maximum number of hardware threads
    int current_pcb_count;       // Current number of PCBs in this core (DEPRECATED - use hw_threads)
    PCB* pcbs;                   // Legacy PCB array (DEPRECATED)
    HardwareThread* hw_threads;  // Array of hardware threads
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
