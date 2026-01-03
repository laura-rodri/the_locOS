#include "memory.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Create and initialize physical memory
PhysicalMemory* create_physical_memory() {
    PhysicalMemory* pm = malloc(sizeof(PhysicalMemory));
    if (!pm) {
        fprintf(stderr, "Error: Failed to allocate PhysicalMemory structure\n");
        return NULL;
    }
    
    // Allocate memory array (in words)
    pm->memory = calloc(TOTAL_WORDS, sizeof(uint32_t));
    if (!pm->memory) {
        fprintf(stderr, "Error: Failed to allocate physical memory array\n");
        free(pm);
        return NULL;
    }
    
    // Allocate frame bitmap (1 bit per frame, use 1 byte per frame for simplicity)
    pm->frame_bitmap = calloc(TOTAL_FRAMES, sizeof(uint8_t));
    if (!pm->frame_bitmap) {
        fprintf(stderr, "Error: Failed to allocate frame bitmap\n");
        free(pm->memory);
        free(pm);
        return NULL;
    }
    
    // Initialize memory boundaries
    pm->kernel_space_end = KERNEL_SPACE_WORDS;
    pm->user_space_start = KERNEL_SPACE_WORDS;
    pm->next_kernel_frame = 0;  // Start allocating kernel frames from 0
    pm->total_allocated_frames = 0;
    
    // Mark kernel frames as allocated
    for (uint32_t i = 0; i < KERNEL_FRAMES; i++) {
        pm->frame_bitmap[i] = 1;  // Mark as allocated (reserved for kernel)
    }
    pm->total_allocated_frames = KERNEL_FRAMES;
    
    printf("Physical Memory initialized:\n");
    printf("  Total size: %u bytes (%u words)\n", PHYSICAL_MEMORY_SIZE, TOTAL_WORDS);
    printf("  Kernel space: %u bytes (%u words, %u frames)\n", 
           KERNEL_SPACE_SIZE, KERNEL_SPACE_WORDS, KERNEL_FRAMES);
    printf("  User space: %u bytes (%u words, %u frames)\n", 
           PHYSICAL_MEMORY_SIZE - KERNEL_SPACE_SIZE, 
           TOTAL_WORDS - KERNEL_SPACE_WORDS,
           USER_FRAMES);
    printf("  Address bus: %d bits\n", ADDRESS_BUS_BITS);
    printf("  Word size: %d bytes\n", WORD_SIZE);
    printf("  Page/Frame size: %d bytes\n", PAGE_SIZE);
    
    return pm;
}

// Destroy physical memory and free resources
void destroy_physical_memory(PhysicalMemory* pm) {
    if (pm) {
        free(pm->memory);
        free(pm->frame_bitmap);
        free(pm);
    }
}

// Allocate a frame from user space
uint32_t allocate_frame(PhysicalMemory* pm) {
    if (!pm) return 0;
    
    // Search for a free frame in user space (starting from KERNEL_FRAMES)
    for (uint32_t i = KERNEL_FRAMES; i < TOTAL_FRAMES; i++) {
        if (pm->frame_bitmap[i] == 0) {
            pm->frame_bitmap[i] = 1;  // Mark as allocated
            pm->total_allocated_frames++;
            return i;
        }
    }
    
    fprintf(stderr, "Error: No free frames available\n");
    return 0;  // No free frame found
}

// Free a frame
void free_frame(PhysicalMemory* pm, uint32_t frame_number) {
    if (!pm || frame_number >= TOTAL_FRAMES || frame_number < KERNEL_FRAMES) {
        fprintf(stderr, "Error: Invalid frame number %u\n", frame_number);
        return;
    }
    
    if (pm->frame_bitmap[frame_number] == 1) {
        pm->frame_bitmap[frame_number] = 0;  // Mark as free
        pm->total_allocated_frames--;
    }
}

// Check if a frame is allocated
int is_frame_allocated(PhysicalMemory* pm, uint32_t frame_number) {
    if (!pm || frame_number >= TOTAL_FRAMES) {
        return 0;
    }
    return pm->frame_bitmap[frame_number];
}

// Allocate space in kernel area (for page tables)
void* allocate_kernel_space(PhysicalMemory* pm, uint32_t size_in_words) {
    if (!pm) return NULL;
    
    // Check if there's enough space in kernel area
    if (pm->next_kernel_frame + size_in_words > KERNEL_SPACE_WORDS) {
        fprintf(stderr, "Error: Kernel space exhausted\n");
        return NULL;
    }
    
    // Calculate physical address (in words)
    uint32_t physical_address = pm->next_kernel_frame;
    
    // Update next available position
    pm->next_kernel_frame += size_in_words;
    
    // Return pointer to physical memory location
    return (void*)&pm->memory[physical_address];
}

// Read a word from physical memory
uint32_t read_word(PhysicalMemory* pm, uint32_t address) {
    if (!pm || address >= TOTAL_WORDS) {
        fprintf(stderr, "Error: Invalid memory address %u\n", address);
        return 0;
    }
    return pm->memory[address];
}

// Write a word to physical memory
void write_word(PhysicalMemory* pm, uint32_t address, uint32_t value) {
    if (!pm || address >= TOTAL_WORDS) {
        fprintf(stderr, "Error: Invalid memory address %u\n", address);
        return;
    }
    pm->memory[address] = value;
}

// Create a page table for a process
PageTableEntry* create_page_table(PhysicalMemory* pm, uint32_t num_pages) {
    if (!pm) return NULL;
    
    // Allocate space in kernel area for the page table
    uint32_t size_needed = num_pages * sizeof(PageTableEntry) / WORD_SIZE;
    if (num_pages * sizeof(PageTableEntry) % WORD_SIZE != 0) {
        size_needed++;  // Round up
    }
    
    PageTableEntry* page_table = (PageTableEntry*)allocate_kernel_space(pm, size_needed);
    if (!page_table) {
        fprintf(stderr, "Error: Failed to allocate page table in kernel space\n");
        return NULL;
    }
    
    // Initialize page table entries
    for (uint32_t i = 0; i < num_pages; i++) {
        page_table[i].frame_number = 0;
        page_table[i].present = 0;      // Not in memory initially
        page_table[i].rw = 1;            // Read/write
        page_table[i].user = 1;          // User mode
        page_table[i].accessed = 0;
        page_table[i].dirty = 0;
        page_table[i].reserved = 0;
    }
    
    return page_table;
}

// Destroy a page table
void destroy_page_table(PhysicalMemory* pm, PageTableEntry* page_table, uint32_t num_pages) {
    if (!pm || !page_table) return;
    
    // Free all allocated frames in the page table
    for (uint32_t i = 0; i < num_pages; i++) {
        if (page_table[i].present) {
            free_frame(pm, page_table[i].frame_number);
        }
    }
    
    // Note: We don't free the page table itself from kernel space
    // In a real OS, kernel space would have its own allocation/deallocation
    // For this simulation, kernel space is simply a sequential allocator
}

// MMU: Translate virtual address to physical address
// Virtual Address = [Virtual Page Number | Offset]
// Physical Address = [Physical Frame Number | Offset]
uint32_t translate_virtual_to_physical(PhysicalMemory* pm, PageTableEntry* page_table, 
                                       uint32_t virtual_address) {
    if (!pm || !page_table) {
        fprintf(stderr, "Error: Invalid PM or page table in MMU translation\n");
        return 0;
    }
    
    // Extract virtual page number and offset
    uint32_t offset = virtual_address & ((1 << PAGE_OFFSET_BITS) - 1);  // Lower 12 bits
    uint32_t virtual_page = virtual_address >> PAGE_OFFSET_BITS;         // Upper bits
    
    // Check if page is present in memory
    if (!page_table[virtual_page].present) {
        fprintf(stderr, "Error: Page fault! Virtual page %u not present in memory\n", virtual_page);
        return 0;
    }
    
    // Get physical frame number from page table
    uint32_t frame_number = page_table[virtual_page].frame_number;
    
    // Calculate physical address (in bytes)
    uint32_t physical_address_bytes = (frame_number << PAGE_OFFSET_BITS) | offset;
    
    // Convert to word address
    uint32_t physical_address_words = physical_address_bytes / WORD_SIZE;
    
    return physical_address_words;
}

// MMU: Read a word using virtual address
uint32_t mmu_read_word(PhysicalMemory* pm, PageTableEntry* page_table, 
                       uint32_t virtual_address) {
    if (!pm || !page_table) {
        fprintf(stderr, "Error: Invalid PM or page table in MMU read\n");
        return 0;
    }
    
    // Translate virtual address to physical address
    uint32_t physical_address = translate_virtual_to_physical(pm, page_table, virtual_address);
    
    // Read from physical memory
    return read_word(pm, physical_address);
}

// MMU: Write a word using virtual address
void mmu_write_word(PhysicalMemory* pm, PageTableEntry* page_table, 
                    uint32_t virtual_address, uint32_t value) {
    if (!pm || !page_table) {
        fprintf(stderr, "Error: Invalid PM or page table in MMU write\n");
        return;
    }
    
    // Translate virtual address to physical address
    uint32_t physical_address = translate_virtual_to_physical(pm, page_table, virtual_address);
    
    // Extract virtual page number to mark as dirty
    uint32_t virtual_page = virtual_address >> PAGE_OFFSET_BITS;
    page_table[virtual_page].dirty = 1;
    page_table[virtual_page].accessed = 1;
    
    // Write to physical memory
    write_word(pm, physical_address, value);
}

