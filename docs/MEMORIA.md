# Gestión de Memoria Virtual

## Descripción General

El sistema the_locOS implementa un gestor de memoria virtual con paginación, permitiendo a los procesos acceder a un espacio de direcciones virtuales que se traduce a memoria física mediante tablas de páginas y una MMU (Memory Management Unit).

## Arquitectura de Memoria

### Configuración Física

```c
// Bus de direcciones de 24 bits
#define ADDRESS_BUS_BITS 24
#define PHYSICAL_MEMORY_SIZE (1 << 24)  // 16,777,216 bytes (16 MB)

// Palabras de 4 bytes
#define WORD_SIZE 4
#define TOTAL_WORDS 4,194,304  // 16 MB / 4 bytes

// Páginas de 4 KB
#define PAGE_SIZE 4096
#define PAGE_OFFSET_BITS 12  // log2(4096)
#define TOTAL_FRAMES 4096    // 16 MB / 4 KB

// Distribución de memoria
Kernel Space:  1 MB (256 marcos) - Para tablas de páginas
User Space:   15 MB (3840 marcos) - Para procesos de usuario
```

### Estructura de Direcciones Virtuales

```
Virtual Address (32 bits):
┌─────────────────────┬──────────────┐
│  Page Number (20)   │  Offset (12) │
└─────────────────────┴──────────────┘
 Bits 31-12              Bits 11-0
```

- **Page Number**: Índice en la tabla de páginas
- **Offset**: Desplazamiento dentro de la página (0-4095)

### Estructura de Direcciones Físicas

```
Physical Address (24 bits):
┌─────────────────────┬──────────────┐
│  Frame Number (12)  │  Offset (12) │
└─────────────────────┴──────────────┘
 Bits 23-12              Bits 11-0
```

## Estructuras de Datos

### PhysicalMemory

```c
typedef struct {
    uint32_t* memory;                    // Array de palabras (4 bytes cada una)
    uint8_t* frame_bitmap;               // Bitmap: 1=ocupado, 0=libre
    uint32_t kernel_space_end;           // Fin del espacio kernel (en palabras)
    uint32_t user_space_start;           // Inicio del espacio usuario (en palabras)
    uint32_t next_kernel_frame;          // Siguiente marco libre en kernel space
    uint32_t total_allocated_frames;     // Total de marcos asignados
} PhysicalMemory;
```

**Funciones principales**:
- `create_physical_memory()`: Inicializa la memoria física
- `allocate_frame()`: Asigna un marco libre del user space
- `free_frame()`: Libera un marco
- `allocate_kernel_space()`: Reserva espacio en kernel space

### PageTableEntry

```c
typedef struct {
    uint32_t frame_number : 12;  // Número de marco físico (0-4095)
    uint32_t present : 1;        // 1=en memoria, 0=no presente
    uint32_t rw : 1;             // 1=escritura, 0=solo lectura
    uint32_t user : 1;           // 1=usuario, 0=supervisor
    uint32_t accessed : 1;       // Bit de acceso
    uint32_t dirty : 1;          // Bit de modificación
    uint32_t reserved : 15;      // Reservado para uso futuro
} PageTableEntry;
```

**Interpretación**:
- Si `present = 0`: página no está en memoria (page fault)
- Si `present = 1`: `frame_number` indica el marco físico

### MemoryManagement (en PCB)

```c
typedef struct {
    void* code;   // Dirección virtual del inicio del segmento de código
    void* data;   // Dirección virtual del inicio del segmento de datos
    void* pgb;    // Dirección física de la base de la tabla de páginas (PTBR)
} MemoryManagement;
```

Este campo está incluido en cada PCB y mantiene la información de memoria del proceso.

## Translation Lookaside Buffer (TLB)

### Estructura

```c
typedef struct {
    uint32_t virtual_page;   // Número de página virtual
    uint32_t physical_frame; // Número de marco físico
    uint8_t valid;           // 1=entrada válida, 0=inválida
} TLBEntry;

typedef struct {
    TLBEntry entries[TLB_SIZE];  // TLB_SIZE = 16
    int next_replace;            // Índice para reemplazo round-robin
} TLB;
```

### Funcionamiento

El TLB es una **caché de traducciones** que acelera el acceso a memoria:

```
1. Buscar traducción en TLB (virtual_page → physical_frame)
2. Si TLB hit:
   - Usar physical_frame directamente
   - Evitar acceso a tabla de páginas
3. Si TLB miss:
   - Consultar tabla de páginas
   - Actualizar TLB con nueva traducción
   - Usar reemplazo round-robin
```

**Ventajas**: Reduce accesos a memoria (tabla de páginas está en memoria).

## Memory Management Unit (MMU)

### Estructura

```c
typedef struct {
    void* page_table_base;  // Puntero a la tabla de páginas (PTBR)
    int enabled;            // 1=MMU habilitada, 0=deshabilitada
} MMU;
```

### Traducción de Direcciones

```c
uint32_t translate_virtual_to_physical(
    PhysicalMemory* pm,
    PageTableEntry* page_table,
    uint32_t virtual_address
)
```

**Proceso**:
```
1. Extraer page_number = virtual_address >> 12
2. Extraer offset = virtual_address & 0xFFF
3. Consultar page_table[page_number]
4. Verificar present bit
5. Si present:
   - frame_number = page_table[page_number].frame_number
   - physical_address = (frame_number << 12) | offset
6. Si no present:
   - Page fault (no implementado aún)
```

### Lectura/Escritura con MMU

```c
// Leer palabra desde dirección virtual
uint32_t mmu_read_word(PhysicalMemory* pm, PageTableEntry* page_table, 
                       uint32_t virtual_address);

// Escribir palabra en dirección virtual
void mmu_write_word(PhysicalMemory* pm, PageTableEntry* page_table, 
                    uint32_t virtual_address, uint32_t value);
```

## Loader - Cargador de Programas

### Formato de Programa

Los programas se cargan desde archivos `.elf` (formato prometheus) con el siguiente formato:

```
.text <dirección_hex>   # Inicio del segmento de código
.data <dirección_hex>   # Inicio del segmento de datos
<instrucción_hex>       # Instrucciones/datos (8 dígitos hex por línea)
...
```

**Ejemplo** (`programs/prog000.elf`):
```
.text 000000
.data 000014
0A00002C
0B000030
2CAB0000
1C000014
F0000000
...
```

**Nota**: TTL y prioridad se calculan automáticamente basándose en el tamaño del programa.

### Estructura del Loader

```c
typedef struct {
    PhysicalMemory* physical_memory;  // Referencia a memoria física
} Loader;
```

### Proceso de Carga

```c
int load_program(Loader* loader, const char* filename, PCB* pcb);
```

**Pasos**:
```
1. Abrir y parsear archivo .elf del programa
2. Leer directivas .text y .data para obtener direcciones virtuales
3. Leer instrucciones en formato hexadecimal (8 dígitos por línea)
4. Calcular TTL y prioridad basándose en el tamaño del programa
5. Calcular páginas necesarias para el programa completo
6. Crear tabla de páginas para el proceso
7. Asignar marcos físicos para cada página
8. Copiar código y datos a memoria física usando las direcciones virtuales
9. Configurar PCB:
   - pcb->mm.code = dirección virtual del código (.text)
   - pcb->mm.data = dirección virtual de los datos (.data)
   - pcb->mm.pgb = dirección física de la tabla de páginas
   - pcb->ttl = calculado automáticamente
   - pcb->priority = calculado automáticamente
10. Inicializar contexto de ejecución (PC = entry_point)
```

### Creación de Tabla de Páginas

```c
PageTableEntry* create_page_table(PhysicalMemory* pm, uint32_t num_pages);
```

**Proceso**:
```
1. Asignar espacio en kernel space para la tabla
2. Para cada página (0 a num_pages-1):
   - Asignar un marco físico del user space
   - Crear entrada en la tabla:
     - frame_number = marco asignado
     - present = 1
     - rw = 1 (lectura/escritura)
     - user = 1 (modo usuario)
3. Retornar puntero a la tabla
```

## HardwareThread con MMU/TLB

### Estructura

```c
typedef struct {
    uint32_t PC;            // Program Counter
    uint32_t IR;            // Instruction Register
    void* PTBR;             // Page Table Base Register
    uint32_t registers[16]; // r0-r15
    MMU mmu;                // Memory Management Unit
    TLB tlb;                // Translation Lookaside Buffer
    PCB* pcb;               // Proceso actual
} HardwareThread;
```

### Configuración al Asignar Proceso

```c
// Cuando el scheduler asigna un PCB a un HardwareThread
hw_thread->PTBR = pcb->mm.pgb;           // Cargar tabla de páginas
hw_thread->PC = pcb->context.pc;         // Restaurar PC
hw_thread->mmu.page_table_base = pcb->mm.pgb;
hw_thread->mmu.enabled = 1;              // Habilitar MMU

// Invalidar TLB (cambio de contexto)
for (int i = 0; i < TLB_SIZE; i++) {
    hw_thread->tlb.entries[i].valid = 0;
}
```

## Flujo Completo de Ejecución

### 1. Inicio del Sistema

```
kernel.c: main()
  ├── create_physical_memory()
  ├── create_loader(physical_memory)
  ├── create_machine(num_cpus, num_cores, num_threads)
  └── create_process_generator(loader, ...)
```

### 2. Generación de Proceso

```
ProcessGenerator:
  ├── Crear PCB vacío
  ├── Llamar a load_program(loader, filename, pcb)
  │   ├── Parser archivo
  │   ├── Crear tabla de páginas
  │   ├── Asignar marcos físicos
  │   ├── Cargar código y datos en memoria
  │   └── Configurar pcb->mm
  └── Añadir PCB a ready_queue
```

### 3. Ejecución de Proceso

```
Scheduler:
  ├── Seleccionar PCB de ready_queue
  ├── Buscar HardwareThread libre
  ├── Asignar PCB a HardwareThread
  │   ├── hw_thread->PTBR = pcb->mm.pgb
  │   ├── hw_thread->PC = 0 (inicio del código)
  │   ├── hw_thread->mmu.enabled = 1
  │   └── Invalidar TLB
  └── Marcar pcb->state = RUNNING
```

### 4. Acceso a Memoria durante Ejecución

```
HardwareThread ejecutando:
  ├── Fetch instrucción en PC
  │   ├── Traducir PC (virtual) → física
  │   │   ├── Buscar en TLB
  │   │   ├── Si TLB miss: consultar page_table
  │   │   └── Actualizar TLB
  │   └── Leer instrucción de memoria física
  ├── Ejecutar instrucción
  └── Acceso a datos
      ├── Traducir dirección virtual → física
      └── Leer/escribir en memoria física
```

### 5. Finalización de Proceso

```
Cuando TTL = 0:
  ├── Marcar pcb->state = TERMINATED
  ├── Guardar contexto en PCB
  ├── Liberar HardwareThread (pcb = NULL)
  └── Destruir tabla de páginas
      ├── Liberar cada marco físico
      └── Liberar espacio de la tabla en kernel space
```

## Funciones de la API

### Gestión de Memoria Física

```c
PhysicalMemory* create_physical_memory();
void destroy_physical_memory(PhysicalMemory* pm);
uint32_t allocate_frame(PhysicalMemory* pm);
void free_frame(PhysicalMemory* pm, uint32_t frame_number);
void* allocate_kernel_space(PhysicalMemory* pm, uint32_t size_in_words);
```

### Acceso a Memoria

```c
uint32_t read_word(PhysicalMemory* pm, uint32_t address);
void write_word(PhysicalMemory* pm, uint32_t address, uint32_t value);
```

### Gestión de Tablas de Páginas

```c
PageTableEntry* create_page_table(PhysicalMemory* pm, uint32_t num_pages);
void destroy_page_table(PhysicalMemory* pm, PageTableEntry* page_table, 
                        uint32_t num_pages);
```

### Traducción de Direcciones (MMU)

```c
uint32_t translate_virtual_to_physical(PhysicalMemory* pm, 
                                       PageTableEntry* page_table, 
                                       uint32_t virtual_address);
uint32_t mmu_read_word(PhysicalMemory* pm, PageTableEntry* page_table, 
                       uint32_t virtual_address);
void mmu_write_word(PhysicalMemory* pm, PageTableEntry* page_table, 
                    uint32_t virtual_address, uint32_t value);
```

### Loader

```c
Loader* create_loader(PhysicalMemory* pm);
void destroy_loader(Loader* loader);
int load_program(Loader* loader, const char* filename, PCB* pcb);
```

## Ejemplo de Uso

### Crear Sistema con Memoria

```c
// En kernel.c
PhysicalMemory* pm = create_physical_memory();
Loader* loader = create_loader(pm);
Machine* machine = create_machine(2, 2, 4);  // 2 CPUs, 2 cores, 4 threads cada uno

// Crear generador con loader
ProcessGenerator* gen = create_process_generator(
    5, 10,           // intervalo entre procesos
    30, 100,         // TTL range
    ready_queue, 
    machine, 
    scheduler, 
    20,              // max procesos
    loader,          // loader para cargar programas
    "programs"       // directorio de programas
);
```

### Cargar Programa Manualmente

```c
// El loader carga programas .elf automáticamente
// desde el directorio de programas y crea PCBs
Program* prog = load_program_from_elf("programs/prog000.elf");
if (prog) {
    PCB* pcb = create_process_from_program(loader, prog);
    if (pcb) {
        printf("Program loaded successfully\n");
        printf("  Code at: %p\n", pcb->mm.code);
        printf("  Data at: %p\n", pcb->mm.data);
        printf("  Page table at: %p\n", pcb->mm.pgb);
        printf("  TTL: %d\n", pcb->ttl);
        printf("  Priority: %d\n", pcb->priority);
    }
    destroy_program(prog);
}
```

## Estado Actual y Próximos Pasos

### Implementado ✅

- Memoria física de 16 MB
- Paginación con páginas de 4 KB
- Tablas de páginas
- Traducción de direcciones virtuales a físicas
- MMU con traducción
- TLB básico (16 entradas, round-robin)
- Loader de programas desde archivos
- Integración con PCB y HardwareThread

### Pendiente ⏳

- **Page faults**: manejo de páginas no presentes
- **Swap**: intercambio de páginas a disco
- **Copy-on-write**: optimización de fork
- **Shared memory**: memoria compartida entre procesos
- **Demand paging**: cargar páginas solo cuando se acceden
- **LRU/FIFO**: algoritmos de reemplazo de páginas
- **Protección**: permisos de lectura/escritura/ejecución
