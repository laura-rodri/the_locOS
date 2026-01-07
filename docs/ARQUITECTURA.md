# Arquitectura del Sistema LocOS

## Descripción General

the_locOS es un sistema operativo educativo que simula un kernel con gestión de procesos, planificación, memoria virtual y hardware virtualizado. El sistema soporta múltiples CPUs, cores, y hardware threads, con políticas de planificación configurables.

## Jerarquía del Sistema

```
Kernel
├── Reloj del Sistema (Clock)
│   └── Timers de Interrupción
├── Gestión de Procesos (ProcessGenerator + Scheduler)
│   ├── Process Control Blocks (PCBs)
│   ├── Colas de Procesos (ProcessQueue)
│   └── Scheduler (RR, BFS, o Preemptivo con Prioridades)
├── Memoria Física (PhysicalMemory)
│   ├── Espacio del Kernel (tablas de páginas)
│   └── Espacio de Usuario (marcos de página)
└── Máquina Virtual (Machine)
    └── CPUs[]
        └── Cores[]
            └── HardwareThreads[]
                ├── Registros (PC, IR, PTBR, r0-r15)
                ├── MMU (Memory Management Unit)
                └── TLB (Translation Lookaside Buffer)
```

## Componentes Principales

### 1. Process Control Block (PCB)

Estructura que representa un proceso en el sistema:

```c
typedef struct {
    int pid;                     // Identificador único
    int state;                   // RUNNING, WAITING, TERMINATED
    int priority;                // -20 (máxima) a 19 (mínima)
    int ttl;                     // Time to live actual
    int initial_ttl;             // TTL inicial
    int quantum_counter;         // Contador de quantum usado
    int virtual_deadline;        // Para BFS scheduling
    MemoryManagement mm;         // Gestión de memoria
    ExecutionContext context;    // Contexto de ejecución
} PCB;
```

**MemoryManagement**: Contiene `code`, `data` y `pgb` (page table base).  
**ExecutionContext**: Incluye PC, IR y registros para guardar el estado del proceso.

### 2. Hardware Thread

Representa el contexto de ejecución en hardware:

```c
typedef struct {
    uint32_t PC;                 // Program Counter
    uint32_t IR;                 // Instruction Register
    void* PTBR;                  // Page Table Base Register
    uint32_t registers[16];      // Registros r0-r15
    MMU mmu;                     // Unidad de gestión de memoria
    TLB tlb;                     // Translation Lookaside Buffer
    PCB* pcb;                    // PCB actualmente ejecutando
} HardwareThread;
```

### 3. Máquina Virtual (Machine → CPU → Core)

- **Machine**: Contiene múltiples CPUs
- **CPU**: Contiene múltiples Cores
- **Core**: Contiene múltiples HardwareThreads

Cada HardwareThread puede ejecutar un PCB. El scheduler asigna PCBs a HardwareThreads disponibles.

### 4. Memoria Física

```c
typedef struct {
    uint32_t* memory;                    // Array de palabras (4 bytes)
    uint8_t* frame_bitmap;               // Bitmap de marcos libres/ocupados
    uint32_t kernel_space_end;           // Fin del espacio del kernel
    uint32_t user_space_start;           // Inicio del espacio de usuario
    uint32_t next_kernel_frame;          // Próximo marco para kernel
    uint32_t total_allocated_frames;     // Total de marcos asignados
} PhysicalMemory;
```

**Configuración**:
- Bus de direcciones: 24 bits (16 MB)
- Tamaño de palabra: 4 bytes
- Tamaño de página: 4 KB
- Total de marcos: 4096
- Espacio kernel: 1 MB (256 marcos)
- Espacio usuario: 15 MB (3840 marcos)

### 5. System Clock

Genera ticks periódicos que sincronizan todos los componentes del sistema:

- Frecuencia configurable (Hz)
- Notifica mediante broadcast a todos los componentes
- **Decrementa el TTL** de procesos en ejecución
- **Ejecuta un ciclo de instrucción** (fetch-decode-execute) por cada proceso en ejecución en cada tick
- Mantiene referencia a la Machine para acceder a procesos en ejecución
- Mantiene referencia a la PhysicalMemory para ejecutar instrucciones

### 6. Timers

Generan interrupciones cada N ticks del reloj del sistema:

- Intervalo configurable
- Sistema de callbacks para ejecutar acciones al interrumpir
- Usados para sincronización del scheduler

## Flujo de Ejecución

### 1. Inicialización del Sistema

```
1. Crear memoria física (create_physical_memory)
2. Crear máquina virtual (create_machine)
3. Crear reloj del sistema (create_system_clock)
4. Crear timers si es necesario
5. Crear generador de procesos (create_process_generator)
6. Crear scheduler (create_scheduler)
7. Iniciar reloj del sistema
8. Iniciar generador de procesos
9. Iniciar scheduler
```

### 2. Generación de Procesos

```
ProcessGenerator → cada X ticks → Nuevo PCB
    ├── Asigna PID único
    ├── Asigna prioridad aleatoria (-20 a 19)
    ├── Asigna TTL aleatorio
    ├── Carga programa desde archivo (Loader)
    │   ├── Crea tabla de páginas
    │   ├── Asigna marcos físicos
    │   └── Carga código en memoria
    └── Añade a ready_queue o priority_queues
```

### 3. Planificación (Scheduler)

El scheduler se ejecuta periódicamente según su modo de sincronización:

```
Scheduler despierta
    ├── Selecciona siguiente PCB según política
    │   ├── Round Robin: primer proceso de la cola
    │   ├── BFS: proceso con menor virtual_deadline
    │   └── Preemptive: proceso de máxima prioridad
    ├── Busca HardwareThread libre
    ├── Asigna PCB al HardwareThread
    │   ├── Copia contexto a HardwareThread
    │   ├── Configura PTBR con tabla de páginas
    │   └── Marca proceso como RUNNING
    └── Si quantum expira o proceso termina
        ├── Guarda contexto en PCB
        ├── Libera HardwareThread
        └── Reencola o termina proceso
```

### 4. Ejecución de Procesos

```
HardwareThread ejecutando PCB (cada tick del SystemClock):
    ├── SystemClock decrementa TTL
    ├── SystemClock ejecuta un ciclo de instrucción (fetch-decode-execute)
    │   ├── Fetch: leer instrucción de memoria usando PC
    │   ├── Decode: decodificar la instrucción
    │   ├── Execute: ejecutar la instrucción
    │   └── Actualizar PC y registros
    ├── Scheduler incrementa quantum_counter cada vez que se despierta
    └── Si TTL llega a 0 o instrucción EXIT:
        ├── Marca proceso como TERMINATED
        └── Scheduler libera recursos de memoria
```

## Parámetros de Configuración

El sistema se configura mediante línea de comandos:

```bash
./kernel -f <hz> -q <ticks> -policy <num> -sync <mode>
```

**Parámetros**:
- `-f <hz>`: Frecuencia del reloj en Hz (default: 1)
- `-q <ticks>`: Quantum del scheduler (default: 3)
- `-policy <num>`: Política de planificación
  - 0: Round Robin (default)
  - 1: Brain Fuck Scheduler (BFS)
  - 2: Preemptiva con prioridades
- `-sync <mode>`: Modo de sincronización
  - 0: Sincronización con reloj global (default)
  - 1: Sincronización con timer dedicado

## Archivos del Sistema

```
sys/
├── kernel.c         → Main del kernel, integración de componentes
├── process.h/c      → PCB, colas, generador de procesos
├── machine.h/c      → Machine, CPU, Core, HardwareThread
├── memory.h/c       → Gestión de memoria física y virtual
├── loader.h/c       → Cargador de programas
├── clock_sys.h/c    → Reloj del sistema
├── timer.h/c        → Timers de interrupción
└── Makefile         → Compilación
```

## Compilación y Ejecución

### Compilar

```bash
cd sys
make clean
make
```

### Ejecutar

```bash
# Configuración por defecto
./kernel

# Ejemplo: 10 Hz, quantum 3, BFS, timer sync
./kernel -f 10 -q 3 -policy 1 -sync 1

# Ejemplo: 5 Hz, quantum 2, prioridades preemptivas
./kernel -f 5 -q 2 -policy 2 -sync 0
```

## Estados del Sistema

### Estados de Proceso
- **WAITING**: En cola de listos
- **RUNNING**: Ejecutándose en un HardwareThread
- **TERMINATED**: Finalizado (TTL = 0)

### Sincronización
El sistema usa mutexes y variables de condición para sincronizar:
- SystemClock notifica cada tick
- Timers se sincronizan con el reloj
- Scheduler se sincroniza con reloj o timer
- ProcessGenerator se sincroniza con el reloj
