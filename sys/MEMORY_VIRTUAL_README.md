# Memoria Virtual - Fase 1: Preparación del Entorno

## Resumen de Cambios

Este documento describe las modificaciones realizadas para preparar el sistema operativo simulado para soportar memoria virtual. Esta es la **Fase 1**, enfocada únicamente en la creación de estructuras y componentes de hardware simulado.

## 1. Modificación del PCB (Process Control Block)

### Cambios en `process.h` y `process.c`

Se ha añadido una estructura `MemoryManagement` al PCB:

```c
typedef struct {
    void* code;   // Dirección virtual de inicio del segmento de código
    void* data;   // Dirección virtual de inicio del segmento de datos
    void* pgb;    // Dirección física de la tabla de páginas (Page Table Base)
} MemoryManagement;

typedef struct {
    int pid;
    int state;
    int priority;
    int ttl;
    int initial_ttl;
    int quantum_counter;
    int virtual_deadline;
    MemoryManagement mm;  // NUEVO: Gestión de memoria
} PCB;
```

**Inicialización**: En `create_pcb()`, los campos se inicializan a NULL.

## 2. Memoria Física Simulada

### Archivos: `memory.h` y `memory.c`

Se ha creado una estructura completa para simular la RAM:

#### Características:
- **Bus de direcciones**: 24 bits (16 MB direccionables)
- **Tamaño de palabra**: 4 bytes
- **Total de palabras**: 4,194,304 (16 MB / 4 bytes)
- **Espacio reservado para Kernel**: 1 MB (256K palabras)
  - Para almacenar tablas de páginas
- **Espacio de usuario**: 15 MB

#### Configuración de Páginas:
- **Tamaño de página/marco**: 4 KB
- **Total de marcos**: 4,096
- **Marcos del kernel**: 256
- **Marcos de usuario**: 3,840

#### Estructura Principal:
```c
typedef struct {
    uint32_t* memory;              // Array de palabras (4 bytes cada una)
    uint8_t* frame_bitmap;         // Bitmap para asignación de marcos
    uint32_t kernel_space_end;     // Fin del espacio del kernel
    uint32_t user_space_start;     // Inicio del espacio de usuario
    uint32_t next_kernel_frame;    // Siguiente marco disponible en kernel
    uint32_t total_allocated_frames;
} PhysicalMemory;
```

#### Entrada de Tabla de Páginas:
```c
typedef struct {
    uint32_t frame_number : 12;  // Número de marco físico
    uint32_t present : 1;        // Bit de presencia
    uint32_t rw : 1;             // Lectura/escritura
    uint32_t user : 1;           // Usuario/supervisor
    uint32_t accessed : 1;       // Bit de acceso
    uint32_t dirty : 1;          // Bit de modificación
    uint32_t reserved : 15;      // Reservado
} PageTableEntry;
```

#### Funciones Principales:
- `create_physical_memory()`: Inicializa la memoria física
- `allocate_frame()`: Asigna un marco de memoria
- `free_frame()`: Libera un marco
- `create_page_table()`: Crea tabla de páginas en espacio del kernel
- `read_word()` / `write_word()`: Acceso a memoria

## 3. Hardware Thread Ampliado

### Cambios en `machine.h` y `machine.c`

Se ha extendido la estructura de Core para incluir Hardware Threads con registros completos:

#### TLB (Translation Lookaside Buffer):
```c
#define TLB_SIZE 16

typedef struct {
    uint32_t virtual_page;
    uint32_t physical_frame;
    uint8_t valid;
} TLBEntry;

typedef struct {
    TLBEntry entries[TLB_SIZE];
    int next_replace;  // Reemplazo round-robin
} TLB;
```

#### MMU (Memory Management Unit):
```c
typedef struct {
    void* page_table_base;  // Base de tabla de páginas
    int enabled;            // MMU habilitada
} MMU;
```

#### Hardware Thread:
```c
typedef struct {
    uint32_t PC;      // Program Counter
    uint32_t IR;      // Instruction Register
    void* PTBR;       // Page Table Base Register
    MMU mmu;          // Unidad de gestión de memoria
    TLB tlb;          // Buffer de traducción
    PCB* pcb;         // PCB asociado (NULL si idle)
} HardwareThread;
```

#### Core Actualizado:
```c
typedef struct Core {
    int num_kernel_threads;
    int current_pcb_count;       // DEPRECATED
    PCB* pcbs;                   // DEPRECATED
    HardwareThread* hw_threads;  // NUEVO: Array de hardware threads
} Core;
```

**Inicialización**: En `create_core()`, cada hardware thread se inicializa con:
- PC, IR, PTBR = 0/NULL
- MMU deshabilitada
- TLB con todas las entradas inválidas
- PCB = NULL

## 4. Loader (Transformación del Process Generator)

### Archivos: `loader.h` y `loader.c`

El antiguo Process Generator se ha transformado en un **Loader** que carga programas desde archivos.

#### Estructura del Loader:
```c
typedef struct {
    PhysicalMemory* physical_memory;
    ProcessQueue* ready_queue;
    Machine* machine;
    Scheduler* scheduler;
    volatile int next_pid;
    volatile int total_loaded;
} Loader;
```

#### Formato de Archivo de Programa:
Los programas se almacenan en archivos de texto con el siguiente formato:

```
PROGRAM <nombre>
CODE_SIZE <tamaño_en_palabras>
DATA_SIZE <tamaño_en_palabras>
ENTRY_POINT <dirección>
PRIORITY <prioridad>
TTL <tiempo_de_vida>
CODE_SECTION
<palabra_hex>
<palabra_hex>
...
DATA_SECTION
<palabra_hex>
<palabra_hex>
...
```

#### Funciones Principales:

1. **`load_program_from_file(filename)`**
   - Lee un programa desde un archivo de texto
   - Parsea el header y los segmentos
   - Retorna estructura `Program`

2. **`create_process_from_program(loader, program)`**
   - Crea un PCB
   - Calcula páginas necesarias
   - Crea tabla de páginas en espacio del kernel
   - Asigna marcos físicos
   - Carga código y datos en memoria física
   - Inicializa campos mm del PCB:
     - `code`: Dirección virtual 0
     - `data`: Después del código
     - `pgb`: Puntero a tabla de páginas

3. **`load_and_create_process(loader, filename)`**
   - Función de conveniencia que combina las dos anteriores

#### Ejemplo de Carga:

Para un programa con 8 palabras de código y 4 de datos:
1. Se calculan las páginas necesarias (1 página de código, 1 de datos)
2. Se crea tabla de páginas de 2 entradas en espacio del kernel
3. Se asignan 2 marcos físicos de usuario
4. Se copian los datos del programa a los marcos
5. Se actualizan las entradas de la tabla de páginas:
   - Página 0: marco X, presente=1, rw=0 (código read-only)
   - Página 1: marco Y, presente=1, rw=1 (datos read-write)
6. PCB.mm se configura:
   - code = 0x00000000
   - data = 0x00001000 (4KB después)
   - pgb = dirección de la tabla de páginas

## 5. Programas de Ejemplo

Se han creado tres programas de ejemplo en `sys/programs/`:

1. **simple_add.txt**: Programa simple (8 palabras código, 4 datos)
2. **high_priority.txt**: Programa con prioridad alta (16 palabras código, 8 datos)
3. **loop_test.txt**: Programa de prueba (12 palabras código, 6 datos)

## 6. Compilación

El Makefile ha sido actualizado para incluir los nuevos módulos:

```bash
make clean
make
```

Esto compilará:
- kernel.o
- machine.o (actualizado)
- process.o (actualizado)
- clock_sys.o
- timer.o
- memory.o (NUEVO)
- loader.o (NUEVO)

## Próximos Pasos (Fase 2)

En la siguiente fase se implementará:

1. **Traducción de direcciones**: MMU funcional
2. **Gestión de TLB**: Búsqueda y actualización
3. **Ejecución de instrucciones**: Fetch-Decode-Execute con traducción
4. **Page faults**: Manejo de páginas no presentes
5. **Context switching**: Actualización de PTBR al cambiar procesos

## Notas Importantes

- **NO** se ha implementado la lógica de ejecución todavía
- **NO** se ha implementado la traducción de direcciones
- Las estructuras están preparadas pero no se usan activamente
- El código legacy del PCB array se mantiene por compatibilidad
- Los Hardware Threads están inicializados pero no ejecutan procesos aún

## Verificación

Para verificar que todo está correcto:

1. Compilar el proyecto: `make clean && make`
2. Verificar que no hay errores de compilación
3. Las estructuras de memoria están listas para usar
4. Los archivos de programa pueden ser leídos (no ejecutados aún)

## Arquitectura General

```
┌─────────────────────────────────────────────────────┐
│              Physical Memory (16 MB)                 │
├───────────────────────────┬─────────────────────────┤
│   Kernel Space (1 MB)     │   User Space (15 MB)    │
│   - Page Tables           │   - Code Segments       │
│   - Kernel Data           │   - Data Segments       │
└───────────────────────────┴─────────────────────────┘

┌─────────────────────────────────────────────────────┐
│                      Machine                         │
│  ┌─────────────────────────────────────────────┐    │
│  │                    CPU                       │    │
│  │  ┌──────────────────────────────────────┐   │    │
│  │  │              Core                     │   │    │
│  │  │  ┌────────────────────────────────┐  │   │    │
│  │  │  │      Hardware Thread           │  │   │    │
│  │  │  │  - PC, IR, PTBR                │  │   │    │
│  │  │  │  - MMU (page_table_base)       │  │   │    │
│  │  │  │  - TLB (16 entries)            │  │   │    │
│  │  │  │  - PCB pointer                 │  │   │    │
│  │  │  └────────────────────────────────┘  │   │    │
│  │  └──────────────────────────────────────┘   │    │
│  └─────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│                        PCB                           │
│  - pid, state, priority, ttl                        │
│  - MemoryManagement mm:                             │
│    - void* code  (virtual address)                  │
│    - void* data  (virtual address)                  │
│    - void* pgb   (physical address of page table)   │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│                      Loader                          │
│  - Reads programs from files                        │
│  - Creates PCB                                      │
│  - Allocates page table in kernel space             │
│  - Allocates frames for code/data                   │
│  - Loads segments into physical memory              │
│  - Initializes PCB.mm                               │
└─────────────────────────────────────────────────────┘
```
