# Documentación del Sistema LocOS

Esta carpeta contiene la documentación técnica completa del sistema operativo LocOS.

## Documentos Esenciales

El proyecto se documenta en cuatro archivos principales que cubren todos los aspectos del sistema:

### 📘 [INFORME_TECNICO.md](INFORME_TECNICO.md) ⭐ INFORME COMPLETO
Informe técnico detallado con el diseño e implementación completa:
- Arquitectura del sistema con decisiones de diseño
- Planificador: algoritmos, políticas y sincronización
- Gestor de memoria: paginación, MMU, TLB y loader
- Integración de componentes y flujos completos
- Análisis de complejidad y conclusiones

### 📋 [ARQUITECTURA.md](ARQUITECTURA.md)
Describe la arquitectura general del sistema, incluyendo:
- Jerarquía de componentes (Kernel, Machine, CPU, Core, HardwareThread)
- Process Control Blocks (PCBs)
- Reloj del sistema y timers
- Generador de procesos
- Flujo de ejecución completo
- Parámetros de configuración
- Compilación y ejecución

### ⚙️ [SCHEDULER.md](SCHEDULER.md)  
Documenta el sistema de planificación de procesos:
- Modos de sincronización (reloj global vs timer dedicado)
- Políticas de planificación:
  - Round Robin (sin prioridades)
  - Brain Fuck Scheduler (BFS con virtual deadlines)
  - Preemptiva con prioridades estáticas (-20 a +19)
- Gestión de quantum
- Asignación de procesos a HardwareThreads
- Manejo de contexto de ejecución

### 💾 [MEMORIA.md](MEMORIA.md)
Explica el sistema de gestión de memoria virtual:
- Arquitectura de memoria física (16 MB, páginas de 4 KB)
- Paginación y tablas de páginas
- MMU (Memory Management Unit)
- TLB (Translation Lookaside Buffer)
- Traducción de direcciones virtuales a físicas
- Loader de programas
- Formato de archivos de programa
- Integración con HardwareThreads

## Guía de Lectura Recomendada

### Para evaluación académica o informe completo:
1. **INFORME_TECNICO.md** ⭐ - Diseño e implementación completa (45-60 min)

### Para entender el sistema rápidamente:
1. **ARQUITECTURA.md** - Visión general del sistema (15 min)
2. **SCHEDULER.md** - Cómo se planifican los procesos (10 min)
3. **MEMORIA.md** - Gestión de memoria virtual (15 min)

### Para implementación:
- Lee los tres documentos en orden
- Revisa el código fuente en `../sys/`
- Consulta los ejemplos de programas en `../programs/`

## Compilación Rápida

```bash
cd ../sys
make clean
make
./kernel
```

## Ejecución con Parámetros

```bash
# Round Robin, 10 Hz, quantum 5
./kernel -f 10 -q 5 -policy 0 -sync 0

# BFS, 5 Hz, quantum 3, sincronización con timer
./kernel -f 5 -q 3 -policy 1 -sync 1

# Prioridades preemptivas, 10 Hz, quantum 2
./kernel -f 10 -q 2 -policy 2 -sync 0
```

## Estructura del Código Fuente

```
../sys/
├── kernel.c         → Main del kernel, integración
├── process.h/c      → PCB, colas, generador, scheduler
├── machine.h/c      → Machine, CPU, Core, HardwareThread
├── memory.h/c       → Memoria física y virtual
├── loader.h/c       → Cargador de programas
├── clock_sys.h/c    → Reloj del sistema
├── timer.h/c        → Timers de interrupción
└── Makefile         → Compilación
```

## Programas de Ejemplo

```
../programs/
├── prog000.elf          → Programa en formato prometheus
├── prog001.elf          → Otro programa de ejemplo
├── generar_programas.sh → Script para generar programas
└── prometheus/          → Directorio con compilador prometheus
```

Formato de programa (.elf):
```
.text <dirección_hex>   # Inicio segmento código
.data <dirección_hex>   # Inicio segmento datos
<instrucción_hex>       # Instrucciones (8 dígitos hex/línea)
...
```

## Autor

Laura Rodríguez  
Universidad: LocOS Educational Project

## Licencia

Ver archivo LICENSE en la raíz del proyecto.
