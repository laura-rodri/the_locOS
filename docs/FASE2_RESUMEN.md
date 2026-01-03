# Fase 2: Motor de Ejecución y Memoria Virtual - Implementación Completa

## Resumen de la Implementación

La Fase 2 ha sido completamente implementada, dotando de "vida" a la simulación mediante el ciclo de instrucción y la traducción de direcciones virtuales a físicas.

## Componentes Implementados

### 1. MMU (Memory Management Unit) - Traducción de Direcciones

**Archivos modificados:**
- `sys/memory.h` - Declaraciones de funciones MMU
- `sys/memory.c` - Implementación de la MMU

**Funciones implementadas:**

#### `translate_virtual_to_physical()`
- **Entrada:** Dirección virtual (24 bits)
- **Salida:** Dirección física (en palabras)
- **Proceso:**
  1. Extrae el número de página virtual (bits superiores)
  2. Extrae el offset (12 bits inferiores - 4KB por página)
  3. Consulta la tabla de páginas
  4. Verifica el bit `present`
  5. Calcula la dirección física: `(frame_number << 12) | offset`
  6. Convierte de bytes a palabras

#### `mmu_read_word()`
- Lee una palabra de memoria usando dirección virtual
- Traduce la dirección virtual a física
- Accede a la memoria física

#### `mmu_write_word()`
- Escribe una palabra en memoria usando dirección virtual
- Traduce la dirección virtual a física
- Marca la página como `dirty` y `accessed`
- Escribe en la memoria física

### 2. Banco de Registros

**Archivos modificados:**
- `sys/machine.h` - Añadido array de 16 registros a `HardwareThread`
- `sys/machine.c` - Inicialización de registros a 0

**Estructura:**
```c
typedef struct {
    uint32_t PC;            // Program Counter
    uint32_t IR;            // Instruction Register
    void* PTBR;             // Page Table Base Register
    uint32_t registers[16]; // r0-r15 (general purpose)
    MMU mmu;
    TLB tlb;
    PCB* pcb;
} HardwareThread;
```

### 3. Decodificador de Instrucciones

**Archivo:** `sys/machine.c`

**Funciones auxiliares implementadas:**

```c
// Formato de instrucción: 32 bits en hexadecimal
// CRAAAAAA - C=Opcode(4), R=Registro(4), A=Dirección(24)
// CRRR---- - C=Opcode(4), R=Dest(4), R=Src1(4), R=Src2(4)
// C------- - C=Opcode(4)

uint8_t extract_opcode(uint32_t instruction);      // Bits 31-28
uint8_t extract_reg(uint32_t instruction);          // Bits 27-24
uint32_t extract_address(uint32_t instruction);     // Bits 23-0
uint8_t extract_reg_src1(uint32_t instruction);     // Bits 23-20
uint8_t extract_reg_src2(uint32_t instruction);     // Bits 19-16
```

### 4. Juego de Instrucciones (4 instrucciones)

#### **LD (Load) - Opcode 0x0**
- **Formato:** `0RAAAAAA`
- **Acción:** `R = [Address]`
- **Implementación:**
  - Extrae registro y dirección virtual
  - Usa MMU para leer de memoria virtual
  - Carga el valor en el registro

```c
void execute_ld(HardwareThread* hw_thread, PhysicalMemory* pm, uint32_t instruction)
```

#### **ST (Store) - Opcode 0x1**
- **Formato:** `1RAAAAAA`
- **Acción:** `[Address] = R`
- **Implementación:**
  - Extrae registro y dirección virtual
  - Lee el valor del registro
  - Usa MMU para escribir en memoria virtual

```c
void execute_st(HardwareThread* hw_thread, PhysicalMemory* pm, uint32_t instruction)
```

#### **ADD - Opcode 0x2**
- **Formato:** `2RXY----`
- **Acción:** `R = X + Y` (enteros con signo, complemento a 2)
- **Implementación:**
  - Extrae los 3 registros (destino, fuente1, fuente2)
  - Lee los operandos como enteros con signo
  - Realiza la suma
  - Guarda el resultado en registro destino

```c
void execute_add(HardwareThread* hw_thread, uint32_t instruction)
```

#### **EXIT - Opcode 0xF**
- **Formato:** `F-------`
- **Acción:** Detiene la ejecución del hilo
- **Implementación:**
  - Marca el PCB como `TERMINATED`
  - No incrementa el PC

```c
void execute_exit(HardwareThread* hw_thread)
```

### 5. Ciclo de Instrucción (Fetch-Decode-Execute-Update)

**Función principal:** `execute_instruction_cycle()`

**Archivo:** `sys/machine.c`

**Proceso:**

```
1. FETCH:
   - Lee la instrucción desde PC (dirección virtual)
   - Usa MMU para traducir PC a dirección física
   - Carga la instrucción en IR

2. DECODE:
   - Extrae el opcode (4 bits superiores)
   - Identifica la instrucción

3. EXECUTE:
   - Ejecuta la operación correspondiente:
     * LD: Carga desde memoria
     * ST: Guarda en memoria
     * ADD: Suma de registros
     * EXIT: Termina el hilo
   - Todas las operaciones de memoria usan la MMU

4. UPDATE PC:
   - PC = PC + 4 (siguiente instrucción)
   - Excepto para EXIT (no incrementa)
```

### 6. Integración con el Clock

**Archivos modificados:**
- `sys/clock_sys.h` - Añadida referencia a `PhysicalMemory`
- `sys/clock_sys.c` - Modificada función `clock_function()`

**Comportamiento:**

En cada tick del reloj:
1. Decrementa el TTL de cada proceso en ejecución
2. **NUEVO:** Ejecuta un ciclo de instrucción para cada Hardware Thread activo
3. Itera sobre todos los CPUs, Cores y Hardware Threads
4. Solo ejecuta si:
   - Hay un PCB asignado
   - El PCB no está en estado TERMINATED
   - La memoria física está disponible

**Nueva función:**
```c
void set_clock_physical_memory(PhysicalMemory* pm);
```

## Ejemplo de Ejecución

Para el programa de ejemplo `prog000.elf`:

```
.text 000000
.data 000014
09000028    <- LD r9, [0x000028]   # r9 = valor en dirección 0x28
0A00002C    <- LD r10, [0x00002C]  # r10 = valor en dirección 0x2C
2B9A0000    <- ADD r11, r9, r10    # r11 = r9 + r10
1B000030    <- ST r11, [0x000030]  # [0x30] = r11
F0000000    <- EXIT                # Terminar
```

**Salida esperada por consola:**
```
PC=0x000000: Instruction=0x09000028   [LD] r9 = [0x000028] = 0x00000005
PC=0x000004: Instruction=0x0A00002C   [LD] r10 = [0x00002C] = 0x00000003
PC=0x000008: Instruction=0x2B9A0000   [ADD] r11 = r9 + r10 = 5 + 3 = 8
PC=0x00000C: Instruction=0x1B000030   [ST] [0x000030] = r11 = 0x00000008
PC=0x000010: Instruction=0xF0000000   [EXIT] Hardware thread terminating
```

## Características Implementadas

### ✅ Traducción de direcciones virtuales a físicas
- Soporte para páginas de 4KB
- Manejo de bits present, dirty, accessed
- Verificación de page faults

### ✅ Ciclo de instrucción completo
- Fetch usando MMU
- Decode con operaciones bitwise
- Execute según opcode
- Update PC automático

### ✅ Juego de instrucciones funcional
- LD: Lectura desde memoria virtual
- ST: Escritura a memoria virtual
- ADD: Aritmética con signo
- EXIT: Terminación de hilos

### ✅ Integración con el sistema
- Clock ejecuta instrucciones en cada tick
- Soporte multi-core y multi-threading
- Compatible con el Loader de Fase 1

## Archivos Modificados

1. **sys/memory.h** - Declaraciones MMU
2. **sys/memory.c** - Implementación MMU
3. **sys/machine.h** - Registros y funciones de ejecución
4. **sys/machine.c** - Ciclo de instrucción completo
5. **sys/clock_sys.h** - Referencia a memoria física
6. **sys/clock_sys.c** - Integración de ejecución en clock

## Compilación

```bash
cd sys/
make clean
make
```

**Estado:** ✅ Compila sin errores

## Próximos Pasos (Fase 3)

- Implementar más instrucciones (SUB, MUL, JMP, etc.)
- Optimizar con TLB (actualmente no se usa)
- Implementar manejo de page faults
- Añadir estadísticas de rendimiento
- Implementar swap de memoria

## Notas Técnicas

- Las direcciones virtuales son de 24 bits
- El offset de página es de 12 bits (4096 bytes/página)
- Los números de página son de 12 bits (4096 páginas máximo)
- Los registros son de 32 bits
- Las instrucciones son de 32 bits (formato hexadecimal)
- La memoria física tiene 16MB (4,194,304 palabras)
- Cada palabra tiene 4 bytes

---

**Fase 2 - COMPLETADA** ✅

Fecha de implementación: Enero 2026
