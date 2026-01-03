#include "machine.h"
#include "memory.h"
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
        
        // Initialize general purpose registers
        for (int j = 0; j < 16; j++) {
            core->hw_threads[i].registers[j] = 0;
        }
        
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
                int hw_idx = core->current_pcb_count;
                
                // Store pointer to PCB in legacy array (DO NOT COPY - just keep the pointer)
                core->pcbs[hw_idx] = *pcb;  // Keep a copy for compatibility
                
                // Assign PCB pointer to hardware thread (use the actual PCB from parameter)
                HardwareThread* hw_thread = &core->hw_threads[hw_idx];
                hw_thread->pcb = pcb;  // Point to the ORIGINAL PCB, not a copy
                
                // Initialize hardware thread context from PCB
                hw_thread->PTBR = pcb->mm.pgb;  // Set page table base register from original PCB
                
                // Initialize PC only if this is a NEW process (state != RUNNING)
                // If the process is being reassigned after quantum expired, preserve PC
                if (pcb->context.pc == 0 && pcb->state != RUNNING) {
                    hw_thread->PC = 0;  // Start at virtual address 0 (code segment start)
                } else {
                    hw_thread->PC = pcb->context.pc;  // Restore saved PC
                }
                
                // Restore IR from PCB context
                hw_thread->IR = pcb->context.instruction;
                
                // Restore registers from PCB context
                for (int r = 0; r < 16; r++) {
                    hw_thread->registers[r] = pcb->context.registers[r];
                }
                
                // Enable MMU
                hw_thread->mmu.page_table_base = hw_thread->PTBR;
                hw_thread->mmu.enabled = 1;
                
                // Clear TLB
                for (int t = 0; t < TLB_SIZE; t++) {
                    hw_thread->tlb.entries[t].valid = 0;
                }
                hw_thread->tlb.next_replace = 0;
                
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
                HardwareThread* hw_thread = &core->hw_threads[k];
                if (hw_thread->pcb && hw_thread->pcb->pid == pid) {
                    // Found the process - clean up hardware thread
                    // Note: We DON'T free the PCB here - it may be requeued
                    // The scheduler or final cleanup will free it
                    hw_thread->pcb = NULL;
                    hw_thread->PTBR = NULL;
                    hw_thread->PC = 0;
                    hw_thread->IR = 0;
                    hw_thread->mmu.page_table_base = NULL;
                    hw_thread->mmu.enabled = 0;
                    
                    // Shift remaining processes
                    for (int l = k; l < core->current_pcb_count - 1; l++) {
                        core->hw_threads[l].pcb = core->hw_threads[l + 1].pcb;
                        core->hw_threads[l].PTBR = core->hw_threads[l + 1].PTBR;
                        core->hw_threads[l].PC = core->hw_threads[l + 1].PC;
                        core->hw_threads[l].IR = core->hw_threads[l + 1].IR;
                        // Keep copying other hardware thread state as needed
                        
                        // Also update legacy pcbs array
                        if (core->hw_threads[l].pcb) {
                            core->pcbs[l] = *core->hw_threads[l].pcb;
                        }
                    }
                    
                    // Clear the last hardware thread
                    core->hw_threads[core->current_pcb_count - 1].pcb = NULL;
                    core->hw_threads[core->current_pcb_count - 1].PTBR = NULL;
                    
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

// ============================================================================
// INSTRUCTION EXECUTION - FASE 2
// ============================================================================

// Instruction opcodes
#define OP_LD   0x0  // Load
#define OP_ST   0x1  // Store
#define OP_ADD  0x2  // Add
#define OP_EXIT 0xF  // Exit

// Helper: Extract opcode (upper 4 bits)
static inline uint8_t extract_opcode(uint32_t instruction) {
    return (instruction >> 28) & 0xF;
}

// Helper: Extract register field (bits 27-24)
static inline uint8_t extract_reg(uint32_t instruction) {
    return (instruction >> 24) & 0xF;
}

// Helper: Extract address field (lower 24 bits)
static inline uint32_t extract_address(uint32_t instruction) {
    return instruction & 0xFFFFFF;
}

// Helper: Extract source register 1 (bits 23-20)
static inline uint8_t extract_reg_src1(uint32_t instruction) {
    return (instruction >> 20) & 0xF;
}

// Helper: Extract source register 2 (bits 19-16)
static inline uint8_t extract_reg_src2(uint32_t instruction) {
    return (instruction >> 16) & 0xF;
}

// Instruction: LD (Load) - Opcode 0
// Format: 0RAAAAAA (R = register, A = address)
// Action: R = [Address]
static void execute_ld(HardwareThread* hw_thread, PhysicalMemory* pm, uint32_t instruction) {
    uint8_t reg = extract_reg(instruction);
    uint32_t address = extract_address(instruction);
    
    // Use MMU to read from virtual address
    PageTableEntry* page_table = (PageTableEntry*)hw_thread->PTBR;
    uint32_t value = mmu_read_word(pm, page_table, address);
    
    hw_thread->registers[reg] = value;
    
    printf("  [LD] r%u = [0x%06X] = 0x%08X\n", reg, address, value);
}

// Instruction: ST (Store) - Opcode 1
// Format: 1RAAAAAA (R = register, A = address)
// Action: [Address] = R
static void execute_st(HardwareThread* hw_thread, PhysicalMemory* pm, uint32_t instruction) {
    uint8_t reg = extract_reg(instruction);
    uint32_t address = extract_address(instruction);
    uint32_t value = hw_thread->registers[reg];
    
    // Use MMU to write to virtual address
    PageTableEntry* page_table = (PageTableEntry*)hw_thread->PTBR;
    mmu_write_word(pm, page_table, address, value);
    
    printf("  [ST] [0x%06X] = r%u = 0x%08X\n", address, reg, value);
}

// Instruction: ADD - Opcode 2
// Format: 2RXY---- (R = dest, X = src1, Y = src2)
// Action: R = X + Y (signed integers, complement a 2)
static void execute_add(HardwareThread* hw_thread, uint32_t instruction) {
    uint8_t reg_dest = extract_reg(instruction);
    uint8_t reg_src1 = extract_reg_src1(instruction);
    uint8_t reg_src2 = extract_reg_src2(instruction);
    
    // Perform signed addition
    int32_t val1 = (int32_t)hw_thread->registers[reg_src1];
    int32_t val2 = (int32_t)hw_thread->registers[reg_src2];
    int32_t result = val1 + val2;
    
    hw_thread->registers[reg_dest] = (uint32_t)result;
    
    printf("  [ADD] r%u = r%u + r%u = %d + %d = %d (0x%08X)\n",
           reg_dest, reg_src1, reg_src2, val1, val2, result, (uint32_t)result);
}

// Instruction: EXIT - Opcode F
// Format: F-------
// Action: Halt the hardware thread
static void execute_exit(HardwareThread* hw_thread) {
    printf("   [EXIT] Process PID=%d TERMINATED\n", hw_thread->pcb ? hw_thread->pcb->pid : -1);
    fflush(stdout);
    
    // Mark process as terminated
    if (hw_thread->pcb) {
        hw_thread->pcb->state = TERMINATED;
        hw_thread->pcb->ttl = 0;  // Force TTL to 0
    }
    
    // DO NOT increment PC - process has finished
    // The hardware thread will stop executing this process
    // The scheduler will detect TERMINATED state and remove the process
}

// Main instruction cycle: Fetch -> Decode -> Execute -> Update PC
void execute_instruction_cycle(HardwareThread* hw_thread, PhysicalMemory* pm) {
    if (!hw_thread || !pm) {
        fprintf(stderr, "Error: Invalid hardware thread or physical memory\n");
        return;
    }
    
    // Skip if no PCB assigned
    if (!hw_thread->pcb) {
        return;
    }
    
    // Skip if process is terminated - don't execute any more instructions
    if (hw_thread->pcb->state == TERMINATED) {
        return;
    }
    
    // Check if PTBR is valid
    if (!hw_thread->PTBR) {
        fprintf(stderr, "Error: PTBR not initialized for hardware thread\n");
        return;
    }
    
    PageTableEntry* page_table = (PageTableEntry*)hw_thread->PTBR;
    
    // === FETCH ===
    // Fetch instruction from memory using PC (virtual address)
    uint32_t instruction = mmu_read_word(pm, page_table, hw_thread->PC);
    hw_thread->IR = instruction;
    
    printf("PC=0x%06X: Instruction=0x%08X ", hw_thread->PC, instruction);
    
    // === DECODE ===
    uint8_t opcode = extract_opcode(instruction);
    
    // === EXECUTE ===
    switch (opcode) {
        case OP_LD:
            execute_ld(hw_thread, pm, instruction);
            hw_thread->PC += 4;  // Move to next instruction
            break;
            
        case OP_ST:
            execute_st(hw_thread, pm, instruction);
            hw_thread->PC += 4;
            break;
            
        case OP_ADD:
            execute_add(hw_thread, instruction);
            hw_thread->PC += 4;
            break;
            
        case OP_EXIT:
            execute_exit(hw_thread);
            // Don't increment PC - thread is done
            break;
            
        default:
            fprintf(stderr, "Error: Unknown opcode 0x%X in instruction 0x%08X\n", 
                    opcode, instruction);
            hw_thread->pcb->state = TERMINATED;
            break;
    }
}

