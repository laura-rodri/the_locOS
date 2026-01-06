#include "loader.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

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

// Load a program from a .elf file (prometheus format)
// File format:
// .text <hex_address>     # Start of code section
// .data <hex_address>     # Start of data section
// <hex_instruction>       # Instructions/data follow (8 hex digits per line)
// ...
// Note: The .elf format uses absolute virtual addresses
// We need to load the entire program into a contiguous virtual address space
Program* load_program_from_elf(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open ELF file '%s'\n", filename);
        return NULL;
    }
    
    Program* program = malloc(sizeof(Program));
    if (!program) {
        fclose(file);
        return NULL;
    }
    
    // Extract program name from filename
    const char* name_start = strrchr(filename, '/');
    if (name_start) {
        name_start++;  // Skip the '/'
    } else {
        name_start = filename;
    }
    strncpy(program->header.program_name, name_start, MAX_PROGRAM_NAME - 1);
    program->header.program_name[MAX_PROGRAM_NAME - 1] = '\0';
    
    // Default values
    program->header.entry_point = 0;
    program->header.priority = 0;  // Will be set later based on program size
    program->header.ttl = 50;  // Will be set later based on program size
    program->header.text_address = 0;  // Will be set from .text directive
    program->header.data_address = 0;  // Will be set from .data directive
    
    char line[512];
    uint32_t text_addr = 0;
    uint32_t data_addr = 0;
    int found_text = 0;
    int found_data = 0;
    
    // First pass: find .text and .data addresses and count words
    uint32_t total_words = 0;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, ".text", 5) == 0) {
            sscanf(line, ".text %x", &text_addr);
            program->header.text_address = text_addr;  // Already a byte address
            found_text = 1;
        } else if (strncmp(line, ".data", 5) == 0) {
            sscanf(line, ".data %x", &data_addr);
            program->header.data_address = data_addr;  // Already a byte address
            found_data = 1;
        } else if (found_text && line[0] != '.' && line[0] != '\n') {
            // This is a hex word
            uint32_t dummy;
            if (sscanf(line, "%x", &dummy) == 1) {
                total_words++;
            }
        }
    }
    
    if (!found_text) {
        fprintf(stderr, "Error: .text section not found in '%s'\n", filename);
        free(program);
        fclose(file);
        return NULL;
    }
    
    // Calculate the size of code and data based on addresses
    // The entire program is loaded as one contiguous block starting at text_addr
    if (found_data && data_addr > text_addr) {
        // Calculate code size in words (from text_addr to data_addr)
        program->header.code_size = (data_addr - text_addr) / WORD_SIZE;
        // Remaining words are data
        program->header.data_size = total_words - program->header.code_size;
    } else {
        // Everything is code if no .data section
        program->header.code_size = total_words;
        program->header.data_size = 0;
    }
    
    // Assign random priority: -20 (highest) to 19 (lowest)
    // Use a better seed combining time, address, and code size for variety
    static unsigned int seed_counter = 0;
    srand((unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)file ^ seed_counter++ ^ program->header.code_size);
    program->header.priority = MIN_PRIORITY + (rand() % NUM_PRIORITY_LEVELS);
    
    // Calculate realistic TTL based on code size
    // Estimate: ~2-3 ticks per instruction on average, with some margin
    // For small programs: minimum 10 ticks, maximum 100 ticks
    int estimated_ttl = program->header.code_size * 3;
    if (estimated_ttl < 10) estimated_ttl = 10;
    if (estimated_ttl > 100) estimated_ttl = 100;
    program->header.ttl = estimated_ttl;
    
    printf("[Loader] Program '%s': code_size=%u words, priority=%d, TTL=%u ticks\n",
           program->header.program_name, program->header.code_size, 
           program->header.priority, program->header.ttl);
    
    // Allocate one contiguous segment for the entire program
    // This makes it easier to load into virtual memory
    uint32_t total_size = program->header.code_size + program->header.data_size;
    uint32_t* full_program = malloc(sizeof(uint32_t) * total_size);
    if (!full_program) {
        fprintf(stderr, "Error: Failed to allocate program memory\n");
        free(program);
        fclose(file);
        return NULL;
    }
    
    // Second pass: read actual data
    rewind(file);
    uint32_t word_idx = 0;
    int in_sections = 0;
    
    while (fgets(line, sizeof(line), file)) {
        // Skip directive lines
        if (line[0] == '.') {
            in_sections = 1;
            continue;
        }
        
        if (in_sections && line[0] != '\n') {
            uint32_t word_value;
            if (sscanf(line, "%x", &word_value) == 1) {
                if (word_idx < total_size) {
                    full_program[word_idx] = word_value;
                    word_idx++;
                }
            }
        }
    }
    
    fclose(file);
    
    // Split into code and data segments
    program->code_segment = malloc(sizeof(uint32_t) * program->header.code_size);
    if (program->header.data_size > 0) {
        program->data_segment = malloc(sizeof(uint32_t) * program->header.data_size);
    } else {
        program->data_segment = NULL;
    }
    
    if (!program->code_segment || (program->header.data_size > 0 && !program->data_segment)) {
        fprintf(stderr, "Error: Failed to allocate segments\n");
        free(full_program);
        free(program->code_segment);
        free(program->data_segment);
        free(program);
        return NULL;
    }
    
    // Copy data to segments
    memcpy(program->code_segment, full_program, sizeof(uint32_t) * program->header.code_size);
    if (program->header.data_size > 0) {
        memcpy(program->data_segment, full_program + program->header.code_size, 
               sizeof(uint32_t) * program->header.data_size);
    }
    
    free(full_program);
    
    printf("[Loader] ELF Program '%s' loaded: CODE=%u words @0x%06X, DATA=%u words @0x%06X\n",
           program->header.program_name, 
           program->header.code_size, text_addr,
           program->header.data_size, data_addr);
    
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
    
    // Calculate the total memory span needed
    // .text and .data contain WORD offsets from the .elf file
    uint32_t code_start_word = program->header.text_address / WORD_SIZE;  // Should be 0
    uint32_t code_end_word = code_start_word + program->header.code_size;
    uint32_t data_start_word = program->header.data_address / WORD_SIZE;
    uint32_t data_end_word = data_start_word + program->header.data_size;
    
    // The total address space is from word 0 to the end of data
    uint32_t total_words = (data_end_word > code_end_word) ? data_end_word : code_end_word;
    uint32_t total_bytes = total_words * WORD_SIZE;
    uint32_t total_pages = calculate_pages_needed(total_bytes);
    
    printf("[Loader] Process %d: Memory layout - CODE: words 0x%X-0x%X, DATA: words 0x%X-0x%X, Total: %u pages\n", 
           pcb->pid, code_start_word, code_end_word-1, data_start_word, data_end_word-1, total_pages);
    
    // Create page table in kernel space
    PageTableEntry* page_table = create_page_table(pm, total_pages);
    if (!page_table) {
        fprintf(stderr, "Error: Failed to create page table for process %d\n", pcb->pid);
        destroy_pcb(pcb);
        return NULL;
    }
    
    // Set page table base in PCB
    pcb->mm.pgb = (void*)page_table;
    
    // Set virtual addresses (preserve original layout)
    pcb->mm.code = (void*)(uintptr_t)(code_start_word * WORD_SIZE);
    pcb->mm.data = (void*)(uintptr_t)(data_start_word * WORD_SIZE);
    
    // Allocate frames and initialize entire address space
    uint32_t words_per_page = FRAME_SIZE / WORD_SIZE;
    
    for (uint32_t page = 0; page < total_pages; page++) {
        uint32_t frame = allocate_frame(pm);
        if (frame == 0) {
            fprintf(stderr, "Error: Failed to allocate frame for page %u\n", page);
            // TODO: Cleanup allocated frames
            destroy_pcb(pcb);
            return NULL;
        }
        
        // Update page table entry
        page_table[page].frame_number = frame;
        page_table[page].present = 1;
        page_table[page].rw = 1;  // Allow read-write for simplicity
        page_table[page].user = 1;
        
        // Calculate physical frame address
        uint32_t frame_address = frame * words_per_page;
        
        // Initialize this page (fill with zeros first)
        for (uint32_t j = 0; j < words_per_page; j++) {
            write_word(pm, frame_address + j, 0);
        }
        
        // Now copy actual code and data to their correct positions
        uint32_t page_start_word = page * words_per_page;
        uint32_t page_end_word = page_start_word + words_per_page;
        
        // Copy code if it overlaps this page
        if (code_start_word < page_end_word && code_end_word > page_start_word) {
            uint32_t copy_start = (code_start_word > page_start_word) ? code_start_word : page_start_word;
            uint32_t copy_end = (code_end_word < page_end_word) ? code_end_word : page_end_word;
            
            for (uint32_t word = copy_start; word < copy_end; word++) {
                uint32_t code_idx = word - code_start_word;
                uint32_t offset_in_page = word - page_start_word;
                write_word(pm, frame_address + offset_in_page, program->code_segment[code_idx]);
            }
        }
        
        // Copy data if it overlaps this page
        if (data_start_word < page_end_word && data_end_word > page_start_word) {
            uint32_t copy_start = (data_start_word > page_start_word) ? data_start_word : page_start_word;
            uint32_t copy_end = (data_end_word < page_end_word) ? data_end_word : page_end_word;
            
            for (uint32_t word = copy_start; word < copy_end; word++) {
                uint32_t data_idx = word - data_start_word;
                uint32_t offset_in_page = word - page_start_word;
                write_word(pm, frame_address + offset_in_page, program->data_segment[data_idx]);
            }
        }
    }
    
    loader->total_loaded++;
    printf("[Loader] Process %d created: '%s' (priority=%d, ttl=%d, pages=%u)\n",
           pcb->pid, program->header.program_name, 
           pcb->priority, pcb->ttl, total_pages);
    
    return pcb;
}
