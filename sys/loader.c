#include "loader.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Create a new loader
Loader* create_loader(PhysicalMemory* pm, ProcessQueue* ready_queue, 
                      Machine* machine, Scheduler* scheduler) {
    Loader* loader = malloc(sizeof(Loader));
    if (!loader) {
        fprintf(stderr, "Error: Failed to allocate Loader\n");
        return NULL;
    }
    
    loader->physical_memory = pm;
    loader->ready_queue = ready_queue;
    loader->machine = machine;
    loader->scheduler = scheduler;
    loader->next_pid = 1;
    loader->total_loaded = 0;
    
    printf("Loader initialized\n");
    return loader;
}

// Destroy loader
void destroy_loader(Loader* loader) {
    if (loader) {
        free(loader);
    }
}

// Load a program from a text file
// File format:
// Line 1: PROGRAM <name>
// Line 2: CODE_SIZE <size_in_words>
// Line 3: DATA_SIZE <size_in_words>
// Line 4: ENTRY_POINT <address>
// Line 5: PRIORITY <priority>
// Line 6: TTL <time_to_live>
// Line 7: CODE_SECTION
// Lines 8+: <hex_word> (one per line, code_size lines)
// Line N: DATA_SECTION
// Lines N+1+: <hex_word> (one per line, data_size lines)
Program* load_program_from_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open program file '%s'\n", filename);
        return NULL;
    }
    
    Program* program = malloc(sizeof(Program));
    if (!program) {
        fclose(file);
        return NULL;
    }
    
    char line[512];
    char keyword[64];
    
    // Read header
    // Line 1: PROGRAM <name>
    if (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%s %s", keyword, program->header.program_name) != 2 ||
            strcmp(keyword, "PROGRAM") != 0) {
            fprintf(stderr, "Error: Invalid program header\n");
            free(program);
            fclose(file);
            return NULL;
        }
    }
    
    // Line 2: CODE_SIZE <size>
    if (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%s %u", keyword, &program->header.code_size) != 2 ||
            strcmp(keyword, "CODE_SIZE") != 0) {
            fprintf(stderr, "Error: Invalid CODE_SIZE\n");
            free(program);
            fclose(file);
            return NULL;
        }
    }
    
    // Line 3: DATA_SIZE <size>
    if (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%s %u", keyword, &program->header.data_size) != 2 ||
            strcmp(keyword, "DATA_SIZE") != 0) {
            fprintf(stderr, "Error: Invalid DATA_SIZE\n");
            free(program);
            fclose(file);
            return NULL;
        }
    }
    
    // Line 4: ENTRY_POINT <address>
    if (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%s %u", keyword, &program->header.entry_point) != 2 ||
            strcmp(keyword, "ENTRY_POINT") != 0) {
            fprintf(stderr, "Error: Invalid ENTRY_POINT\n");
            free(program);
            fclose(file);
            return NULL;
        }
    }
    
    // Line 5: PRIORITY <priority>
    if (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%s %u", keyword, &program->header.priority) != 2 ||
            strcmp(keyword, "PRIORITY") != 0) {
            fprintf(stderr, "Error: Invalid PRIORITY\n");
            free(program);
            fclose(file);
            return NULL;
        }
    }
    
    // Line 6: TTL <ttl>
    if (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "%s %u", keyword, &program->header.ttl) != 2 ||
            strcmp(keyword, "TTL") != 0) {
            fprintf(stderr, "Error: Invalid TTL\n");
            free(program);
            fclose(file);
            return NULL;
        }
    }
    
    // Allocate code segment
    if (program->header.code_size > 0) {
        program->code_segment = malloc(sizeof(uint32_t) * program->header.code_size);
        if (!program->code_segment) {
            fprintf(stderr, "Error: Failed to allocate code segment\n");
            free(program);
            fclose(file);
            return NULL;
        }
        
        // Line 7: CODE_SECTION
        if (fgets(line, sizeof(line), file)) {
            if (sscanf(line, "%s", keyword) != 1 || strcmp(keyword, "CODE_SECTION") != 0) {
                fprintf(stderr, "Error: Expected CODE_SECTION\n");
                free(program->code_segment);
                free(program);
                fclose(file);
                return NULL;
            }
        }
        
        // Read code segment data
        for (uint32_t i = 0; i < program->header.code_size; i++) {
            if (fgets(line, sizeof(line), file)) {
                if (sscanf(line, "%x", &program->code_segment[i]) != 1) {
                    fprintf(stderr, "Error: Invalid code data at line %u\n", i);
                    free(program->code_segment);
                    free(program);
                    fclose(file);
                    return NULL;
                }
            } else {
                fprintf(stderr, "Error: Unexpected end of file in code section\n");
                free(program->code_segment);
                free(program);
                fclose(file);
                return NULL;
            }
        }
    } else {
        program->code_segment = NULL;
    }
    
    // Allocate data segment
    if (program->header.data_size > 0) {
        program->data_segment = malloc(sizeof(uint32_t) * program->header.data_size);
        if (!program->data_segment) {
            fprintf(stderr, "Error: Failed to allocate data segment\n");
            if (program->code_segment) free(program->code_segment);
            free(program);
            fclose(file);
            return NULL;
        }
        
        // Line N: DATA_SECTION
        if (fgets(line, sizeof(line), file)) {
            if (sscanf(line, "%s", keyword) != 1 || strcmp(keyword, "DATA_SECTION") != 0) {
                fprintf(stderr, "Error: Expected DATA_SECTION\n");
                if (program->code_segment) free(program->code_segment);
                free(program->data_segment);
                free(program);
                fclose(file);
                return NULL;
            }
        }
        
        // Read data segment data
        for (uint32_t i = 0; i < program->header.data_size; i++) {
            if (fgets(line, sizeof(line), file)) {
                if (sscanf(line, "%x", &program->data_segment[i]) != 1) {
                    fprintf(stderr, "Error: Invalid data at line %u\n", i);
                    if (program->code_segment) free(program->code_segment);
                    free(program->data_segment);
                    free(program);
                    fclose(file);
                    return NULL;
                }
            } else {
                fprintf(stderr, "Error: Unexpected end of file in data section\n");
                if (program->code_segment) free(program->code_segment);
                free(program->data_segment);
                free(program);
                fclose(file);
                return NULL;
            }
        }
    } else {
        program->data_segment = NULL;
    }
    
    fclose(file);
    printf("Program '%s' loaded: CODE=%u words, DATA=%u words\n",
           program->header.program_name, 
           program->header.code_size, 
           program->header.data_size);
    
    return program;
}

// Destroy program and free memory
void destroy_program(Program* program) {
    if (program) {
        if (program->code_segment) free(program->code_segment);
        if (program->data_segment) free(program->data_segment);
        free(program);
    }
}

// Calculate number of pages needed for a given size in bytes
uint32_t calculate_pages_needed(uint32_t size_in_bytes) {
    if (size_in_bytes == 0) return 0;
    return (size_in_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
}

// Create a process from a loaded program
PCB* create_process_from_program(Loader* loader, Program* program) {
    if (!loader || !program || !loader->physical_memory) {
        fprintf(stderr, "Error: Invalid loader or program\n");
        return NULL;
    }
    
    PhysicalMemory* pm = loader->physical_memory;
    
    // Create PCB
    PCB* pcb = create_pcb(loader->next_pid++);
    if (!pcb) {
        fprintf(stderr, "Error: Failed to create PCB\n");
        return NULL;
    }
    
    // Set process attributes
    set_pcb_priority(pcb, program->header.priority);
    set_pcb_ttl(pcb, program->header.ttl);
    
    // Calculate memory requirements
    uint32_t code_size_bytes = program->header.code_size * WORD_SIZE;
    uint32_t data_size_bytes = program->header.data_size * WORD_SIZE;
    uint32_t code_pages = calculate_pages_needed(code_size_bytes);
    uint32_t data_pages = calculate_pages_needed(data_size_bytes);
    uint32_t total_pages = code_pages + data_pages;
    
    printf("Process %d: Requires %u pages (code=%u, data=%u)\n", 
           pcb->pid, total_pages, code_pages, data_pages);
    
    // Create page table in kernel space
    PageTableEntry* page_table = create_page_table(pm, total_pages);
    if (!page_table) {
        fprintf(stderr, "Error: Failed to create page table for process %d\n", pcb->pid);
        destroy_pcb(pcb);
        return NULL;
    }
    
    // Set page table base in PCB
    pcb->mm.pgb = (void*)page_table;
    
    // Set virtual addresses (code starts at virtual address 0)
    pcb->mm.code = (void*)0;  // Code segment starts at virtual address 0
    pcb->mm.data = (void*)(code_pages * PAGE_SIZE);  // Data segment follows code
    
    // Allocate frames and load code segment
    uint32_t virtual_page = 0;
    for (uint32_t i = 0; i < code_pages; i++) {
        uint32_t frame = allocate_frame(pm);
        if (frame == 0) {
            fprintf(stderr, "Error: Failed to allocate frame for code page %u\n", i);
            // TODO: Cleanup allocated frames
            destroy_pcb(pcb);
            return NULL;
        }
        
        // Update page table entry
        page_table[virtual_page].frame_number = frame;
        page_table[virtual_page].present = 1;
        page_table[virtual_page].rw = 0;  // Code is read-only
        page_table[virtual_page].user = 1;
        
        // Copy code data to physical memory
        uint32_t frame_address = frame * (FRAME_SIZE / WORD_SIZE);  // Convert to word address
        uint32_t words_to_copy = (i < code_pages - 1) ? 
                                 (FRAME_SIZE / WORD_SIZE) : 
                                 (program->header.code_size - i * (FRAME_SIZE / WORD_SIZE));
        
        for (uint32_t j = 0; j < words_to_copy && (i * (FRAME_SIZE / WORD_SIZE) + j) < program->header.code_size; j++) {
            write_word(pm, frame_address + j, 
                      program->code_segment[i * (FRAME_SIZE / WORD_SIZE) + j]);
        }
        
        virtual_page++;
    }
    
    // Allocate frames and load data segment
    for (uint32_t i = 0; i < data_pages; i++) {
        uint32_t frame = allocate_frame(pm);
        if (frame == 0) {
            fprintf(stderr, "Error: Failed to allocate frame for data page %u\n", i);
            // TODO: Cleanup allocated frames
            destroy_pcb(pcb);
            return NULL;
        }
        
        // Update page table entry
        page_table[virtual_page].frame_number = frame;
        page_table[virtual_page].present = 1;
        page_table[virtual_page].rw = 1;  // Data is read-write
        page_table[virtual_page].user = 1;
        
        // Copy data to physical memory
        uint32_t frame_address = frame * (FRAME_SIZE / WORD_SIZE);  // Convert to word address
        uint32_t words_to_copy = (i < data_pages - 1) ? 
                                 (FRAME_SIZE / WORD_SIZE) : 
                                 (program->header.data_size - i * (FRAME_SIZE / WORD_SIZE));
        
        for (uint32_t j = 0; j < words_to_copy && (i * (FRAME_SIZE / WORD_SIZE) + j) < program->header.data_size; j++) {
            write_word(pm, frame_address + j, 
                      program->data_segment[i * (FRAME_SIZE / WORD_SIZE) + j]);
        }
        
        virtual_page++;
    }
    
    loader->total_loaded++;
    printf("Process %d created: '%s' (priority=%d, ttl=%d, pages=%u)\n",
           pcb->pid, program->header.program_name, 
           pcb->priority, pcb->ttl, total_pages);
    
    return pcb;
}

// Load program from file and create process
PCB* load_and_create_process(Loader* loader, const char* filename) {
    // Load program from file
    Program* program = load_program_from_file(filename);
    if (!program) {
        fprintf(stderr, "Error: Failed to load program from '%s'\n", filename);
        return NULL;
    }
    
    // Create process from program
    PCB* pcb = create_process_from_program(loader, program);
    
    // Free program structure (data is now in physical memory)
    destroy_program(program);
    
    return pcb;
}
