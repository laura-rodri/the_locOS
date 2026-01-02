# Guía de Uso - Fase 1: Memoria Virtual

## Compilación

### Compilar el kernel completo:
```bash
cd sys
make clean
make
```

### Compilar el programa de prueba:
```bash
cd sys
make clean
make test
```

## Ejecución

### Ejecutar el test de memoria:
```bash
cd sys
./test_memory
```

Este programa de prueba demuestra:
- Creación de memoria física (16 MB)
- Creación de tablas de páginas
- Asignación de marcos de memoria
- Lectura/escritura de palabras
- Carga de programas desde archivos
- Configuración de PCB con información de memoria

### Salida esperada:
```
=== Memory Virtual - Fase 1: Test de Estructuras ===

1. Creando memoria física...
Physical Memory initialized:
  Total size: 16777216 bytes (4194304 words)
  Kernel space: 1048576 bytes (262144 words, 256 frames)
  User space: 15728640 bytes (3932160 words, 3840 frames)
  ...

2. Probando creación de tabla de páginas...
   ✓ Tabla de páginas creada (10 páginas)
   ...

3. Probando asignación de marcos...
   Marco asignado 0: 256
   ...

4. Probando lectura/escritura de memoria...
   ✓ Lectura/escritura correcta

5. Probando carga de programa desde archivo...
   ✓ Programa cargado
   ...

6. Probando creación de PCB con tabla de páginas...
   ✓ PCB configurado con tabla de páginas
   ...

=== Test completado exitosamente ===
```

## Estructura de Programas

Los programas se almacenan en `sys/programs/` como archivos de texto con el siguiente formato:

```
PROGRAM nombre_programa
CODE_SIZE 8
DATA_SIZE 4
ENTRY_POINT 0
PRIORITY 0
TTL 10
CODE_SECTION
00000001
00000002
...
DATA_SECTION
0000000A
0000000B
...
```

### Programas de ejemplo incluidos:
- `simple_add.txt`: Programa básico (8 palabras código, 4 datos)
- `high_priority.txt`: Programa con prioridad alta (16 palabras código, 8 datos)
- `loop_test.txt`: Programa de prueba de bucles (12 palabras código, 6 datos)

## Nuevos Archivos

### Archivos de cabecera:
- `memory.h`: Definiciones de memoria física, marcos, tablas de páginas
- `loader.h`: Definiciones del loader de programas

### Archivos de implementación:
- `memory.c`: Implementación de memoria física y gestión de marcos
- `loader.c`: Implementación del loader de programas

### Archivos modificados:
- `process.h`: Añadido campo `MemoryManagement mm` al PCB
- `process.c`: Inicialización del campo `mm` en `create_pcb()`
- `machine.h`: Añadidas estructuras `HardwareThread`, `TLB`, `MMU`
- `machine.c`: Inicialización de hardware threads
- `Makefile`: Añadidos nuevos targets para compilación

### Archivos de prueba:
- `test_memory.c`: Programa de prueba independiente

## Uso del Loader en el Kernel (Futuro)

Aunque en esta fase no se ejecutan programas, el loader está listo para usarse:

```c
// Crear memoria física
PhysicalMemory* pm = create_physical_memory();

// Crear loader
Loader* loader = create_loader(pm, ready_queue, machine, scheduler);

// Cargar un programa
PCB* pcb = load_and_create_process(loader, "programs/simple_add.txt");

// El PCB ahora tiene:
// - pcb->mm.code: dirección virtual del código
// - pcb->mm.data: dirección virtual de los datos
// - pcb->mm.pgb: puntero a la tabla de páginas
```

## Próximos Pasos (Fase 2)

En la Fase 2 se implementará:
1. Traducción de direcciones virtuales a físicas usando la MMU
2. Búsqueda y actualización de TLB
3. Ciclo fetch-decode-execute con traducción
4. Manejo de page faults
5. Context switching con actualización de PTBR

## Verificación

Para verificar que todo funciona:

1. Compilar sin errores: `make clean && make`
2. Compilar test: `make clean && make test`
3. Ejecutar test: `./test_memory`
4. Verificar salida exitosa

## Documentación Adicional

Ver `MEMORY_VIRTUAL_README.md` para detalles técnicos completos de:
- Arquitectura de memoria
- Estructuras de datos
- Formato de entrada de tabla de páginas
- Diagramas de arquitectura

## Notas

- La memoria física se simula como un array de 4,194,304 palabras (16 MB)
- Los primeros 256 marcos están reservados para el kernel
- Los marcos de usuario comienzan en el marco 256
- Las tablas de páginas se almacenan en el espacio del kernel
- Cada página/marco es de 4 KB (1024 palabras)
