# the_locOS

Bienvenido a the_locOS, un sistema operativo tan “de locos” que hasta él mismo duda de su cordura.  
the_locOS es un lunático digital: un kernel inquieto, multihilo, un tanto impredecible, que no descansa mientras coordina procesos como si fuera un manicomio perfectamente organizado.

Solo los locos consiguen que todo funcione sin colgarse.


## Schedulers
Cualquier scheduler que se elija puede ser activado o bien por el reloj del sistema, o bien por la interrupción de un timer - configurable por el usuario.
#### Round Robin sin prioridades:  
Por defecto, the_locOS utiliza un scheduler Round Robin sin prioridades.  
Se asume que el hecho de cambiar de contexto es instantáneo y no consume tiempo. Cuando un proceso termina su ejecución de forma natural, se elige inmediatamente el siguiente proceso a ejecutar. Cuando el quantum de un proceso expira, el scheduler vuelve a elegir un proceso.

#### Brain fuck scheduler (BFS):  
Originalmente creado por Con Kolivas, tiene una única cola global donde a cada tarea se le asigna un deadline. Para decidir qué tarea ejecutar a continuación, BFS escanea la lista de tareas en busca de la que tenga la fecha límite más cercana.

#### Expulsora por eventos con prioridades:   
> Nota: En una configuración donde el scheduler solo se activa con el Timer, no es verdaderamente "expulsor por evento", sino "expulsor por tiempo" (Time-Sharing).  
Se perderán más o menos ciclos dependiendo de cuándo se genere la interrupción del Timer.

#### Schedulers divertidos y poco útiles (para un futuro próximo):  
**El juego de la patata caliente:** los procesos están sentados en un círculo, y cada uno tiene un quantum variable. En un intervalo de tiempo indefinido, la patata explotará y matará al proceso que la tenga en ese instante. El juego terminará cuando todos mueran o ya no queden más patatas.

**Hundir la flota:** si un proceso adivina correctamente una coordenada, puede seguir ejecutándose. Si falla, pierde su turno.  

## Memoria
Memoria virtual con paginación. La MMU traduce direcciones virtuales a físicas usando tablas de páginas, con una TLB de 16 entradas que acelera las traducciones mediante caché.

#### Configuración:
- Bus de direcciones: 24 bits (16 MB)
- Tamaño de página: 4 KB (4096 marcos totales)
- Espacio kernel: 1 MB (256 marcos) - para tablas de páginas
- Espacio usuario: 15 MB (3840 marcos) - para procesos

Los programas se generan con **prometheus** en formato `.elf` y se cargan mediante el loader. **heracles** es una utilidad para verificar la correcta decodificación de los archivos `.elf`, pero no se usa en el simulador.


## Compilación Rápida

```bash
cd sys/
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

## Creación de Programas

```bash
cd programs/
./generar_programas.sh 2 10 40 #Genera 2 programas con entre 10 y 40 instrucciones
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

## Autora

Laura Rodríguez  
UPV/EHU Universidad del País Vasco

## Licencia

Ver archivo LICENSE en la raíz del proyecto.
