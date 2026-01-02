#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "memory.h"

// Simplified PCB structure and functions for testing (to avoid process.c dependencies)
typedef struct {
    void* code;
    void* data;
    void* pgb;
} MemoryManagement_Test;

typedef struct {
    int pid;
    int state;
    int priority;
    int ttl;
    int initial_ttl;
    int quantum_counter;
    int virtual_deadline;
    MemoryManagement_Test mm;
} PCB_Test;

PCB_Test* create_test_pcb(int pid) {
    PCB_Test* pcb = malloc(sizeof(PCB_Test));
    if (!pcb) return NULL;
    
    pcb->pid = pid;
    pcb->state = 1;  // WAITING
    pcb->priority = 0;
    pcb->ttl = 0;
    pcb->initial_ttl = 0;
    pcb->quantum_counter = 0;
    pcb->virtual_deadline = 0;
    pcb->mm.code = NULL;
    pcb->mm.data = NULL;
    pcb->mm.pgb = NULL;
    
    return pcb;
}

// Program structure (simplified from loader.h)
#define MAX_PROGRAM_NAME 256

typedef struct {
    char program_name[MAX_PROGRAM_NAME];
    uint32_t code_size;
    uint32_t data_size;
    uint32_t entry_point;
    uint32_t priority;
    uint32_t ttl;
} ProgramHeader_Test;

typedef struct {
    ProgramHeader_Test header;
    uint32_t* code_segment;
    uint32_t* data_segment;
} Program_Test;

// Load program from file (simplified)
Program_Test* load_program_test(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return NULL;
    }
    
    Program_Test* prog = malloc(sizeof(Program_Test));
    if (!prog) {
        fclose(file);
        return NULL;
    }
    
    char line[512];
    char keyword[64];
    
    // Read header
    if (fgets(line, sizeof(line), file) &&
        sscanf(line, "%s %s", keyword, prog->header.program_name) == 2 &&
        fgets(line, sizeof(line), file) &&
        sscanf(line, "%s %u", keyword, &prog->header.code_size) == 2 &&
        fgets(line, sizeof(line), file) &&
        sscanf(line, "%s %u", keyword, &prog->header.data_size) == 2 &&
        fgets(line, sizeof(line), file) &&
        sscanf(line, "%s %u", keyword, &prog->header.entry_point) == 2 &&
        fgets(line, sizeof(line), file) &&
        sscanf(line, "%s %u", keyword, &prog->header.priority) == 2 &&
        fgets(line, sizeof(line), file) &&
        sscanf(line, "%s %u", keyword, &prog->header.ttl) == 2) {
        
        // Allocate segments
        prog->code_segment = malloc(sizeof(uint32_t) * prog->header.code_size);
        prog->data_segment = malloc(sizeof(uint32_t) * prog->header.data_size);
        
        if (!prog->code_segment || !prog->data_segment) {
            free(prog->code_segment);
            free(prog->data_segment);
            free(prog);
            fclose(file);
            return NULL;
        }
        
        // Skip CODE_SECTION line
        fgets(line, sizeof(line), file);
        
        // Read code
        for (uint32_t i = 0; i < prog->header.code_size; i++) {
            if (fgets(line, sizeof(line), file)) {
                sscanf(line, "%x", &prog->code_segment[i]);
            }
        }
        
        // Skip DATA_SECTION line
        fgets(line, sizeof(line), file);
        
        // Read data
        for (uint32_t i = 0; i < prog->header.data_size; i++) {
            if (fgets(line, sizeof(line), file)) {
                sscanf(line, "%x", &prog->data_segment[i]);
            }
        }
        
        fclose(file);
        return prog;
    }
    
    free(prog);
    fclose(file);
    return NULL;
}

// Calculate pages needed
uint32_t calculate_pages_needed(uint32_t size_in_bytes) {
    if (size_in_bytes == 0) return 0;
    return (size_in_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
}

// Test program to demonstrate the memory and loader functionality
int main() {
    printf("=== Memory Virtual - Fase 1: Test de Estructuras ===\n\n");
    
    // 1. Create Physical Memory
    printf("1. Creando memoria física...\n");
    PhysicalMemory* pm = create_physical_memory();
    if (!pm) {
        fprintf(stderr, "Error: Failed to create physical memory\n");
        return 1;
    }
    printf("\n");
    
    // 2. Test page table creation
    printf("2. Probando creación de tabla de páginas...\n");
    PageTableEntry* test_pt = create_page_table(pm, 10);
    if (test_pt) {
        printf("   ✓ Tabla de páginas creada (10 páginas)\n");
        printf("   Dirección de la tabla: %p\n", (void*)test_pt);
    } else {
        printf("   ✗ Error al crear tabla de páginas\n");
    }
    printf("\n");
    
    // 3. Test frame allocation
    printf("3. Probando asignación de marcos...\n");
    uint32_t frames[5];
    for (int i = 0; i < 5; i++) {
        frames[i] = allocate_frame(pm);
        printf("   Marco asignado %d: %u\n", i, frames[i]);
    }
    printf("\n");
    
    // 4. Test memory read/write
    printf("4. Probando lectura/escritura de memoria...\n");
    uint32_t test_addr = frames[0] * (FRAME_SIZE / WORD_SIZE);
    write_word(pm, test_addr, 0xDEADBEEF);
    uint32_t read_val = read_word(pm, test_addr);
    printf("   Escrito: 0x%08X\n", 0xDEADBEEF);
    printf("   Leído:   0x%08X\n", read_val);
    if (read_val == 0xDEADBEEF) {
        printf("   ✓ Lectura/escritura correcta\n");
    } else {
        printf("   ✗ Error en lectura/escritura\n");
    }
    printf("\n");
    
    // 5. Test loading a program from file
    printf("5. Probando carga de programa desde archivo...\n");
    Program_Test* prog = load_program_test("programs/simple_add.txt");
    if (prog) {
        printf("   ✓ Programa cargado\n");
        printf("   Nombre: %s\n", prog->header.program_name);
        printf("   Tamaño código: %u palabras\n", prog->header.code_size);
        printf("   Tamaño datos: %u palabras\n", prog->header.data_size);
        printf("   Prioridad: %u\n", prog->header.priority);
        printf("   TTL: %u\n", prog->header.ttl);
        
        // Display some code
        if (prog->header.code_size > 0) {
            printf("   Primeras palabras de código:\n");
            for (uint32_t i = 0; i < prog->header.code_size && i < 4; i++) {
                printf("     [%u]: 0x%08X\n", i, prog->code_segment[i]);
            }
        }
        
        free(prog->code_segment);
        free(prog->data_segment);
        free(prog);
    } else {
        printf("   ✗ Error al cargar programa\n");
    }
    printf("\n");
    
    // 6. Test creating PCB with page table
    printf("6. Probando creación de PCB con tabla de páginas...\n");
    Program_Test* prog2 = load_program_test("programs/simple_add.txt");
    if (prog2) {
        PCB_Test* pcb = create_test_pcb(1);
        if (pcb) {
            pcb->priority = prog2->header.priority;
            pcb->ttl = prog2->header.ttl;
            pcb->initial_ttl = prog2->header.ttl;
            
            // Calculate pages needed
            uint32_t code_size_bytes = prog2->header.code_size * WORD_SIZE;
            uint32_t data_size_bytes = prog2->header.data_size * WORD_SIZE;
            uint32_t code_pages = calculate_pages_needed(code_size_bytes);
            uint32_t data_pages = calculate_pages_needed(data_size_bytes);
            uint32_t total_pages = code_pages + data_pages;
            
            printf("   Páginas necesarias: %u (código=%u, datos=%u)\n", 
                   total_pages, code_pages, data_pages);
            
            // Create page table
            PageTableEntry* pt = create_page_table(pm, total_pages);
            if (pt) {
                pcb->mm.pgb = (void*)pt;
                pcb->mm.code = (void*)0;
                pcb->mm.data = (void*)(uintptr_t)(code_pages * PAGE_SIZE);
                
                // Allocate frames and load data
                uint32_t page_idx = 0;
                for (uint32_t i = 0; i < code_pages; i++) {
                    uint32_t frame = allocate_frame(pm);
                    pt[page_idx].frame_number = frame;
                    pt[page_idx].present = 1;
                    pt[page_idx].rw = 0;  // Read-only
                    pt[page_idx].user = 1;
                    page_idx++;
                }
                for (uint32_t i = 0; i < data_pages; i++) {
                    uint32_t frame = allocate_frame(pm);
                    pt[page_idx].frame_number = frame;
                    pt[page_idx].present = 1;
                    pt[page_idx].rw = 1;  // Read-write
                    pt[page_idx].user = 1;
                    page_idx++;
                }
                
                printf("   ✓ PCB configurado con tabla de páginas\n");
                printf("     - PID: %d\n", pcb->pid);
                printf("     - Código: %p\n", pcb->mm.code);
                printf("     - Datos: %p\n", pcb->mm.data);
                printf("     - Tabla páginas: %p\n", pcb->mm.pgb);
                printf("   Tabla de páginas:\n");
                for (uint32_t i = 0; i < total_pages; i++) {
                    printf("     Página %u: Frame=%u, Present=%d, RW=%d\n",
                           i, pt[i].frame_number, pt[i].present, pt[i].rw);
                }
                
                free(pcb);
            } else {
                printf("   ✗ Error al crear tabla de páginas\n");
                free(pcb);
            }
        }
        free(prog2->code_segment);
        free(prog2->data_segment);
        free(prog2);
    }
    printf("\n");
    
    // 7. Display memory statistics
    printf("7. Estadísticas finales de memoria:\n");
    printf("   Total frames asignados: %u\n", pm->total_allocated_frames);
    printf("   Espacio kernel usado: %u palabras\n", pm->next_kernel_frame);
    printf("\n");
    
    // 8. Cleanup
    printf("8. Limpiando recursos...\n");
    destroy_physical_memory(pm);
    printf("   ✓ Recursos liberados\n\n");
    
    printf("=== Test completado exitosamente ===\n");
    return 0;
}
