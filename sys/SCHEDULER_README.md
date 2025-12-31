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

#### 2. Brain Fuck Scheduler (SCHED_POLICY_BFS = 1)
- Selecciona siempre el proceso con el PID más bajo
- Útil para pruebas y depuración
- Comportamiento determinista y predecible

#### 3. Política Expulsora con Prioridades Estáticas (SCHED_POLICY_PREEMPTIVE_PRIO = 2)
- Los procesos tienen prioridades estáticas
- Siempre se ejecuta el proceso con mayor prioridad (número más bajo = mayor prioridad)
- Política expulsora: un proceso de mayor prioridad puede interrumpir a uno de menor prioridad

## Estructura del Scheduler

```c
typedef struct {
    int quantum;                     // Quantum (max ticks per process)
    int policy;                      // Política de planificación
    int sync_mode;                   // Modo de sincronización (CLOCK o TIMER)
    void* sync_source;               // Timer* si sync_mode==TIMER, NULL si CLOCK
    ProcessQueue* ready_queue;       // Cola de procesos listos
    Machine* machine;                // Máquina con CPUs y cores
    pthread_t thread;                // Thread del scheduler
    volatile int running;            // Flag de control de ejecución
    volatile int total_completed;    // Total de procesos completados
} Scheduler;
```

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

### Ejemplo 3: Brain Fuck Scheduler con reloj global
```bash
./kernel -q 5 -policy 1 -sync 0
```

### Ejemplo 4: Planificación con prioridades y timer
```bash
./kernel -q 8 -policy 2 -sync 1
```

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

### Sincronización con Reloj Global
Cuando `sync_mode = SCHED_SYNC_CLOCK`:
- El scheduler se despierta en cada tick del reloj
- Procesa todos los procesos en ejecución
- Decrementa TTL y incrementa contadores de quantum
- Asigna nuevos procesos desde la cola de listos

### Sincronización con Timer
Cuando `sync_mode = SCHED_SYNC_TIMER`:
- Se crea un timer con intervalo igual al quantum
- El timer genera interrupciones periódicas
- El scheduler se activa con las interrupciones del timer
- El quantum del timer y del scheduler están sincronizados

### Selección de Procesos

La función `select_next_process()` implementa la lógica de selección según la política:

- **Round Robin**: Extrae el primer proceso de la cola (FIFO)
- **BFS**: Busca y extrae el proceso con menor PID
- **Prioridades**: Busca y extrae el proceso con mayor prioridad (menor número)

## Notas de Implementación

1. **Quantum**: Define cuántos ticks puede ejecutar un proceso antes de ser expulsado
2. **Prioridades**: En la política de prioridades, un valor menor indica mayor prioridad
3. **Timer dedicado**: Cuando se usa sincronización con timer, se crea exactamente un timer con ID 0
4. **Compatibilidad**: La función `create_scheduler()` mantiene compatibilidad con código anterior, usando Round Robin y sincronización con reloj global

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
