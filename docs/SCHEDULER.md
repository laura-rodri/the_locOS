# Scheduler - Sistema de Planificación

## Descripción General

El scheduler de the_locOS es responsable de asignar procesos (PCBs) a los HardwareThreads disponibles en la máquina virtual. Soporta múltiples políticas de planificación y modos de sincronización configurables.

## Estructura del Scheduler

```c
typedef struct {
    int quantum;                     // Quantum máximo por proceso (ticks)
    int policy;                      // Política de planificación
    int sync_mode;                   // Modo de sincronización (CLOCK o TIMER)
    void* sync_source;               // Timer* si sync_mode==TIMER, NULL si CLOCK
    ProcessQueue* ready_queue;       // Cola de listos (RR y BFS)
    ProcessQueue** priority_queues;  // 40 colas para prioridades (Preemptive)
    Machine* machine;                // Referencia a la máquina virtual
    pthread_t thread;                // Thread del scheduler
    volatile int running;            // Flag de control
    volatile int total_completed;    // Procesos completados
    pthread_mutex_t sched_mutex;     // Mutex de sincronización
    pthread_cond_t sched_cond;       // Condition variable
} Scheduler;
```

## Modos de Sincronización

### 1. Sincronización con Reloj Global (SCHED_SYNC_CLOCK = 0)

- El scheduler se despierta **cada tick** del reloj del sistema
- Verifica quantum de procesos en ejecución cada tick
- Más responsive pero mayor overhead
- **Default mode**

**Comportamiento**:
```
Cada tick del SystemClock:
    → Scheduler verifica quantum_counter de todos los procesos
    → Si quantum_counter >= quantum: preempt process
    → Intenta asignar nuevos procesos a HardwareThreads libres
```

### 2. Sincronización con Timer Dedicado (SCHED_SYNC_TIMER = 1)

- El scheduler se despierta cuando el Timer interrumpe
- El intervalo del Timer es independiente del quantum (configurable con `-timert`)
- Cuando se despierta, verifica si quantum_counter >= 1 para expulsar
- Menos overhead que CLOCK, scheduler solo se activa en interrupciones
- Útil para reducir activaciones innecesarias del scheduler

**Comportamiento**:
```
Timer interrumpe (cada timer_interval ticks):
    → Callback activa el scheduler
    → Scheduler incrementa quantum_counter de procesos en ejecución
    → Si quantum_counter >= 1: expulsar y reencolar
    → Scheduler verifica y replanifica todos los procesos
```

## Políticas de Planificación

### 1. Round Robin (SCHED_POLICY_ROUND_ROBIN = 0) - Default

**Características**:
- Planificación circular **sin prioridades**
- Cola única FIFO (First In, First Out)
- Todos los procesos reciben igual tratamiento
- Quantum de tiempo fijo para cada proceso

**Algoritmo**:
```
1. Tomar primer proceso de ready_queue
2. Asignar a HardwareThread libre
3. Ejecutar hasta quantum o finalización
4. Si no terminó: volver al final de ready_queue
5. Repetir
```

**Ventajas**:
- Simple y justo
- Evita inanición (starvation)
- Predecible

**Desventajas**:
- No distingue entre procesos críticos y no críticos
- Todos los procesos tienen la misma importancia

**Uso**: Sistemas donde todos los procesos tienen igual prioridad.

### 2. Brain Fuck Scheduler (SCHED_POLICY_BFS = 1)

**Características**:
- Selecciona proceso con **virtual_deadline más baja**
- Asigna rodajas de tiempo fijas (quantum)
- Usa prioridades para calcular deadlines
- Cola única ordenada por deadline

**Cálculo de Virtual Deadline**:
```
virtual_deadline = T_actual + offset
offset = (quantum * prioridad) / 100
```

- **Prioridad alta (-20)**: offset negativo → deadline más temprana
- **Prioridad baja (+19)**: offset positivo → deadline más tardía

**Algoritmo**:
```
1. Al crear proceso: calcular virtual_deadline
2. Ordenar ready_queue por virtual_deadline
3. Seleccionar proceso con menor deadline
4. Ejecutar hasta quantum o finalización
5. Si no terminó:
   - Recalcular virtual_deadline
   - Reinsertar en ready_queue ordenada
```

**Ventajas**:
- Priorización implícita sin múltiples colas
- Mejor para cargas mixtas (interactivas y batch)
- Balance entre fairness y prioridad

**Desventajas**:
- Overhead de reordenamiento
- Más complejo que Round Robin

**Uso**: Sistemas con procesos de diferentes tipos pero sin requerimientos estrictos de tiempo real.

### 3. Política Expulsora con Prioridades Estáticas (SCHED_POLICY_PREEMPTIVE_PRIO = 2)

**Características**:
- **40 colas de prioridad** (-20 a +19)
- **Política expulsora (preemptive)**: alta prioridad interrumpe baja prioridad
- **Event-driven**: replanifica en eventos específicos
- **Prioridades estáticas**: no cambian durante la vida del proceso

**Estructura**:
```
priority_queues[0]  → Procesos con prioridad -20 (máxima)
priority_queues[1]  → Procesos con prioridad -19
...
priority_queues[39] → Procesos con prioridad +19 (mínima)
```

**Algoritmo**:
```
1. Buscar primera cola no vacía (desde índice 0)
2. Tomar primer proceso de esa cola
3. Si no hay HardwareThread libre:
   - Buscar proceso con menor prioridad en ejecución
   - Si nueva prioridad > prioridad en ejecución:
     - EXPULSAR proceso de baja prioridad
     - Guardar su contexto
     - Reencolar en su cola de prioridad
     - Asignar HardwareThread al nuevo proceso
4. Ejecutar hasta quantum o finalización
5. Si quantum expira pero no termina:
   - Volver al final de su cola de prioridad
```

**Eventos que disparan replanificación**:
- Llegada de nuevo proceso de alta prioridad
- Finalización de un proceso (TTL=0)
- Expiración de quantum
- Interrupción del timer (si sync_mode = TIMER)

**Ventajas**:
- Garantiza CPU a procesos críticos
- Prioridad -20 siempre se ejecuta primero
- Útil para sistemas con tareas de diferentes criticidades

**Desventajas**:
- Posible inanición de procesos de baja prioridad
- Mayor overhead de gestión de múltiples colas
- Requiere configuración cuidadosa de prioridades

**Uso**: Sistemas con tareas críticas que requieren ejecución inmediata (ej: controladores en tiempo real).

## Funcionamiento del Quantum

### Contador de Quantum

Cada PCB tiene un campo `quantum_counter`:

```c
PCB->quantum_counter  // Incrementa cada tick mientras se ejecuta
```

### Gestión del Quantum

**Por el SystemClock** (cada tick):
```c
// En el reloj del sistema
for cada HardwareThread en ejecución:
    hw_thread->pcb->ttl--  // Decrementa TTL solamente
```

**Por el Scheduler**:
```c
// El scheduler incrementa quantum_counter cada vez que se despierta
pcb->quantum_counter++;

if (pcb->quantum_counter >= scheduler->quantum) {
    // Quantum expirado
    preempt_process(pcb);
    pcb->quantum_counter = 0;  // Reset
    requeue_process(pcb);       // Volver a cola
}
```

## Asignación de Procesos

### Búsqueda de HardwareThread Libre

```c
HardwareThread* find_free_hw_thread(Machine* machine) {
    for cada CPU:
        for cada Core:
            for cada HardwareThread:
                if (hw_thread->pcb == NULL)
                    return hw_thread;
    return NULL;
}
```

### Contexto de Ejecución

Al asignar un PCB a un HardwareThread:

```c
// Cargar contexto
hw_thread->PC = pcb->context.pc;
hw_thread->IR = pcb->context.instruction;
hw_thread->PTBR = pcb->mm.pgb;  // Tabla de páginas
copiar pcb->context.registers a hw_thread->registers

// Marcar como ejecutando
pcb->state = RUNNING;
hw_thread->pcb = pcb;
```

Al expulsar un proceso:

```c
// Guardar contexto
pcb->context.pc = hw_thread->PC;
pcb->context.instruction = hw_thread->IR;
copiar hw_thread->registers a pcb->context.registers

// Liberar HardwareThread
pcb->state = WAITING;
hw_thread->pcb = NULL;
```

## Selección de Política

### Parámetro de Línea de Comandos

```bash
./kernel -policy <num>
```

- `-policy 0`: Round Robin (equitativo, sin prioridades)
- `-policy 1`: BFS (mejor para cargas mixtas)
- `-policy 2`: Preemptive Priority (para tareas críticas)

### Casos de Uso

| Escenario | Política Recomendada |
|-----------|---------------------|
| Sistema compartido, todos igual importancia | Round Robin (0) |
| Mix de tareas interactivas y batch | BFS (1) |
| Control en tiempo real, tareas críticas | Preemptive Priority (2) |
| Sistema educativo/demo | Round Robin (0) |
| Simulación de Linux CFQ | BFS (1) |

## Interacción con Otros Componentes

### Con ProcessGenerator

```
ProcessGenerator crea nuevo PCB
    → Asigna prioridad aleatoria
    → Añade a ready_queue (RR, BFS)
      o priority_queues[prioridad] (Preemptive)
```

### Con SystemClock

```
SystemClock tick (cada tick)
    → Decrementa TTL de todos los procesos en ejecución
    → Ejecuta un ciclo de instrucción para cada proceso
    → Notifica al scheduler mediante broadcast (si sync_mode = CLOCK)

Scheduler (se despierta con cada tick si sync_mode = CLOCK)
    → Incrementa quantum_counter de procesos en ejecución
    → Verifica expiración de quantum y TTL
    → Replanifica procesos según sea necesario
```

### Con Machine

```
Scheduler necesita ejecutar proceso
    → Busca HardwareThread libre en Machine
    → Asigna PCB a HardwareThread
    → Configura registros, PTBR, MMU
```

## Compilación con Scheduler

```bash
cd sys
make clean
make
```

El Makefile incluye automáticamente `process.c` que contiene toda la implementación del scheduler.

## Ejemplo de Ejecución

```bash
# Round Robin, quantum 5, reloj a 10 Hz
./kernel -f 10 -q 5 -policy 0 -sync 0

# BFS, quantum 3, reloj a 5 Hz, sincronización con timer
./kernel -f 5 -q 3 -policy 1 -sync 1

# Preemptive Priority, quantum 2, reloj a 10 Hz
./kernel -f 10 -q 2 -policy 2 -sync 0
```

## Debugging y Monitoreo

El scheduler imprime información útil:

```
Scheduler started (policy: X, quantum: Y)
Process X assigned to CPU Y Core Z HardwareThread W
Process X completed (TTL expired)
Process X preempted (quantum expired)
Scheduler statistics:
  Total processes completed: N
```

Para debugging detallado, descomentar las líneas de debug en `process.c`.
