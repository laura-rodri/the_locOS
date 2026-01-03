# Cómo Usar la Fase 2: Motor de Ejecución

## 🚀 Inicio Rápido

### 1. Compilar el Sistema

```bash
cd sys/
make clean
make
```

Deberías ver:
```
gcc -Wall -Wextra -pthread -g -c kernel.c
gcc -Wall -Wextra -pthread -g -c machine.c
...
gcc -Wall -Wextra -pthread -g -o kernel kernel.o machine.o ...
```

✅ **Compilación exitosa** (algunos warnings son normales)

---

## 🎯 Opción 1: Ejecutar con kernel (Sistema Completo)

### Estado Actual

El `kernel` actual usa un **ProcessGenerator** que crea procesos sin memoria física, por lo que **aún no ejecuta instrucciones reales**.

### Para habilitar la ejecución de instrucciones:

Necesitas modificar `sys/kernel.c` para:
1. Crear una `PhysicalMemory`
2. Crear un `Loader`
3. Llamar a `set_clock_physical_memory(pm)`
4. Cargar programas desde archivos

**Código a añadir en kernel.c (después de crear la Machine):**

```c
// Crear memoria física
PhysicalMemory* pm = create_physical_memory();
if (!pm) {
    fprintf(stderr, "Failed to create physical memory\n");
    return 1;
}

// Configurar memoria en el clock para ejecución
set_clock_physical_memory(pm);

// Crear loader
Loader* loader = create_loader(pm, ready_queue_global, 
                               machine_global, scheduler_global);

// Cargar un programa de prueba
Program* prog = load_program_from_file(loader, 
                                       "programs/suma_simple.txt");
if (prog) {
    printf("Programa cargado exitosamente\n");
    destroy_program(prog);
}
```

---

## 🧪 Opción 2: Usar test_memory (Recomendado para pruebas)

El programa `test_memory` ya está listo para probar el motor de ejecución.

### Compilar test_memory

```bash
cd sys/
make test_memory
```

### Ejecutar con programa de prueba

```bash
./test_memory programs/suma_simple.txt
```

### Salida esperada

```
Loading program from programs/suma_simple.txt...
Program loaded: suma_simple
  Code size: 5 words
  Data size: 4 words
  Priority: 0
  TTL: 20

Creating process from program...
Process 0: Requires 3 pages (code=2, data=1)
Process created successfully (PID=0)

Executing instructions:
PC=0x000000: Instruction=0x09000014   [LD] r9 = [0x000014] = 0x00000005
PC=0x000004: Instruction=0x0A000018   [LD] r10 = [0x000018] = 0x00000003
PC=0x000008: Instruction=0x2B9A0000   [ADD] r11 = r9 + r10 = 5 + 3 = 8
PC=0x00000C: Instruction=0x1B00001C   [ST] [0x00001C] = r11 = 0x00000008
PC=0x000010: Instruction=0xF0000000   [EXIT] Hardware thread terminating

Verification:
  r9  = 5 (expected: 5) ✓
  r10 = 3 (expected: 3) ✓
  r11 = 8 (expected: 8) ✓
  [0x00001C] = 8 (expected: 8) ✓

Test PASSED!
```

---

## 📝 Crear tus propios programas

### Formato del archivo

Crea un archivo `.txt` en `sys/programs/`:

```
.program <nombre>
.code_size <número_de_palabras_de_código>
.data_size <número_de_palabras_de_datos>
.entry_point <dirección_de_inicio>
.priority <prioridad_0-19>
.ttl <tiempo_de_vida_en_ticks>
.text <dirección_virtual_inicial>
<instrucción_hex_1>
<instrucción_hex_2>
...
.data <dirección_virtual_inicial>
<dato_hex_1>
<dato_hex_2>
...
```

### Ejemplo: Multiplicación por 2 (usando sumas)

Archivo: `sys/programs/mult_por_2.txt`

```
.program mult_por_2
.code_size 4
.data_size 3
.entry_point 0
.priority 0
.ttl 15
.text 0x000000
09000010
2AAA0000
1A000014
F0000000
.data 0x000010
00000007
00000000
00000000
```

**Explicación:**
```
09000010  → LD r9, [0x000010]     # r9 = 7
2AAA0000  → ADD r10, r9, r9       # r10 = r9 + r9 = 14
1A000014  → ST r10, [0x000014]    # [0x000014] = 14
F0000000  → EXIT
```

---

## 🔍 Instrucciones Disponibles

### LD (Load) - Opcode 0

Carga un valor de memoria a un registro.

**Formato:** `0RAAAAAA`
- R = Registro destino (1 dígito hex)
- A = Dirección virtual (6 dígitos hex)

**Ejemplo:**
```
0500ABCD  → LD r5, [0x00ABCD]
```

### ST (Store) - Opcode 1

Guarda un valor de registro en memoria.

**Formato:** `1RAAAAAA`
- R = Registro fuente (1 dígito hex)
- A = Dirección virtual (6 dígitos hex)

**Ejemplo:**
```
1700ABCD  → ST r7, [0x00ABCD]
```

### ADD - Opcode 2

Suma dos registros y guarda en un tercero.

**Formato:** `2RXY0000`
- R = Registro destino (1 dígito hex)
- X = Registro fuente 1 (1 dígito hex)
- Y = Registro fuente 2 (1 dígito hex)

**Ejemplo:**
```
23120000  → ADD r3, r1, r2  # r3 = r1 + r2
```

### EXIT - Opcode F

Termina la ejecución del proceso.

**Formato:** `F0000000`

**Ejemplo:**
```
F0000000  → EXIT
```

---

## 🛠️ Codificación Manual

### Herramienta de ayuda (Python)

```python
def encode_ld(reg, addr):
    return f"{0:01X}{reg:01X}{addr:06X}"

def encode_st(reg, addr):
    return f"{1:01X}{reg:01X}{addr:06X}"

def encode_add(dest, src1, src2):
    return f"{2:01X}{dest:01X}{src1:01X}{src2:01X}0000"

def encode_exit():
    return "F0000000"

# Ejemplos
print(encode_ld(9, 0x14))      # 09000014
print(encode_st(11, 0x1C))     # 1B00001C
print(encode_add(11, 9, 10))   # 2B9A0000
print(encode_exit())           # F0000000
```

---

## 🎓 Ejercicios Sugeridos

### 1. Suma de 3 números

Cargar tres números de memoria, sumarlos, y guardar el resultado.

**Números:** 10, 20, 30  
**Resultado esperado:** 60

### 2. Intercambio de variables

Intercambiar los valores de dos posiciones de memoria usando registros temporales.

**Inicial:** A=5, B=3  
**Final:** A=3, B=5

### 3. Acumulador

Cargar varios números y acumular su suma en un registro.

**Números:** 1, 2, 3, 4, 5  
**Resultado esperado:** 15

---

## 📊 Debugging

### Ver direcciones de memoria

Si modificas `test_memory.c`, puedes añadir:

```c
// Después de ejecutar las instrucciones
printf("\nMemory dump:\n");
for (int i = 0; i < 32; i += 4) {
    uint32_t val = mmu_read_word(pm, page_table, i);
    printf("  [0x%06X] = 0x%08X\n", i, val);
}
```

### Ver estado de registros

```c
printf("\nRegisters:\n");
for (int i = 0; i < 16; i++) {
    printf("  r%d = 0x%08X (%d)\n", 
           i, hw_thread->registers[i], 
           (int32_t)hw_thread->registers[i]);
}
```

---

## ⚠️ Limitaciones Actuales

- ✅ Solo 4 instrucciones implementadas (LD, ST, ADD, EXIT)
- ✅ No hay saltos condicionales (JMP, BEQ, etc.)
- ✅ No hay instrucciones aritméticas adicionales (SUB, MUL, etc.)
- ✅ TLB no se usa (traducción directa por tabla de páginas)
- ✅ Sin manejo de interrupciones
- ✅ Sin syscalls

---

## 📚 Referencias

- **FASE2_RESUMEN.md** - Documentación técnica completa
- **FASE2_TESTING.md** - Guía detallada de testing
- **FASE2_FINAL.md** - Resumen final del proyecto

---

## 🆘 Soporte

**Problemas comunes:**

1. **"Page fault"**
   - El loader debe crear la tabla de páginas
   - Verifica que `PTBR` esté configurado

2. **"Invalid memory address"**
   - Las direcciones deben estar dentro del rango cargado
   - Código: 0x000000 - (code_size * 4)
   - Datos: después del código

3. **Instrucción no se ejecuta**
   - Verifica formato hexadecimal correcto
   - Debe tener exactamente 8 dígitos hex

---

✨ **¡Listo para ejecutar programas en memoria virtual!** ✨
