# Integración del Loader con el Kernel - Guía

## Cómo integrar el Loader en kernel.c

### 1. Incluir los headers necesarios

En `kernel.c`, añadir al principio con los demás includes:

```c
#include "memory.h"
#include "loader.h"
```

### 2. Crear la memoria física global

Añadir al principio del archivo con las otras variables globales:

```c
static PhysicalMemory* physical_memory_global = NULL;
static Loader* loader_global = NULL;
```

### 3. Inicializar en main()

En la función `main()`, después de crear la máquina y antes del scheduler:

```c
// Create Physical Memory
printf("Creating physical memory...\n");
physical_memory_global = create_physical_memory();
if (!physical_memory_global) {
    fprintf(stderr, "Error: Failed to create physical memory\n");
    return 1;
}

// Create Loader
printf("Creating loader...\n");
loader_global = create_loader(physical_memory_global, ready_queue, 
                               machine, scheduler);
if (!loader_global) {
    fprintf(stderr, "Error: Failed to create loader\n");
    destroy_physical_memory(physical_memory_global);
    return 1;
}
```

### 4. Cargar programas en lugar de generarlos

En lugar de usar el Process Generator, cargar programas desde archivos:

```c
// Load some initial programs
printf("Loading initial programs...\n");

const char* initial_programs[] = {
    "programs/simple_add.txt",
    "programs/high_priority.txt",
    "programs/loop_test.txt"
};

for (int i = 0; i < 3; i++) {
    PCB* pcb = load_and_create_process(loader_global, initial_programs[i]);
    if (pcb) {
        enqueue_process(ready_queue, pcb);
        printf("Loaded program: %s (PID=%d)\n", initial_programs[i], pcb->pid);
    } else {
        fprintf(stderr, "Warning: Failed to load %s\n", initial_programs[i]);
    }
}
```

### 5. Actualizar cleanup_system()

En la función `cleanup_system()`, añadir la limpieza de memoria y loader:

```c
void cleanup_system(pthread_t clock_thread, Timer** timers, int num_timers) {
    // ... código existente ...
    
    // Cleanup loader
    if (loader_global) {
        printf("Cleaning up loader...\n");
        destroy_loader(loader_global);
        loader_global = NULL;
    }
    
    // Cleanup physical memory
    if (physical_memory_global) {
        printf("Cleaning up physical memory...\n");
        destroy_physical_memory(physical_memory_global);
        physical_memory_global = NULL;
    }
    
    // ... resto del código ...
}
```

### 6. Actualizar assign_process_to_core() para usar Hardware Threads

Modificar la función para usar los nuevos Hardware Threads:

```c
int assign_process_to_core_with_hw_thread(Machine* machine, PCB* pcb) {
    if (!machine || !pcb) return 0;
    
    for (int i = 0; i < machine->num_CPUs; i++) {
        for (int j = 0; j < machine->cpus[i].num_cores; j++) {
            Core* core = &machine->cpus[i].cores[j];
            
            // Find free hardware thread
            for (int k = 0; k < core->num_kernel_threads; k++) {
                HardwareThread* hw_thread = &core->hw_threads[k];
                if (hw_thread->pcb == NULL) {
                    // Assign PCB to hardware thread
                    hw_thread->pcb = pcb;
                    hw_thread->PC = 0;  // Start at beginning
                    hw_thread->PTBR = pcb->mm.pgb;  // Set page table base
                    hw_thread->mmu.page_table_base = pcb->mm.pgb;
                    hw_thread->mmu.enabled = 1;  // Enable MMU
                    
                    // Clear TLB
                    for (int t = 0; t < TLB_SIZE; t++) {
                        hw_thread->tlb.entries[t].valid = 0;
                    }
                    
                    // Also update legacy array for compatibility
                    if (core->current_pcb_count < core->num_kernel_threads) {
                        core->pcbs[core->current_pcb_count] = *pcb;
                        core->current_pcb_count++;
                    }
                    
                    return 1;  // Success
                }
            }
        }
    }
    
    return 0;  // No free hardware thread found
}
```

### 7. Actualizar remove_process_from_core()

Actualizar para limpiar el Hardware Thread:

```c
int remove_process_from_core_with_hw_thread(Machine* machine, int pid) {
    if (!machine) return 0;
    
    for (int i = 0; i < machine->num_CPUs; i++) {
        for (int j = 0; j < machine->cpus[i].num_cores; j++) {
            Core* core = &machine->cpus[i].cores[j];
            
            // Check hardware threads
            for (int k = 0; k < core->num_kernel_threads; k++) {
                HardwareThread* hw_thread = &core->hw_threads[k];
                if (hw_thread->pcb && hw_thread->pcb->pid == pid) {
                    // Clear hardware thread
                    hw_thread->pcb = NULL;
                    hw_thread->PC = 0;
                    hw_thread->IR = 0;
                    hw_thread->PTBR = NULL;
                    hw_thread->mmu.page_table_base = NULL;
                    hw_thread->mmu.enabled = 0;
                    
                    // Clear TLB
                    for (int t = 0; t < TLB_SIZE; t++) {
                        hw_thread->tlb.entries[t].valid = 0;
                    }
                    
                    // Also handle legacy array
                    for (int l = k; l < core->current_pcb_count - 1; l++) {
                        core->pcbs[l] = core->pcbs[l + 1];
                    }
                    core->current_pcb_count--;
                    
                    return 1;  // Success
                }
            }
        }
    }
    
    return 0;  // Not found
}
```

## Ejemplo Completo de Integración

Ver el archivo `kernel.c` original y modificar siguiendo estos pasos. El flujo sería:

1. **main()** inicia
2. Se crea la **Physical Memory**
3. Se crea el **Loader**
4. Se cargan programas iniciales desde archivos
5. Los PCBs se añaden a la **ready queue**
6. El **Scheduler** los asigna a **Hardware Threads**
7. Cada **Hardware Thread** tiene su **MMU** y **TLB** configurados
8. (Fase 2) Los procesos se ejecutan con traducción de direcciones

## Notas Importantes

- **NO** eliminar el Process Generator aún - puede convivir con el Loader
- El Loader puede usarse para cargar procesos específicos
- El Process Generator puede seguir creando procesos aleatorios si se desea
- Los Hardware Threads están listos pero no ejecutan instrucciones (Fase 2)

## Verificación

Para verificar la integración:

1. Compilar: `make clean && make`
2. Ejecutar kernel con los programas cargados
3. Verificar que los PCBs tienen `mm.code`, `mm.data`, `mm.pgb` configurados
4. Verificar que los Hardware Threads tienen PTBR apuntando a tablas de páginas

## Debugging

Para ver información de memoria en tiempo de ejecución:

```c
// En cualquier punto después de cargar procesos
printf("Memory Statistics:\n");
printf("  Total frames allocated: %u\n", 
       physical_memory_global->total_allocated_frames);
printf("  Kernel space used: %u words\n", 
       physical_memory_global->next_kernel_frame);

// Para ver tabla de páginas de un proceso
PageTableEntry* pt = (PageTableEntry*)pcb->mm.pgb;
for (int i = 0; i < num_pages; i++) {
    printf("  Page %d: Frame=%u, Present=%d, RW=%d\n",
           i, pt[i].frame_number, pt[i].present, pt[i].rw);
}
```

## Migración Gradual

Se recomienda:

1. **Fase 1a**: Mantener Process Generator, añadir Loader para casos específicos
2. **Fase 1b**: Usar Loader para procesos iniciales, Generator para dinámicos
3. **Fase 2**: Eliminar Generator completamente si no se necesita

Esta aproximación gradual permite mantener la funcionalidad existente mientras se prueba el nuevo sistema.
