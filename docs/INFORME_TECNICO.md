# Informe Técnico: Diseño e Implementación del Sistema LocOS

**Proyecto**: Sistema Operativo Educativo LocOS  
**Rama**: p3_memoria  
**Fecha**: 6 de enero de 2026  
**Autora**: Laura Rodríguez

---

## Tabla de Contenidos

1. [Introducción](#introducción)
2. [Arquitectura del Sistema](#arquitectura-del-sistema)
3. [Planificador de Procesos](#planificador-de-procesos)
4. [Gestor de Memoria Virtual](#gestor-de-memoria-virtual)
5. [Integración de Componentes](#integración-de-componentes)
6. [Conclusiones](#conclusiones)

---

## Introducción

### Objetivo del Proyecto

the_locOS es un sistema operativo educativo diseñado para simular los componentes fundamentales de un kernel moderno, incluyendo la gestión de procesos, la planificación con múltiples políticas, y la memoria virtual con paginación. El sistema permite experimentar con diferentes configuraciones y observar el comportamiento de distintas estrategias de scheduling.

### Alcance de la Implementación

La implementación actual incluye:
- Hardware virtual con arquitectura Machine → CPU → Core → HardwareThread
- Sistema de reloj global sincronizado con timers configurables
- Tres políticas de planificación (Round Robin, BFS, Preemptive Priority)
- Gestor de memoria virtual con paginación de 4 KB
- Cargador de programas en formato ELF (prometheus)
- MMU y TLB para traducción de direcciones

---

## Arquitectura del Sistema

### 1. Diseño General

#### 1.1 Filosofía de Diseño

El sistema sigue una arquitectura **modular** donde cada componente tiene responsabilidades claramente definidas:

- **Separación de concerns**: Hardware virtual, gestión de procesos, memoria y planificación están en módulos independientes
- **Sincronización basada en eventos**: Uso de mutexes y condition variables para coordinación entre threads
- **Simulación realista**: El hardware virtual simula CPUs, cores y threads con registros y contexto de ejecución

#### 1.2 Jerarquía de Hardware Virtual

```
Machine (machine.h/c)
  ├── CPUs[] (array de CPUs)
  │   └── Cores[] (array de cores por CPU)
  │       └── HardwareThreads[] (array de threads por core)
  │           ├── Registros: PC, IR, PTBR, r0-r15
  │           ├── MMU (Memory Management Unit)
  │           ├── TLB (Translation Lookaside Buffer)
  │           └── PCB* (proceso actualmente ejecutando)
```

**Decisión de diseño**: Se optó por una jerarquía de tres niveles (Machine-CPU-Core) para simular arquitecturas multi-core reales. Cada HardwareThread es una unidad de ejecución independiente que puede ejecutar un PCB.

#### 1.3 Implementación de HardwareThread

```c
typedef struct {
    uint32_t PC;            // Program Counter
    uint32_t IR;            // Instruction Register
    void* PTBR;             // Page Table Base Register
    uint32_t registers[16]; // Registros r0-r15
    MMU mmu;                // Unidad de gestión de memoria
    TLB tlb;                // Translation Lookaside Buffer
    PCB* pcb;               // Proceso actual
} HardwareThread;
```

**Características**:
- **Contexto completo**: Mantiene todos los registros necesarios para la ejecución
- **MMU integrada**: Cada thread tiene su propia MMU con tabla de páginas (PTBR)
- **TLB privado**: Cache de traducciones de 16 entradas con reemplazo round-robin
- **Referencia al PCB**: Permite acceso bidireccional entre hardware y proceso

### 2. Process Control Block (PCB)

#### 2.1 Diseño del PCB

```c
typedef struct {
    int pid;                     // Identificador único
    int state;                   // RUNNING, WAITING, TERMINATED
    int priority;                // -20 (máxima) a 19 (mínima)
    int ttl;                     // Time to live actual
    int initial_ttl;             // TTL inicial (para reset si necesario)
    int quantum_counter;         // Uso actual del quantum
    int virtual_deadline;        // Para BFS scheduling
    MemoryManagement mm;         // Información de memoria
    ExecutionContext context;    // Contexto guardado
} PCB;
```

**Decisiones de diseño**:

1. **TTL dual** (ttl + initial_ttl): Permite reset de procesos si fuera necesario en futuras extensiones
2. **quantum_counter**: Contador independiente del TTL para gestión de quantum
3. **virtual_deadline**: Campo específico para BFS, evitando cálculos repetitivos
4. **Estado explícito**: Simplifica la lógica del scheduler

#### 2.2 Gestión de Memoria en PCB

```c
typedef struct {
    void* code;   // Dirección virtual del código
    void* data;   // Dirección virtual de los datos
    void* pgb;    // Dirección física de la tabla de páginas
} MemoryManagement;
```

**Implementación**:
- `code` y `data` son direcciones **virtuales** desde la perspectiva del proceso
- `pgb` (Page Global Base) es la dirección **física** de la tabla de páginas
- Esta separación permite al scheduler configurar el PTBR directamente con `pgb`

#### 2.3 Contexto de Ejecución

```c
typedef struct {
    uint32_t pc;            // Program Counter guardado
    uint32_t instruction;   // Última instrucción
    uint32_t registers[16]; // Estado de registros
} ExecutionContext;
```

**Propósito**: Permite cambios de contexto completos al expulsar un proceso. Al reanudar, se restauran todos los registros desde el contexto guardado.

### 3. Reloj del Sistema (SystemClock)

#### 3.1 Diseño del Reloj

El reloj del sistema es el **coordinador temporal** de todo el sistema. Su implementación se basa en:

```c
void* clock_function(void* arg) {
    while (running) {
        usleep(1000000 / CLOCK_FREQUENCY_HZ);  // Esperar un período
        
        pthread_mutex_lock(&clk_mutex);
        clk_counter++;
        
        // 1. Decrementar TTL de procesos en ejecución
        for (cada HardwareThread con PCB):
            decrement_pcb_ttl(pcb);
        
        // 2. Ejecutar un ciclo de instrucción por proceso
        for (cada HardwareThread con PCB):
            execute_instruction_cycle(hw_thread, pm);
        
        // 3. Notificar a todos los componentes sincronizados
        pthread_cond_broadcast(&clk_cond);
        pthread_mutex_unlock(&clk_mutex);
    }
}
```

**Decisiones de implementación**:

1. **Frecuencia configurable**: Permite simular desde sistemas lentos (1 Hz) hasta rápidos (100+ Hz)
2. **Broadcast de notificaciones**: Todos los componentes (scheduler, generator, timers) se despiertan en cada tick
3. **Ejecución de instrucciones**: El reloj ejecuta el ciclo fetch-decode-execute, simulando el comportamiento real de un CPU
4. **Referencias globales**: `clock_machine_ref` y `clock_pm_ref` permiten acceso directo a Machine y PhysicalMemory

#### 3.2 Gestión de TTL

**Implementación**:
```c
int decrement_pcb_ttl(PCB* pcb) {
    if (pcb && pcb->ttl > 0) {
        pcb->ttl--;
    }
    return pcb->ttl;
}
```

**Características**:
- TTL nunca se decrementa por debajo de 0
- Retorna el nuevo valor para logging
- Thread-safe (llamado bajo clk_mutex)

### 4. Timers

#### 4.1 Diseño de Timers

Los timers son **generadores de interrupciones periódicas** que se sincronizan con el reloj:

```c
typedef struct {
    int id;
    int interval;                    // Ticks entre interrupciones
    volatile int counter;            // Contador de ticks
    timer_callback_t callback;       // Función a ejecutar
    void* callback_data;             // Datos para callback
} Timer;
```

**Funcionamiento**:
```c
void* timer_function(void* arg) {
    while (timer->running) {
        pthread_mutex_lock(&clk_mutex);
        while (running && timer->counter < last_tick) {
            pthread_cond_wait(&clk_cond, &clk_mutex);
        }
        
        if (clk_counter - last_tick >= timer->interval) {
            timer->callback(timer->id, timer->callback_data);
            last_tick = clk_counter;
        }
        pthread_mutex_unlock(&clk_mutex);
    }
}
```

**Aplicación**: En modo SCHED_SYNC_TIMER, el scheduler se sincroniza con un timer que ejecuta un callback para despertar al scheduler.

### 5. Colas de Procesos

#### 5.1 Implementación de ProcessQueue

```c
typedef struct {
    PCB** queue;         // Array dinámico de punteros a PCB
    int front;           // Índice del frente
    int rear;            // Índice del final
    int max_capacity;    // Capacidad máxima
    int current_size;    // Tamaño actual
} ProcessQueue;
```

**Estructura**: Cola circular implementada con array para O(1) en enqueue/dequeue.

**Operaciones**:
```c
int enqueue_process(ProcessQueue* pq, PCB* pcb) {
    if (pq->current_size >= pq->max_capacity) return -1;
    pq->rear = (pq->rear + 1) % pq->max_capacity;
    pq->queue[pq->rear] = pcb;
    pq->current_size++;
    return 0;
}

PCB* dequeue_process(ProcessQueue* pq) {
    if (pq->current_size == 0) return NULL;
    PCB* pcb = pq->queue[pq->front];
    pq->front = (pq->front + 1) % pq->max_capacity;
    pq->current_size--;
    return pcb;
}
```

**Ventaja**: Operaciones en tiempo constante sin necesidad de desplazar elementos.

---

## Planificador de Procesos

### 1. Arquitectura del Scheduler

#### 1.1 Estructura Principal

```c
typedef struct Scheduler {
    int quantum;                     // Quantum en ticks
    int policy;                      // Política activa
    int sync_mode;                   // CLOCK o TIMER
    void* sync_source;               // Timer si TIMER mode
    ProcessQueue* ready_queue;       // Cola única (RR/BFS)
    ProcessQueue** priority_queues;  // 40 colas (Preemptive)
    Machine* machine;                // Hardware virtual
    pthread_t thread;                // Thread del scheduler
    volatile int running;            // Flag de control
    volatile int total_completed;    // Estadística
    pthread_mutex_t sched_mutex;     // Para activación
    pthread_cond_t sched_cond;       // Sincronización
} Scheduler;
```

#### 1.2 Modos de Sincronización

**SCHED_SYNC_CLOCK (modo 0)**:
```c
// Scheduler se despierta en cada tick del reloj
while (running && clk_counter == last_tick) {
    pthread_cond_wait(&clk_cond, &clk_mutex);
}
```

**Características**:
- Activación en cada tick del sistema
- Verifica quantum de todos los procesos cada tick
- Mayor overhead pero más responsive
- Quantum se cuenta en ticks individuales

**SCHED_SYNC_TIMER (modo 1)**:
```c
// Timer callback despierta al scheduler
void timer_scheduler_callback(int timer_id, void* user_data) {
    Scheduler* sched = (Scheduler*)user_data;
    pthread_mutex_lock(&sched->sched_mutex);
    pthread_cond_signal(&sched->sched_cond);
    pthread_mutex_unlock(&sched->sched_mutex);
}
```

**Características**:
- Activación periódica basada en timer
- Menor overhead (menos activaciones)
- Quantum expira cuando counter >= 1 (al recibir interrupción del timer)
- Útil para reducir cambios de contexto frecuentes

### 2. Políticas de Planificación

#### 2.1 Round Robin (SCHED_POLICY_ROUND_ROBIN = 0)

**Diseño**:
- Cola FIFO única
- Sin prioridades
- Todos los procesos reciben trato igualitario

**Implementación**:
```c
PCB* select_next_process(Scheduler* sched) {
    if (sched->policy == SCHED_POLICY_ROUND_ROBIN) {
        if (sched->ready_queue->current_size == 0) return NULL;
        return dequeue_process(sched->ready_queue);
    }
    // ...
}
```

**Características**:
- Simplicidad: O(1) para selección
- Fairness: Todos los procesos avanzan equitativamente
- No hay inanición (starvation)

**Gestión de Quantum**:
```c
if (pcb->quantum_counter >= sched->quantum) {
    // Expulsar proceso
    pcb->state = WAITING;
    pcb->quantum_counter = 0;
    enqueue_process(ready_queue, pcb);  // Al final de la cola
}
```

#### 2.2 Brain Fuck Scheduler (SCHED_POLICY_BFS = 1)

**Diseño**:
- Cola única con selección por virtual deadline
- Priorización implícita mediante deadlines
- Balance entre fairness y prioridad

**Cálculo de Virtual Deadline**:
```c
// Al encolar o reencolar un proceso
int offset = (sched->quantum * pcb->priority) / 100;
int current_tick = clk_counter;
pcb->virtual_deadline = current_tick + offset;
```

**Matemática del offset**:
- Prioridad -20: offset = (quantum * -20) / 100 = negativo → deadline más temprana
- Prioridad 0: offset = 0 → deadline = tick actual
- Prioridad +19: offset = (quantum * 19) / 100 = positivo → deadline más tardía

**Implementación de selección**:
```c
PCB* select_next_process_bfs(Scheduler* sched) {
    int min_deadline = -1;
    int min_idx = -1;
    
    // Buscar proceso con menor deadline
    for (int i = 0; i < ready_queue->current_size; i++) {
        PCB* pcb = ready_queue->queue[idx];
        if (min_deadline == -1 || pcb->virtual_deadline < min_deadline) {
            min_deadline = pcb->virtual_deadline;
            min_idx = idx;
        }
        idx = (idx + 1) % max_capacity;
    }
    
    // Extraer el proceso seleccionado y reorganizar cola
    PCB* selected = ready_queue->queue[min_idx];
    // Shift elements...
    return selected;
}
```

**Complejidad**: O(n) para selección, pero permite priorización sin múltiples colas.

**Ventajas**:
- Priorización gradual (no absoluta como en Preemptive)
- Una sola cola (menos overhead que 40 colas)
- Procesos de baja prioridad no sufren inanición extrema

#### 2.3 Preemptive Priority (SCHED_POLICY_PREEMPTIVE_PRIO = 2)

**Diseño**:
- 40 colas (una por nivel de prioridad: -20 a +19)
- Política expulsora por evento
- Prioridades estáticas

**Estructura de colas**:
```c
// Creación de colas de prioridad
sched->priority_queues = malloc(sizeof(ProcessQueue*) * NUM_PRIORITY_LEVELS);
for (int i = 0; i < NUM_PRIORITY_LEVELS; i++) {
    sched->priority_queues[i] = create_process_queue(queue_capacity);
}
```

**Mapeo prioridad → índice**:
```c
int queue_idx = pcb->priority - MIN_PRIORITY;
// priority -20 → index 0 (mayor prioridad)
// priority +19 → index 39 (menor prioridad)
```

**Selección de proceso**:
```c
// Buscar desde la cola de mayor prioridad
for (int prio = MIN_PRIORITY; prio <= MAX_PRIORITY; prio++) {
    int queue_idx = prio - MIN_PRIORITY;
    ProcessQueue* pq = priority_queues[queue_idx];
    
    if (pq && pq->current_size > 0) {
        return dequeue_process(pq);  // Primera cola no vacía
    }
}
return NULL;  // Todas las colas vacías
```

**Expulsión (Preemption)**:

Cuando llega un proceso de alta prioridad y no hay slots libres:

```c
void preempt_lower_priority_processes(Scheduler* sched, PCB* new_pcb) {
    // Buscar proceso de menor prioridad en ejecución
    int lowest_prio = get_lowest_priority_executing(machine, &cpu, &core, &thread);
    
    if (new_pcb->priority < lowest_prio) {  // Mayor importancia
        // Guardar contexto del proceso expulsado
        save_context(hw_thread, pcb);
        pcb->state = WAITING;
        
        // Reencolar en su cola de prioridad
        enqueue_to_priority_queue(sched, pcb);
        
        // Asignar nuevo proceso al HardwareThread
        assign_pcb_to_hw_thread(hw_thread, new_pcb);
    }
}
```

**Características event-driven**:

Los eventos que disparan replanificación:
1. Llegada de nuevo proceso (desde generator)
2. Finalización de proceso (TTL=0 o EXIT)
3. Expiración de quantum
4. Interrupción del timer (en modo TIMER)

**Problema de inanición**: Procesos de prioridad +19 pueden no ejecutarse nunca si constantemente llegan procesos de mayor prioridad. Esto es **intencional** en esta política para garantizar ejecución de tareas críticas.

### 3. Ciclo de Planificación

#### 3.1 Loop Principal del Scheduler

```c
void* scheduler_function(void* arg) {
    int last_tick = 0;
    
    while (sched->running && running) {
        // 1. SINCRONIZACIÓN
        if (sync_mode == SCHED_SYNC_TIMER) {
            // Esperar interrupción del timer
            pthread_mutex_lock(&sched_mutex);
            pthread_cond_wait(&sched_cond, &sched_mutex);
            pthread_mutex_unlock(&sched_mutex);
        } else {
            // Esperar tick del reloj
            pthread_mutex_lock(&clk_mutex);
            while (clk_counter == last_tick) {
                pthread_cond_wait(&clk_cond, &clk_mutex);
            }
            last_tick = clk_counter;
        }
        
        // 2. GESTIÓN DE QUANTUM Y TERMINACIÓN
        for (cada HardwareThread con PCB) {
            pcb->quantum_counter++;
            
            if (pcb->state == TERMINATED || pcb->ttl <= 0) {
                // Liberar proceso y recursos
                destroy_pcb(pcb);
                hw_thread->pcb = NULL;
                sched->total_completed++;
            }
            else if (quantum expirado) {
                // Guardar contexto
                save_context(hw_thread, pcb);
                // Reencolar
                enqueue_to_scheduler(sched, pcb);
                // Liberar HardwareThread
                hw_thread->pcb = NULL;
            }
        }
        
        // 3. ASIGNACIÓN DE NUEVOS PROCESOS
        while (has_ready_processes(sched) && has_free_hw_threads(machine)) {
            PCB* next = select_next_process(sched);
            HardwareThread* hw = find_free_hw_thread(machine);
            
            // Cargar contexto
            hw->PC = next->context.pc;
            hw->IR = next->context.instruction;
            hw->PTBR = next->mm.pgb;
            for (int i = 0; i < 16; i++) {
                hw->registers[i] = next->context.registers[i];
            }
            
            // Configurar MMU
            hw->mmu.page_table_base = next->mm.pgb;
            hw->mmu.enabled = 1;
            
            // Invalidar TLB (cambio de contexto)
            for (int i = 0; i < TLB_SIZE; i++) {
                hw->tlb.entries[i].valid = 0;
            }
            
            // Asignar PCB
            hw->pcb = next;
            next->state = RUNNING;
            next->quantum_counter = 0;
        }
        
        if (sync_mode == SCHED_SYNC_TIMER) {
            pthread_mutex_unlock(&clk_mutex);
        } else {
            pthread_mutex_unlock(&clk_mutex);
        }
    }
}
```

#### 3.2 Cambio de Contexto

**Guardar contexto** (al expulsar):
```c
void save_context(HardwareThread* hw, PCB* pcb) {
    pcb->context.pc = hw->PC;
    pcb->context.instruction = hw->IR;
    for (int i = 0; i < 16; i++) {
        pcb->context.registers[i] = hw->registers[i];
    }
}
```

**Restaurar contexto** (al reanudar):
```c
void restore_context(HardwareThread* hw, PCB* pcb) {
    hw->PC = pcb->context.pc;
    hw->IR = pcb->context.instruction;
    hw->PTBR = pcb->mm.pgb;
    for (int i = 0; i < 16; i++) {
        hw->registers[i] = pcb->context.registers[i];
    }
    hw->mmu.page_table_base = pcb->mm.pgb;
    hw->mmu.enabled = 1;
}
```

**Costo**: 18 asignaciones (PC, IR, PTBR, 16 registros) más invalidación de TLB.

### 4. Generador de Procesos

#### 4.1 Diseño

```c
typedef struct {
    int min_interval, max_interval;  // Rango de intervalo entre creaciones
    int min_ttl, max_ttl;            // Rango de TTL
    ProcessQueue* ready_queue;       // Destino de procesos nuevos
    Machine* machine;                // Para verificar slots disponibles
    Scheduler* scheduler;            // Para acceder a priority_queues
    int max_processes;               // Límite total
    volatile int next_pid;           // Contador de PIDs
    volatile int total_generated;    // Estadística
} ProcessGenerator;
```

#### 4.2 Loop de Generación

```c
void* process_generator_function(void* arg) {
    int next_generation_tick = rand() % (max_interval - min_interval) + min_interval;
    
    while (running) {
        pthread_mutex_lock(&clk_mutex);
        
        // Esperar hasta el próximo tick de generación
        while (clk_counter < next_generation_tick) {
            pthread_cond_wait(&clk_cond, &clk_mutex);
        }
        
        // Verificar límite de procesos
        int total = ready_queue->size + executing_count + priority_queues_count;
        if (total >= max_processes) {
            pthread_mutex_unlock(&clk_mutex);
            continue;  // Esperar a que termine algún proceso
        }
        
        // Crear nuevo PCB
        PCB* pcb = create_pcb(next_pid++);
        pcb->ttl = random_range(min_ttl, max_ttl);
        pcb->priority = random_range(MIN_PRIORITY, MAX_PRIORITY);
        
        // Encolar según política del scheduler
        enqueue_to_scheduler(scheduler, pcb);
        total_generated++;
        
        // Calcular próximo tick de generación
        int interval = random_range(min_interval, max_interval);
        next_generation_tick = clk_counter + interval;
        
        pthread_mutex_unlock(&clk_mutex);
    }
}
```

---

## Gestor de Memoria Virtual

### 1. Diseño de la Memoria Física

#### 1.1 Configuración

**Parámetros básicos**:
```c
#define ADDRESS_BUS_BITS 24              // 16,777,216 bytes (16 MB)
#define WORD_SIZE 4                      // 4 bytes por palabra
#define PAGE_SIZE 4096                   // 4 KB por página
#define PHYSICAL_MEMORY_SIZE (1 << 24)  // 16 MB
#define TOTAL_FRAMES 4096                // 16 MB / 4 KB
#define KERNEL_SPACE_SIZE (1024 * 1024) // 1 MB para kernel
#define KERNEL_FRAMES 256                // 1 MB / 4 KB
#define USER_FRAMES 3840                 // 15 MB / 4 KB
```

**Estructura**:
```c
typedef struct {
    uint32_t* memory;                    // Array de 4M palabras
    uint8_t* frame_bitmap;               // 4096 bits (1 por marco)
    uint32_t kernel_space_end;           // 256K palabras
    uint32_t user_space_start;           // 256K palabras
    uint32_t next_kernel_frame;          // Asignador para kernel
    uint32_t total_allocated_frames;     // Contador
} PhysicalMemory;
```

#### 1.2 Distribución de Memoria

```
Memoria Física (16 MB):
┌──────────────────────────────────────┐ 0x000000
│                                      │
│   Kernel Space (1 MB = 256 marcos)   │ ← Tablas de páginas
│                                      │
├──────────────────────────────────────┤ 0x100000
│                                      │
│                                      │
│  User Space (15 MB = 3840 marcos)    │ ← Código y datos de procesos
│                                      │
│                                      │
└──────────────────────────────────────┘ 0xFFFFFF
```

**Decisión de diseño**: Reservar 1 MB para tablas de páginas permite alojar hasta ~1000 tablas de páginas pequeñas sin fragmentación.

### 2. Paginación

#### 2.1 Page Table Entry

```c
typedef struct {
    uint32_t frame_number : 12;  // Marco físico (0-4095)
    uint32_t present : 1;        // 1=en memoria, 0=page fault
    uint32_t rw : 1;             // 1=R/W, 0=solo lectura
    uint32_t user : 1;           // 1=usuario, 0=supervisor
    uint32_t accessed : 1;       // Bit de acceso
    uint32_t dirty : 1;          // Bit de modificación
    uint32_t reserved : 15;      // Reservado
} PageTableEntry;  // Total: 32 bits
```

**Diseño compacto**: Cada entrada ocupa exactamente 4 bytes, permitiendo almacenar 1024 entradas por página (4 KB / 4 bytes).

#### 2.2 Traducción de Direcciones

**Dirección virtual (32 bits)**:
```
┌─────────────────────┬──────────────┐
│  Page Number (20)   │  Offset (12) │
└─────────────────────┴──────────────┘
 Bits 31-12              Bits 11-0
```

**Proceso de traducción**:
```c
uint32_t translate_virtual_to_physical(
    PhysicalMemory* pm,
    PageTableEntry* page_table,
    uint32_t virtual_address
) {
    // 1. Extraer número de página y offset
    uint32_t page_number = virtual_address >> 12;
    uint32_t offset = virtual_address & 0xFFF;
    
    // 2. Consultar tabla de páginas
    PageTableEntry* pte = &page_table[page_number];
    
    // 3. Verificar present bit
    if (!pte->present) {
        // Page fault (no implementado aún)
        fprintf(stderr, "Page fault at virtual address 0x%08X\n", virtual_address);
        return 0xFFFFFFFF;
    }
    
    // 4. Construir dirección física
    uint32_t frame_number = pte->frame_number;
    uint32_t physical_address = (frame_number << 12) | offset;
    
    return physical_address;
}
```

**Complejidad**: O(1) - un acceso a tabla de páginas.

### 3. Translation Lookaside Buffer (TLB)

#### 3.1 Diseño del TLB

```c
#define TLB_SIZE 16

typedef struct {
    uint32_t virtual_page;   // Página virtual
    uint32_t physical_frame; // Marco físico
    uint8_t valid;           // Entrada válida
} TLBEntry;

typedef struct {
    TLBEntry entries[TLB_SIZE];
    int next_replace;        // Round-robin
} TLB;
```

#### 3.2 Búsqueda en TLB

```c
int tlb_lookup(TLB* tlb, uint32_t virtual_page, uint32_t* physical_frame) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb->entries[i].valid && 
            tlb->entries[i].virtual_page == virtual_page) {
            *physical_frame = tlb->entries[i].physical_frame;
            return 1;  // TLB hit
        }
    }
    return 0;  // TLB miss
}
```

#### 3.3 Actualización del TLB

```c
void tlb_update(TLB* tlb, uint32_t virtual_page, uint32_t physical_frame) {
    int idx = tlb->next_replace;
    tlb->entries[idx].virtual_page = virtual_page;
    tlb->entries[idx].physical_frame = physical_frame;
    tlb->entries[idx].valid = 1;
    tlb->next_replace = (tlb->next_replace + 1) % TLB_SIZE;
}
```

**Política de reemplazo**: Round-robin simple. En un sistema real se usaría LRU o pseudo-LRU.

#### 3.4 Traducción con TLB

```c
uint32_t mmu_translate_with_tlb(
    HardwareThread* hw,
    PhysicalMemory* pm,
    uint32_t virtual_address
) {
    uint32_t page_number = virtual_address >> 12;
    uint32_t offset = virtual_address & 0xFFF;
    uint32_t physical_frame;
    
    // 1. Buscar en TLB
    if (tlb_lookup(&hw->tlb, page_number, &physical_frame)) {
        // TLB hit - traducción directa
        return (physical_frame << 12) | offset;
    }
    
    // 2. TLB miss - consultar tabla de páginas
    PageTableEntry* page_table = (PageTableEntry*)hw->PTBR;
    PageTableEntry* pte = &page_table[page_number];
    
    if (!pte->present) {
        // Page fault
        return 0xFFFFFFFF;
    }
    
    physical_frame = pte->frame_number;
    
    // 3. Actualizar TLB
    tlb_update(&hw->tlb, page_number, physical_frame);
    
    // 4. Retornar dirección física
    return (physical_frame << 12) | offset;
}
```

**Optimización**: En TLB hit, evitamos un acceso a memoria (tabla de páginas).

### 4. Gestor de Marcos

#### 4.1 Bitmap de Marcos

```c
// Inicialización
pm->frame_bitmap = calloc(TOTAL_FRAMES, sizeof(uint8_t));

// Marcar marcos del kernel como ocupados
for (uint32_t i = 0; i < KERNEL_FRAMES; i++) {
    pm->frame_bitmap[i] = 1;
}
```

#### 4.2 Asignación de Marcos

```c
uint32_t allocate_frame(PhysicalMemory* pm) {
    // Buscar primer marco libre en user space
    for (uint32_t i = KERNEL_FRAMES; i < TOTAL_FRAMES; i++) {
        if (pm->frame_bitmap[i] == 0) {
            pm->frame_bitmap[i] = 1;
            pm->total_allocated_frames++;
            return i;
        }
    }
    
    // No hay marcos disponibles
    fprintf(stderr, "Error: Out of physical memory frames\n");
    return 0xFFFFFFFF;
}
```

**Complejidad**: O(n) en el peor caso. Optimización posible: mantener lista de marcos libres.

#### 4.3 Liberación de Marcos

```c
void free_frame(PhysicalMemory* pm, uint32_t frame_number) {
    if (frame_number >= KERNEL_FRAMES && frame_number < TOTAL_FRAMES) {
        if (pm->frame_bitmap[frame_number] == 1) {
            pm->frame_bitmap[frame_number] = 0;
            pm->total_allocated_frames--;
        }
    }
}
```

### 5. Loader de Programas

#### 5.1 Formato ELF (Prometheus)

**Formato de archivo**:
```
.text 000000        ← Dirección virtual del código
.data 000014        ← Dirección virtual de los datos
0A00002C            ← Instrucción 1 (8 dígitos hex)
0B000030            ← Instrucción 2
2CAB0000            ← Instrucción 3
...
```

#### 5.2 Proceso de Carga

**Paso 1: Parser del archivo**
```c
Program* load_program_from_elf(const char* filename) {
    FILE* file = fopen(filename, "r");
    
    uint32_t text_addr = 0, data_addr = 0;
    
    // Primera pasada: encontrar .text y .data, contar palabras
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, ".text", 5) == 0) {
            sscanf(line, ".text %x", &text_addr);
        } else if (strncmp(line, ".data", 5) == 0) {
            sscanf(line, ".data %x", &data_addr);
        } else {
            // Contar instrucciones
            total_words++;
        }
    }
    
    // Segunda pasada: leer todas las palabras
    rewind(file);
    uint32_t* full_program = malloc(total_words * sizeof(uint32_t));
    int idx = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] != '.') {
            sscanf(line, "%x", &full_program[idx++]);
        }
    }
    
    fclose(file);
    return program;
}
```

**Paso 2: Calcular tamaño de segmentos**
```c
// Calcular tamaños basándose en direcciones virtuales
uint32_t code_size = (data_addr - text_addr) / 4;  // En palabras
uint32_t data_size = total_words - code_size;
```

**Paso 3: Crear tabla de páginas**
```c
PCB* create_process_from_program(Loader* loader, Program* program) {
    // Calcular páginas necesarias
    uint32_t total_bytes = (program->code_size + program->data_size) * 4;
    uint32_t num_pages = (total_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // Crear tabla de páginas
    PageTableEntry* page_table = create_page_table(pm, num_pages);
    
    // ...
}
```

**Paso 4: Copiar a memoria física**
```c
// Para cada página
for (uint32_t page = 0; page < num_pages; page++) {
    uint32_t frame = page_table[page].frame_number;
    uint32_t frame_start_addr = frame * (PAGE_SIZE / WORD_SIZE);
    
    // Copiar hasta PAGE_SIZE bytes
    uint32_t words_to_copy = min(PAGE_SIZE / WORD_SIZE, remaining_words);
    for (uint32_t i = 0; i < words_to_copy; i++) {
        pm->memory[frame_start_addr + i] = program_data[word_idx++];
    }
    remaining_words -= words_to_copy;
}
```

**Paso 5: Configurar PCB**
```c
pcb->mm.code = (void*)(uintptr_t)program->header.text_address;
pcb->mm.data = (void*)(uintptr_t)program->header.data_address;
pcb->mm.pgb = (void*)page_table;

// Calcular TTL y prioridad basándose en tamaño
pcb->ttl = 50 + (total_words / 10);  // Más código = más tiempo
pcb->priority = (total_words < 100) ? -10 : 0;  // Programas pequeños = alta prioridad

pcb->context.pc = program->header.entry_point;
```

### 6. Memory Management Unit (MMU)

#### 6.1 Estructura

```c
typedef struct {
    void* page_table_base;  // PTBR
    int enabled;            // MMU on/off
} MMU;
```

#### 6.2 Lectura con MMU

```c
uint32_t mmu_read_word(
    PhysicalMemory* pm,
    PageTableEntry* page_table,
    uint32_t virtual_address
) {
    // Traducir dirección virtual a física
    uint32_t physical_addr = translate_virtual_to_physical(
        pm, page_table, virtual_address
    );
    
    if (physical_addr == 0xFFFFFFFF) {
        // Page fault
        return 0;
    }
    
    // Leer de memoria física
    uint32_t word_index = physical_addr / WORD_SIZE;
    return pm->memory[word_index];
}
```

#### 6.3 Escritura con MMU

```c
void mmu_write_word(
    PhysicalMemory* pm,
    PageTableEntry* page_table,
    uint32_t virtual_address,
    uint32_t value
) {
    uint32_t physical_addr = translate_virtual_to_physical(
        pm, page_table, virtual_address
    );
    
    if (physical_addr == 0xFFFFFFFF) return;
    
    uint32_t word_index = physical_addr / WORD_SIZE;
    pm->memory[word_index] = value;
    
    // Marcar página como dirty
    uint32_t page_number = virtual_address >> 12;
    page_table[page_number].dirty = 1;
}
```

### 7. Ciclo de Ejecución de Instrucciones

```c
void execute_instruction_cycle(HardwareThread* hw, PhysicalMemory* pm) {
    if (!hw->pcb || !hw->mmu.enabled) return;
    
    PageTableEntry* page_table = (PageTableEntry*)hw->PTBR;
    
    // 1. FETCH: Leer instrucción
    uint32_t instruction = mmu_read_word(pm, page_table, hw->PC);
    hw->IR = instruction;
    
    // 2. DECODE: Decodificar instrucción
    uint8_t opcode = (instruction >> 24) & 0xFF;
    uint8_t dest = (instruction >> 16) & 0xFF;
    uint8_t src1 = (instruction >> 8) & 0xFF;
    uint8_t src2 = instruction & 0xFF;
    
    // 3. EXECUTE: Ejecutar según opcode
    switch (opcode) {
        case OP_ADD:
            hw->registers[dest] = hw->registers[src1] + hw->registers[src2];
            hw->PC += 4;
            break;
        
        case OP_LOAD:
            uint32_t addr = hw->registers[src1] + src2;
            hw->registers[dest] = mmu_read_word(pm, page_table, addr);
            hw->PC += 4;
            break;
        
        case OP_STORE:
            addr = hw->registers[src1] + src2;
            mmu_write_word(pm, page_table, addr, hw->registers[dest]);
            hw->PC += 4;
            break;
        
        case OP_EXIT:
            hw->pcb->state = TERMINATED;
            break;
        
        // ... más instrucciones
    }
}
```

---

## Integración de Componentes

### 1. Inicialización del Sistema

```c
int main(int argc, char* argv[]) {
    // 1. Parsear argumentos
    parse_arguments(argc, argv);
    
    // 2. Crear memoria física
    PhysicalMemory* pm = create_physical_memory();
    
    // 3. Crear máquina virtual
    Machine* machine = create_machine(num_cpus, num_cores, num_threads);
    
    // 4. Crear cola de listos
    ProcessQueue* ready_queue = create_process_queue(queue_size);
    
    // 5. Crear scheduler
    Scheduler* scheduler = create_scheduler_with_policy(
        quantum, policy, sync_mode, NULL, ready_queue, machine
    );
    
    // 6. Crear loader
    Loader* loader = create_loader(pm, ready_queue, machine, scheduler);
    
    // 7. Crear generador de procesos
    ProcessGenerator* gen = create_process_generator(
        pgen_min, pgen_max, ttl_min, ttl_max,
        ready_queue, machine, scheduler, max_processes, 1
    );
    
    // 8. Configurar referencias globales
    set_clock_machine(machine);
    set_clock_physical_memory(pm);
    
    // 9. Iniciar componentes
    start_clock(&clock_thread);
    start_scheduler(scheduler);
    start_process_generator(gen);
    
    // 10. Loop principal (esperar señal de terminación)
    while (running) {
        sleep(1);
    }
    
    // 11. Cleanup
    cleanup_system();
    return 0;
}
```

### 2. Flujo de un Proceso Completo

```
1. ProcessGenerator crea PCB
   ├── Asigna PID único
   ├── Genera prioridad aleatoria
   └── Carga programa desde .elf
       ├── Loader parsea archivo
       ├── Crea tabla de páginas
       ├── Asigna marcos físicos
       └── Copia código/datos a memoria

2. PCB se encola
   ├── Round Robin: ready_queue
   ├── BFS: ready_queue (con deadline calculado)
   └── Preemptive: priority_queues[prioridad]

3. Scheduler selecciona PCB
   ├── Busca HardwareThread libre
   ├── Carga contexto (PC, registros, PTBR)
   ├── Configura MMU
   ├── Invalida TLB
   └── Marca proceso como RUNNING

4. SystemClock ejecuta proceso
   ├── Cada tick:
   │   ├── Decrementa TTL
   │   └── Ejecuta ciclo de instrucción
   │       ├── Fetch (con traducción de PC)
   │       ├── Decode
   │       └── Execute (con MMU si accede memoria)
   └── Actualiza PC

5. Scheduler verifica quantum
   ├── Incrementa quantum_counter
   └── Si expiró:
       ├── Guarda contexto en PCB
       ├── Libera HardwareThread
       └── Reencola proceso

6. Finalización
   ├── TTL llega a 0 o EXIT instruction
   ├── Marca TERMINATED
   ├── Scheduler libera recursos
   │   ├── Destruye tabla de páginas
   │   └── Libera marcos físicos
   └── Incrementa total_completed
```

### 3. Sincronización entre Componentes

**Mutexes utilizados**:
- `clk_mutex`: Protege acceso a `clk_counter` y notificaciones del reloj
- `sched_mutex`: Para activación del scheduler en modo TIMER

**Condition variables**:
- `clk_cond`: Broadcast del reloj para despertar todos los componentes
- `sched_cond`: Signal del timer para despertar scheduler

**Protocolo de sincronización**:
```
SystemClock (cada tick):
    pthread_mutex_lock(&clk_mutex);
    clk_counter++;
    // Trabajo del reloj...
    pthread_cond_broadcast(&clk_cond);  // Despertar a todos
    pthread_mutex_unlock(&clk_mutex);

Scheduler (modo CLOCK):
    pthread_mutex_lock(&clk_mutex);
    while (clk_counter == last_tick) {
        pthread_cond_wait(&clk_cond, &clk_mutex);  // Esperar tick
    }
    // Trabajo del scheduler...
    pthread_mutex_unlock(&clk_mutex);

ProcessGenerator:
    pthread_mutex_lock(&clk_mutex);
    while (clk_counter < next_generation_tick) {
        pthread_cond_wait(&clk_cond, &clk_mutex);  // Esperar tick
    }
    // Generar proceso...
    pthread_mutex_unlock(&clk_mutex);
```

---

## Conclusiones

### Logros de la Implementación

1. **Arquitectura modular**: Cada componente tiene responsabilidades claras y puede modificarse independientemente

2. **Múltiples políticas de planificación**: El sistema permite comparar el comportamiento de Round Robin, BFS y planificación con prioridades

3. **Memoria virtual funcional**: Implementación completa de paginación con MMU y TLB

4. **Sincronización robusta**: Uso correcto de mutexes y condition variables para coordinar threads

5. **Flexibilidad**: Parámetros configurables por línea de comandos permiten experimentar con diferentes configuraciones

### Limitaciones Actuales

1. **Sin page faults**: No hay intercambio a disco ni demand paging

2. **Asignación de marcos lineal**: Búsqueda O(n), podría optimizarse con lista de libres

3. **TLB simple**: Reemplazo round-robin en lugar de LRU

4. **Sin protección real**: Los bits de permisos no se verifican en tiempo de ejecución

5. **Inanición en Preemptive**: Procesos de baja prioridad pueden no ejecutarse

### Análisis de Complejidad

**Operaciones críticas**:
- Enqueue/Dequeue: O(1)
- Selección RR: O(1)
- Selección BFS: O(n) donde n = procesos en ready_queue
- Selección Preemptive: O(40) = O(1) (máximo 40 colas)
- Traducción de dirección: O(1) con TLB hit, O(2) con TLB miss
- Asignación de marco: O(n) donde n = 3840 marcos
- Cambio de contexto: O(1) - 18 asignaciones + TLB flush

**Escalabilidad**:
- El sistema puede manejar cientos de procesos limitado principalmente por:
  1. Memoria física disponible (3840 marcos)
  2. Overhead del scheduler (depende de la política)
  3. Frecuencia del reloj (a mayor frecuencia, más overhead)

### Conclusión Final

El sistema LocOS representa una implementación educativa completa de los componentes esenciales de un sistema operativo moderno. La arquitectura modular facilita la comprensión de conceptos como planificación, memoria virtual y sincronización. La posibilidad de configurar múltiples políticas y parámetros permite experimentar y observar el comportamiento del sistema bajo diferentes condiciones, haciendo de LocOS una herramienta valiosa para el aprendizaje de sistemas operativos.

---

**Fin del Informe Técnico**
