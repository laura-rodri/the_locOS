# Sistema Operativo LocOS - Arquitectura del Kernel

## 📁 Estructura de Archivos

```
sys/
|── process.h/c      → Gestión de procesos (PCB, colas, generador)
|── machine.h/c      → Arquitectura hardware (Machine → CPU → Core)
|── clock_sys.h/c    → Reloj global del sistema
|── timer.h/c        → Timers de interrupción periódica
|── kernel.c         → Main del kernel (integración)
|── clock.c          → [Versión original monolítica - referencia]
└── Makefile         → Compilación modular
```

## 🏗️ Arquitectura del Sistema

```
Kernel
│
|── System Clock (Global)
│   └── Timers (interrupciones periódicas)
│       |── Timer 0
│       └── Timer 1
│
|── Process Management
│   |── Ready Queue (ProcessQueue)
│   |── Process Generator (crea PCBs aleatoriamente)
│   └── PCBs (Process Control Blocks)
│
└── Machine (Hardware Virtual)
    └── CPUs[]
        └── Cores[]
            └── PCBs[] (kernel threads)
```

## 🔧 Módulos

### 1. **process.h/c** - Gestión de Procesos
Contiene toda la lógica relacionada con procesos:
- **PCB**: Process Control Block (pid, estado)
- **ProcessQueue**: Cola circular para gestionar procesos
- **ProcessGenerator**: Generador automático de procesos con frecuencia configurable

**Funciones principales:**
- `create_pcb()` / `destroy_pcb()`: Gestión de PCBs
- `create_process_queue()`: Crear cola de procesos
- `enqueue_process()` / `dequeue_process()`: Añadir/sacar procesos
- `create_process_generator()`: Crear generador
- `start_process_generator()`: Iniciar generación automática

### 2. **machine.h/c** - Arquitectura Hardware
Modela la jerarquía Machine → CPU → Core:
- **Machine**: Contiene múltiples CPUs
- **CPU**: Contiene múltiples Cores
- **Core**: Contiene PCBs de kernel threads

### 3. **clock_sys.h/c** - Reloj del Sistema
Reloj global que genera ticks periódicos:
- Frecuencia configurable (Hz)
- Sincronización mediante mutex y condition variables
- Notifica a todos los componentes (broadcast)
- **CRÍTICO**: El reloj del sistema decrementa el TTL de todos los procesos en ejecución
- Mantiene referencia a la Machine para acceder a los procesos en ejecución

### 4. **timer.h/c** - Timers de Interrupción
Timers que generan interrupciones cada N ticks:
- Intervalo configurable
- Se sincronizan con el reloj global
- Útiles para scheduling y eventos periódicos
- **Sistema de callbacks**: Los timers pueden ejecutar funciones callback al interrumpir
- Usados para activar el scheduler en modo SCHED_SYNC_TIMER

### 5. **kernel.c** - Integración
Main del kernel que inicializa y coordina todos los componentes.

## 🚀 Compilación y Ejecución

### Compilar
```bash
cd sys
make clean
make
```

### Ejecutar
```bash
./kernel [flags]
```

### Flags disponibles
- `-f <hz>`: Frecuencia del reloj en Hz (default: 1)
- `-q <ticks>`: Quantum del scheduler en ticks (default: 5)
- `-policy <num>`: Política de planificación (default: 0)
  - `0`: Round Robin sin prioridades
  - `1`: Brain Fuck Scheduler (BFS) con virtual deadlines
  - `2`: Política expulsora con prioridades estáticas (-20 a +19)
- `-sync <mode>`: Modo de sincronización del scheduler (default: 0)
  - `0`: Sincronización con reloj global (activación cada tick)
  - `1`: Sincronización con timer dedicado (activación cada quantum ticks)
- `-pmin <ticks>`: Intervalo mínimo de generación de procesos (default: 3)
- `-pmax <ticks>`: Intervalo máximo de generación de procesos (default: 10)
- `-tmin <ticks>`: TTL mínimo de procesos (default: 10)
- `-tmax <ticks>`: TTL máximo de procesos (default: 50)
- `-qsize <num>`: Tamaño de la cola de procesos listos (default: 100)
- `-cpus <num>`: Número de CPUs (default: 1)
- `-cores <num>`: Número de cores por CPU (default: 2)
- `-threads <num>`: Número de kernel threads por core (default: 4)

### Ejemplos
```bash
# Ejecutar con configuración por defecto (Round Robin + Clock)
./kernel

# Round Robin con timer, reloj a 2 Hz, quantum de 8 ticks
./kernel -f 2 -q 8 -policy 0 -sync 1

# BFS con reloj rápido (10 Hz), procesos frecuentes
./kernel -f 10 -policy 1 -pmin 1 -pmax 3

# Prioridades estáticas con expulsión por evento
./kernel -policy 2 -q 5 -f 2

# Sistema completo personalizado
./kernel -f 2 -q 10 -policy 2 -sync 1 -cpus 2 -cores 4 -threads 2 -tmin 20 -tmax 100
```

## 📍 Ubicación de Componentes

### Timers
Los timers están a **nivel de sistema** (no pertenecen a la jerarquía Machine→CPU→Core):
- Se sincronizan con el reloj global
- Generan interrupciones periódicas para el scheduler
- Simulan interrupciones de hardware

### Process Generator
El generador de procesos es un **componente del kernel**:
- Thread independiente que se despierta con cada tick del reloj
- Crea PCBs con intervalos aleatorios configurables
- Los añade a la Ready Queue
- Simula la llegada de nuevos procesos al sistema

### Ready Queue
Cola de procesos **listos para ejecutar**:
- Estructura FIFO circular
- Compartida entre el generador y el scheduler
- Los procesos esperan aquí a ser asignados a un Core

## 🔄 Flujo de Ejecución

1. **Clock** genera un tick cada 1/Hz segundos
2. **Clock** decrementa el TTL de todos los procesos en ejecución (CRÍTICO)
3. **Clock** notifica a todos los componentes (broadcast)
4. **Timers** verifican si deben generar una interrupción
5. **Timer del scheduler** (si sync_mode=TIMER) activa el scheduler con su callback
6. **Process Generator** verifica si debe crear un nuevo proceso
7. Si toca generar:
   - Crea un nuevo PCB con PID único y prioridad aleatoria
   - Lo añade a la Ready Queue (o cola de prioridad correspondiente)
   - Calcula el próximo tiempo de generación (aleatorio)
8. **Scheduler** se activa (por clock o por timer según configuración)
9. **Scheduler** verifica procesos completados (TTL=0) o quantum expirado
10. **Scheduler** asigna nuevos procesos desde las colas a los cores libres
11. Los procesos quedan en ejecución hasta que:
    - Su TTL llegue a 0 (completado)
    - Su quantum expire (vuelta a cola)
    - Sean expulsados por otro de mayor prioridad (solo policy=2)

## 📊 Estados de Proceso

- `RUNNING (0)`: Proceso en ejecución
- `WAITING (1)`: Proceso esperando (recién creado)
- `TERMINATED (2)`: Proceso terminado

## 🧪 Testing

Para probar el sistema durante 10 segundos:
```bash
# Round Robin con reloj global
timeout 10 ./kernel -f 2 -q 5 -policy 0 -sync 0

# BFS con timer
timeout 10 ./kernel -f 2 -q 8 -policy 1 -sync 1

# Prioridades con expulsión
timeout 10 ./kernel -f 3 -q 5 -policy 2 -sync 0 -cpus 1 -cores 1 -threads 1
```

Deberías ver:
- Ticks del reloj según la frecuencia configurada
- Decrementos de TTL realizados por el clock
- Interrupciones de timers (si sync=1)
- Creación de procesos con prioridades aleatorias
- Asignación y expulsión de procesos según la política
- Scheduler activándose según el modo de sincronización

Para ejecutar la suite completa de tests:
```bash
./Test.sh
```

## 📝 Notas

- El archivo `clock.c` original se mantiene para referencia
- **Separación de responsabilidades**: El reloj del sistema decrementa TTL, el scheduler gestiona asignaciones
- Los warnings de compilación sobre `struct PCB *` vs `PCB *` son menores y no afectan la funcionalidad
- El scheduler puede sincronizarse con el reloj global o con un timer dedicado
- Las prioridades van de -20 (mayor) a +19 (menor), con 40 niveles totales
- El modo timer evita activaciones innecesarias del scheduler en cada tick
- Presiona Ctrl+C para terminar el sistema limpiamente