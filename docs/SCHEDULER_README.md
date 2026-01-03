# Scheduler - Sistema de Planificación

## Descripción General

El scheduler del sistema locOS ahora soporta múltiples políticas de planificación y modos de sincronización configurables mediante parámetros de línea de comandos.

## Características

### Modos de Sincronización

El scheduler puede sincronizarse de dos formas:

1. **Reloj Global (SCHED_SYNC_CLOCK = 0)**: El scheduler se despierta en cada tick del reloj global del sistema
2. **Timer (SCHED_SYNC_TIMER = 1)**: El scheduler se sincroniza con un timer dedicado cuyo intervalo es igual al quantum configurado

### Políticas de Planificación

El sistema soporta tres políticas de planificación:

#### 1. Round Robin (SCHED_POLICY_ROUND_ROBIN = 0) - Por Defecto
- Planificación circular sin prioridades
- Los procesos se ejecutan en orden FIFO (First In, First Out)
- Cada proceso recibe un quantum de tiempo de CPU
- Cuando expira el quantum, el proceso vuelve al final de la cola de listos
- Utiliza una única cola de preparados

#### 2. Brain Fuck Scheduler (SCHED_POLICY_BFS = 1)
- Selecciona siempre el proceso con la **virtual deadline más baja**
- Asigna rodajas de tiempo de ejecución fijas (quantum configurable)
- La virtual deadline se calcula: `deadline = T_actual + offset`
  - `T_actual`: número de ticks actual
  - `offset = rodaja_de_tiempo * prioridad / 100`
- Procesos con mayor prioridad (números negativos) obtienen deadlines más tempranas
- Útil para dar prioridad implícita a procesos sin usar colas separadas
- Utiliza una única cola de preparados

#### 3. Política Expulsora con Prioridades Estáticas (SCHED_POLICY_PREEMPTIVE_PRIO = 2)
- Utiliza **múltiples colas de preparados**, una por cada nivel de prioridad
- Las prioridades van de **-20 (mayor prioridad) a +19 (menor prioridad)**
- Total de 40 niveles de prioridad
- Siempre se ejecuta el primer proceso de la cola de mayor prioridad
- **Política expulsora por evento**: 
  - **Por Prioridades**: Cada proceso tiene un número que indica su importancia
  - **Expulsora (Preemptive)**: Un proceso de alta prioridad puede detener forzosamente (expulsar) a uno de menor prioridad
  - **Por Evento (Event-Driven)**: El planificador se activa cuando ocurre un evento:
    - Llegada de un nuevo proceso de alta prioridad
    - Terminación de un proceso
    - Fin de quantum de un proceso
    - Interrupción del timer
- Cuando llega un proceso de alta prioridad y no hay slots libres, **expulsa inmediatamente** al proceso de menor prioridad en ejecución
- Cada cola usa FIFO para procesos del mismo nivel de prioridad
- Garantiza que procesos críticos (-20) tengan prioridad absoluta sobre procesos de baja prioridad (+19)

## Estructura del Scheduler

```c
typedef struct {
    int quantum;                     // Quantum (max ticks per process)
    int policy;                      // Política de planificación
    int sync_mode;                   // Modo de sincronización (CLOCK o TIMER)
    void* sync_source;               // Timer* si sync_mode==TIMER, NULL si CLOCK
    ProcessQueue* ready_queue;       // Cola de procesos listos (RR y BFS)
    ProcessQueue** priority_queues;  // Array de colas para prioridades (40 colas)
    Machine* machine;                // Máquina con CPUs y cores
    pthread_t thread;                // Thread del scheduler
    volatile int running;            // Flag de control de ejecución
    volatile int total_completed;    // Total de procesos completados
} Scheduler;
```

## Estructura del PCB

```c
typedef struct {
    int pid;
    int state;
    int priority;           // Prioridad: -20 (mayor) a 19 (menor)
    int ttl;                // Time to live (actual)
    int initial_ttl;        // TTL inicial (para reset)
    int quantum_counter;    // Uso actual del quantum
    int virtual_deadline;   // Virtual deadline para BFS
} PCB;
```

## Prioridades

- **Rango**: -20 a +19
- **-20**: Mayor prioridad (más urgente)
- **0**: Prioridad normal
- **+19**: Menor prioridad (menos urgente)
- Los procesos generados reciben prioridades aleatorias en todo el rango

## Parámetros de Línea de Comandos

### Nuevos Parámetros

- `-q <ticks>`: Quantum del scheduler (default: 5 ticks)
  - Define el tiempo máximo que un proceso puede ejecutar antes de ser expulsado
  - Si se usa sincronización con timer, este valor también define el intervalo del timer

- `-policy <num>`: Política de planificación (default: 0)
  - `0`: Round Robin sin prioridades
  - `1`: Brain Fuck Scheduler
  - `2`: Política expulsora con prioridades estáticas

- `-sync <mode>`: Modo de sincronización (default: 0)
  - `0`: Sincronización con reloj global
  - `1`: Sincronización con timer dedicado

### Parámetros Existentes

- `-f <hz>`: Frecuencia del reloj en Hz (default: 1)
- `-pmin <ticks>`: Intervalo mínimo de generación de procesos (default: 3)
- `-pmax <ticks>`: Intervalo máximo de generación de procesos (default: 10)
- `-tmin <ticks>`: TTL mínimo de procesos (default: 10)
- `-tmax <ticks>`: TTL máximo de procesos (default: 50)
- `-qsize <num>`: Tamaño de la cola de listos (default: 100)
- `-cpus <num>`: Número de CPUs (default: 1)
- `-cores <num>`: Número de cores por CPU (default: 2)
- `-threads <num>`: Número de kernel threads por core (default: 4)

## Ejemplos de Uso

### Ejemplo 1: Round Robin con reloj global (configuración por defecto)
```bash
./kernel -q 5 -policy 0 -sync 0
```

### Ejemplo 2: Round Robin sincronizado con timer
```bash
./kernel -q 10 -policy 0 -sync 1
```
En este caso, el timer interrumpirá cada 10 ticks, coincidiendo con el quantum.

### Ejemplo 3: Brain Fuck Scheduler con virtual deadlines
```bash
./kernel -q 5 -policy 1 -sync 0
```
BFS calculará virtual deadlines para cada proceso: `deadline = tick_actual + (quantum * prioridad / 100)`

### Ejemplo 4: Planificación con prioridades y múltiples colas
```bash
./kernel -q 8 -policy 2 -sync 0
```
Se crean 40 colas de prioridad (una por cada nivel de -20 a +19). Los procesos se ejecutan estrictamente por prioridad.

**Ejemplo de expulsión por evento:**
```bash
./kernel -q 5 -policy 2 -sync 0 -f 3 -pmin 1 -pmax 2 -cpus 1 -cores 1 -threads 1
```
Con solo 1 thread, se puede observar claramente la expulsión:
- Un proceso de baja prioridad (ej: +6) está ejecutándose
- Llega un proceso de alta prioridad (ej: -16)
- **Evento de expulsión**: El scheduler inmediatamente detiene el de baja prioridad y ejecuta el de alta prioridad
- El proceso expulsado vuelve a su cola de prioridad correspondiente

### Ejemplo 5: Sistema completo personalizado
```bash
./kernel -f 2 -q 10 -policy 2 -sync 1 -pmin 5 -pmax 15 -tmin 20 -tmax 100 -qsize 50 -cpus 2 -cores 4 -threads 2
```
Este comando configura:
- Reloj a 2 Hz
- Quantum de 10 ticks
- Política de prioridades estáticas
- Sincronización con timer
- Generación de procesos cada 5-15 ticks
- TTL de procesos entre 20-100 ticks
- Cola de 50 procesos
- 2 CPUs, 4 cores por CPU, 2 threads por core

## Funcionamiento Interno

### Separación de Responsabilidades: Clock vs Scheduler

**CRÍTICO**: El sistema tiene una separación clara de responsabilidades entre el reloj y el scheduler:

- **Clock (clock_sys.c)**:
  - Genera ticks periódicos según la frecuencia configurada
  - **Decrementa el TTL** de todos los procesos en ejecución
  - Mantiene una referencia a Machine para acceder a los procesos
  - Notifica a todos los componentes mediante broadcast

- **Scheduler (process.c)**:
  - Gestiona la asignación de procesos a cores
  - Verifica procesos completados (TTL=0)
  - Verifica procesos que agotaron su quantum
  - Selecciona nuevos procesos según la política configurada
  - **NO decrementa TTL** - esto es responsabilidad del clock

Esta separación evita conflictos de mutex y race conditions.

### Sincronización con Reloj Global
Cuando `sync_mode = SCHED_SYNC_CLOCK`:
- El scheduler se despierta en cada tick del reloj
- Procesa todos los procesos en ejecución
- Decrementa TTL y incrementa contadores de quantum
- Asigna nuevos procesos desde la cola de listos

### Sincronización con Timer
Cuando `sync_mode = SCHED_SYNC_TIMER`:
- Se crea un timer con intervalo igual al quantum
- El timer genera interrupciones periódicas cada quantum ticks
- El timer ejecuta un callback que despierta al scheduler
- El scheduler se activa únicamente con las señales del timer
- El quantum del timer y del scheduler están sincronizados
- **Ventaja**: Evita activaciones innecesarias del scheduler en cada tick del reloj
- El clock sigue decrementando TTL independientemente

### Selección de Procesos

La función `select_next_process()` implementa la lógica de selección según la política:

- **Round Robin**: Extrae el primer proceso de la cola única (FIFO)
- **BFS**: Busca y extrae el proceso con menor virtual deadline de la cola única
  - Calcula deadline al crear proceso: `deadline = tick + (quantum * prioridad / 100)`
  - Recalcula deadline cuando el proceso vuelve a la cola tras agotar quantum
- **Prioridades**: Busca en las 40 colas desde la prioridad -20 hasta +19
  - Retorna el primer proceso de la primera cola no vacía
  - Garantiza que procesos de mayor prioridad siempre se ejecutan primero

### Colas de Preparados

El sistema usa diferentes estructuras según la política:

- **Round Robin y BFS**: Una única cola global (`ready_queue`)
  - Todos los procesos comparten la misma cola
  - RR: Extrae en orden FIFO
  - BFS: Busca el de menor virtual deadline
  
- **Prioridades Estáticas**: 40 colas de prioridad (`priority_queues`)
  - Una cola por cada nivel de prioridad (-20 a +19)
  - Los procesos se añaden a la cola de su prioridad
  - El scheduler siempre busca en las colas de mayor prioridad primero
  - Dentro de cada cola se usa FIFO
  - Capacidad distribuida: `ready_queue_size / 40` por cola

### Eventos que Activan el Scheduler (Event-Driven)

Cuando se usa la política de prioridades estáticas, el scheduler se activa en los siguientes eventos:

1. **Tick del reloj**: Evaluación periódica según configuración
2. **Llegada de nuevo proceso**: Cuando un proceso entra a la cola de preparados
3. **Fin de quantum**: Cuando un proceso agota su tiempo de CPU
4. **Terminación de proceso**: Cuando un proceso completa su ejecución
5. **Interrupción de timer**: Si está configurado con sincronización por timer

En cada evento, el scheduler:
- Evalúa las prioridades de todos los procesos
- Si hay un proceso de alta prioridad esperando y slots ocupados por procesos de baja prioridad, **expulsa al de menor prioridad**
- Asigna el CPU al proceso de mayor prioridad disponible

## Notas de Implementación

1. **Quantum**: Define cuántos ticks puede ejecutar un proceso antes de ser expulsado

2. **TTL (Time To Live)**: 
   - **CRÍTICO**: Decrementado por el Clock del sistema, NO por el scheduler
   - Cuando TTL llega a 0, el proceso se marca como completado
   - El scheduler detecta TTL=0 y retira el proceso de ejecución

3. **Prioridades**: 
   - Rango: -20 (mayor prioridad) a +19 (menor prioridad)
   - Asignadas aleatoriamente por el generador de procesos
   - Prioridades estáticas (no cambian durante la vida del proceso)

4. **Virtual Deadline (BFS)**: 
   - Fórmula: `deadline = T_actual + (quantum * prioridad / 100)`
   - Procesos con prioridad negativa obtienen deadlines más tempranas
   - Se recalcula cada vez que el proceso vuelve a la cola

5. **Múltiples Colas (Prioridades)**:
   - Se crean 40 colas al inicializar el scheduler con política PREEMPTIVE_PRIO
   - El generador añade procesos a `ready_queue`; el scheduler los redistribuye
   - Destrucción automática de todas las colas al terminar

6. **Timer dedicado**: 
   - Cuando se usa sincronización con timer, se crea exactamente un timer con ID 0
   - El timer ejecuta un callback que despierta al scheduler mediante señales
   - Evita problemas de mutex anidados usando condition variables

7. **Compatibilidad**: 
   - La función `create_scheduler()` mantiene compatibilidad con código anterior
   - Usa Round Robin y sincronización con reloj global por defecto

## Compilación

```bash
cd /home/launix/the_locOS/sys
make clean
make
```

## Ejecución

```bash
./kernel [parámetros]
./kernel --help  # Para ver ayuda completa
```
