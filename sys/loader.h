#ifndef LOADER_H
#define LOADER_H

#include "process.h"
#include "memory.h"
#include "machine.h"

// Program file format
// A program file contains:
// 1. Header with metadata (code size, data size, etc.)
// 2. .text section (code segment)
// 3. .data section (data segment)

#define MAX_PROGRAM_NAME 256
#define MAX_CODE_SIZE 4096  // Maximum code segment size in words
#define MAX_DATA_SIZE 4096  // Maximum data segment size in words

// Program header structure
typedef struct {
    char program_name[MAX_PROGRAM_NAME];
    uint32_t code_size;      // Size of code segment in words
    uint32_t data_size;      // Size of data segment in words
    uint32_t text_address;   // Virtual address where .text section starts (in bytes)
    uint32_t data_address;   // Virtual address where .data section starts (in bytes)
    uint32_t entry_point;    // Entry point (virtual address relative to code segment)
    uint32_t priority;       // Process priority
    uint32_t ttl;            // Time to live
} ProgramHeader;

// Program structure (loaded from file)
typedef struct {
    ProgramHeader header;
    uint32_t* code_segment;  // Code segment data
    uint32_t* data_segment;  // Data segment data
} Program;

// Loader structure
typedef struct {
    PhysicalMemory* physical_memory;  // Reference to physical memory
    ProcessQueue* ready_queue;        // Queue where loaded processes are added
    Machine* machine;                 // Machine reference
    Scheduler* scheduler;             // Scheduler reference
    volatile int next_pid;            // Next process ID to assign
    volatile int total_loaded;        // Total programs loaded
} Loader;

// Loader functions
Loader* create_loader(PhysicalMemory* pm, ProcessQueue* ready_queue, 
                      Machine* machine, Scheduler* scheduler);
void destroy_loader(Loader* loader);

// Program loading (prometheus .elf format)
Program* load_program_from_elf(const char* filename);
void destroy_program(Program* program);

// Process creation from program
PCB* create_process_from_program(Loader* loader, Program* program);

// Helper function to calculate number of pages needed
uint32_t calculate_pages_needed(uint32_t size_in_bytes);

#endif // LOADER_H
