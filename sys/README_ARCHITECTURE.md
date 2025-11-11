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

### 4. **timer.h/c** - Timers de Interrupción
Timers que generan interrupciones cada N ticks:
- Intervalo configurable
- Se sincronizan con el reloj global
- Útiles para scheduling y eventos periódicos

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
- `-t <ticks>`: Intervalo de interrupción de timers (default: 5)
- `-pmin <ticks>`: Intervalo mínimo de generación de procesos (default: 3)
- `-pmax <ticks>`: Intervalo máximo de generación de procesos (default: 10)
- `-pcount <num>`: Número máximo de procesos a generar, 0=ilimitado (default: 20)
- `-qsize <num>`: Tamaño de la cola de procesos listos (default: 100)

### Ejemplos
```bash
# Ejecutar con configuración por defecto
./kernel

# Reloj a 2 Hz, timers cada 4 ticks, generar hasta 10 procesos
./kernel -f 2 -t 4 -pcount 10

# Reloj rápido (10 Hz), procesos frecuentes
./kernel -f 10 -pmin 1 -pmax 3

# Generación ilimitada de procesos
./kernel -pcount 0
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
2. **Clock** notifica a todos los componentes (broadcast)
3. **Timers** verifican si deben generar una interrupción
4. **Process Generator** verifica si debe crear un nuevo proceso
5. Si toca generar:
   - Crea un nuevo PCB con PID único
   - Lo añade a la Ready Queue
   - Calcula el próximo tiempo de generación (aleatorio)
6. Los procesos quedan en Ready Queue esperando al scheduler

## 📊 Estados de Proceso

- `RUNNING (0)`: Proceso en ejecución
- `WAITING (1)`: Proceso esperando (recién creado)
- `TERMINATED (2)`: Proceso terminado

## 🧪 Testing

Para probar el Process Generator durante 10 segundos:
```bash
timeout 10 ./kernel -f 2 -t 4 -pmin 2 -pmax 5 -pcount 10
```

Deberías ver:
- Ticks del reloj cada 0.5 segundos (2 Hz)
- Interrupciones de timers cada 4 ticks
- Creación de procesos cada 2-5 ticks (aleatorio)
- Máximo 10 procesos generados

## 📝 Notas

- El archivo `clock.c` original se mantiene para referencia
- Los warnings de compilación sobre `struct PCB *` vs `PCB *` son menores y no afectan la funcionalidad
- Presiona Ctrl+C para terminar el sistema limpiamente