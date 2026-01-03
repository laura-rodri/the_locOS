#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

// Physical Memory Configuration
// 24-bit address bus = 2^24 = 16,777,216 bytes addressable
// Word size = 4 bytes
// Total words = 16,777,216 / 4 = 4,194,304 words

#define ADDRESS_BUS_BITS 24
#define WORD_SIZE 4  // 4 bytes per word
#define PHYSICAL_MEMORY_SIZE (1 << ADDRESS_BUS_BITS)  // 2^24 = 16,777,216 bytes
#define TOTAL_WORDS (PHYSICAL_MEMORY_SIZE / WORD_SIZE)  // 4,194,304 words

// Kernel reserved space (for page tables)
// Reserve first 1 MB (256K words) for kernel space
#define KERNEL_SPACE_SIZE (1024 * 1024)  // 1 MB in bytes
#define KERNEL_SPACE_WORDS (KERNEL_SPACE_SIZE / WORD_SIZE)  // 256K words
#define USER_SPACE_START_ADDRESS KERNEL_SPACE_SIZE

// Page and frame configuration
#define PAGE_SIZE 4096  // 4 KB pages
#define FRAME_SIZE PAGE_SIZE
#define PAGE_OFFSET_BITS 12  // log2(4096) = 12 bits for offset
#define TOTAL_FRAMES (PHYSICAL_MEMORY_SIZE / FRAME_SIZE)
#define KERNEL_FRAMES (KERNEL_SPACE_SIZE / FRAME_SIZE)
#define USER_FRAMES (TOTAL_FRAMES - KERNEL_FRAMES)

// Page Table Entry structure
typedef struct {
    uint32_t frame_number : 12;  // Physical frame number (12 bits = up to 4096 frames)
    uint32_t present : 1;        // Present bit (1 = in memory, 0 = not in memory)
    uint32_t rw : 1;             // Read/write bit (1 = writable, 0 = read-only)
    uint32_t user : 1;           // User/supervisor bit (1 = user, 0 = supervisor)
    uint32_t accessed : 1;       // Accessed bit
    uint32_t dirty : 1;          // Dirty bit (modified)
    uint32_t reserved : 15;      // Reserved for future use
} PageTableEntry;

// Physical Memory structure
typedef struct {
    uint32_t* memory;                    // Array of words (4 bytes each)
    uint8_t* frame_bitmap;               // Bitmap for frame allocation (1 bit per frame)
    uint32_t kernel_space_end;           // End address of kernel space (in words)
    uint32_t user_space_start;           // Start address of user space (in words)
    uint32_t next_kernel_frame;          // Next available frame in kernel space (for page tables)
    uint32_t total_allocated_frames;     // Total frames allocated
} PhysicalMemory;

// Physical Memory Management Functions
PhysicalMemory* create_physical_memory();
void destroy_physical_memory(PhysicalMemory* pm);

// Frame allocation
uint32_t allocate_frame(PhysicalMemory* pm);
void free_frame(PhysicalMemory* pm, uint32_t frame_number);
int is_frame_allocated(PhysicalMemory* pm, uint32_t frame_number);

// Kernel space management (for page tables)
void* allocate_kernel_space(PhysicalMemory* pm, uint32_t size_in_words);

// Memory access (word-based)
uint32_t read_word(PhysicalMemory* pm, uint32_t address);
void write_word(PhysicalMemory* pm, uint32_t address, uint32_t value);

// Page table management
PageTableEntry* create_page_table(PhysicalMemory* pm, uint32_t num_pages);
void destroy_page_table(PhysicalMemory* pm, PageTableEntry* page_table, uint32_t num_pages);

// MMU - Address Translation
uint32_t translate_virtual_to_physical(PhysicalMemory* pm, PageTableEntry* page_table, 
                                       uint32_t virtual_address);
uint32_t mmu_read_word(PhysicalMemory* pm, PageTableEntry* page_table, 
                       uint32_t virtual_address);
void mmu_write_word(PhysicalMemory* pm, PageTableEntry* page_table, 
                    uint32_t virtual_address, uint32_t value);

#endif // MEMORY_H
