# Guía de Prueba - Fase 2: Ejecución de Instrucciones

## Programa de Prueba Simple

Para verificar que el motor de ejecución funciona correctamente, puedes crear un programa de prueba simple.

### Formato del Programa

Los programas tienen el siguiente formato:
```
.program <nombre>
.code_size <num_palabras>
.data_size <num_palabras>
.entry_point <dirección_virtual>
.priority <prioridad>
.ttl <tiempo_de_vida>
.text <dirección_inicio>
<instrucción_1>
<instrucción_2>
...
.data <dirección_inicio>
<dato_1>
<dato_2>
...
```

### Ejemplo de Programa: suma_simple.txt

Crea el archivo `sys/programs/suma_simple.txt`:

```
.program suma_simple
.code_size 5
.data_size 4
.entry_point 0
.priority 0
.ttl 20
.text 0x000000
09000014
0A000018
2B9A0000
1B00001C
F0000000
.data 0x000014
00000005
00000003
00000000
00000000
```

### Explicación del Programa

**Instrucciones (código):**
1. `09000014` - `LD r9, [0x000014]`
   - Carga en r9 el valor de la dirección virtual 0x000014
   - r9 = 5

2. `0A000018` - `LD r10, [0x000018]`
   - Carga en r10 el valor de la dirección virtual 0x000018
   - r10 = 3

3. `2B9A0000` - `ADD r11, r9, r10`
   - Suma r9 + r10 y guarda en r11
   - r11 = 5 + 3 = 8

4. `1B00001C` - `ST r11, [0x00001C]`
   - Guarda el valor de r11 en la dirección virtual 0x00001C
   - [0x00001C] = 8

5. `F0000000` - `EXIT`
   - Termina la ejecución

**Datos:**
- `0x000014`: 00000005 (5 decimal)
- `0x000018`: 00000003 (3 decimal)
- `0x00001C`: 00000000 (resultado, inicialmente 0)
- `0x000020`: 00000000 (no usado)

### Mapa de Memoria Virtual

```
Dirección Virtual    Contenido              Descripción
-----------------    ---------              -----------
0x000000             09000014               Instrucción 1: LD r9, [0x14]
0x000004             0A000018               Instrucción 2: LD r10, [0x18]
0x000008             2B9A0000               Instrucción 3: ADD r11, r9, r10
0x00000C             1B00001C               Instrucción 4: ST r11, [0x1C]
0x000010             F0000000               Instrucción 5: EXIT
0x000014             00000005               Dato: 5
0x000018             00000003               Dato: 3
0x00001C             00000000 -> 00000008   Resultado (5 + 3)
0x000020             00000000               No usado
```

### Salida Esperada

Al ejecutar este programa, deberías ver en la consola:

```
PC=0x000000: Instruction=0x09000014   [LD] r9 = [0x000014] = 0x00000005
PC=0x000004: Instruction=0x0A000018   [LD] r10 = [0x000018] = 0x00000003
PC=0x000008: Instruction=0x2B9A0000   [ADD] r11 = r9 + r10 = 5 + 3 = 8 (0x00000008)
PC=0x00000C: Instruction=0x1B00001C   [ST] [0x00001C] = r11 = 0x00000008
PC=0x000010: Instruction=0xF0000000   [EXIT] Hardware thread terminating
```

## Cómo Compilar y Ejecutar

### Opción 1: Usando test_memory

El programa `test_memory` ya está configurado para cargar programas. Solo necesitas:

```bash
cd sys/
make test_memory
./test_memory programs/suma_simple.txt
```

### Opción 2: Modificar kernel.c para usar Loader

Para integrar el Loader con el kernel completo, necesitarías:

1. Crear una PhysicalMemory en el main
2. Crear un Loader
3. Cargar programas desde archivos en lugar de usar ProcessGenerator
4. Llamar a `set_clock_physical_memory()` para que el clock ejecute instrucciones

Ejemplo de código a añadir en kernel.c:

```c
// Después de crear la Machine
PhysicalMemory* pm = create_physical_memory();
if (!pm) {
    fprintf(stderr, "Failed to create physical memory\n");
    // cleanup...
    return 1;
}

// Configurar memoria física en el clock para ejecución de instrucciones
set_clock_physical_memory(pm);

// Crear loader
Loader* loader = create_loader(pm, ready_queue_global, machine_global, scheduler_global);
if (!loader) {
    fprintf(stderr, "Failed to create loader\n");
    // cleanup...
    return 1;
}

// Cargar programa
Program* prog = load_program_from_file(loader, "programs/suma_simple.txt");
if (prog) {
    printf("Program loaded successfully\n");
    destroy_program(prog);
}
```

## Decodificación Manual de Instrucciones

Para crear tus propios programas, usa esta tabla:

### Formato de Instrucciones

**LD (Load) - Opcode 0:**
```
0RAAAAAA
│└─────┴─ Dirección virtual (24 bits)
└──────── Registro destino (4 bits)
```

**ST (Store) - Opcode 1:**
```
1RAAAAAA
│└─────┴─ Dirección virtual (24 bits)
└──────── Registro fuente (4 bits)
```

**ADD - Opcode 2:**
```
2RXY0000
│││
││└───── Registro fuente 2 (4 bits)
│└────── Registro fuente 1 (4 bits)
└─────── Registro destino (4 bits)
```

**EXIT - Opcode F:**
```
F0000000
```

### Ejemplos de Codificación

1. `LD r5, [0x000100]` = `05000100`
   - Opcode: 0
   - Registro: 5
   - Dirección: 0x000100

2. `ST r7, [0x000200]` = `17000200`
   - Opcode: 1
   - Registro: 7
   - Dirección: 0x000200

3. `ADD r3, r1, r2` = `23120000`
   - Opcode: 2
   - Destino: 3
   - Src1: 1
   - Src2: 2

4. `EXIT` = `F0000000`

## Verificación de Resultados

Para verificar que el programa funcionó correctamente:

1. El proceso debe terminar con estado TERMINATED
2. La memoria en la dirección 0x00001C debe contener 0x00000008
3. Los registros r9, r10, r11 deben contener 5, 3, 8 respectivamente
4. El PC final debe quedar en 0x000010 (dirección del EXIT)

## Troubleshooting

**Si ves "Page fault":**
- Verifica que el loader haya creado correctamente la tabla de páginas
- Asegúrate de que todas las páginas necesarias estén marcadas como present

**Si las instrucciones no se ejecutan:**
- Verifica que `set_clock_physical_memory()` se haya llamado
- Asegúrate de que el PTBR esté configurado en el HardwareThread
- Verifica que el PCB esté en estado RUNNING, no TERMINATED

**Si los valores son incorrectos:**
- Verifica el formato hexadecimal de las instrucciones
- Asegúrate de que las direcciones virtuales apunten a los datos correctos
- Recuerda que las direcciones son en bytes, no en palabras

---

**Fase 2 - Guía de Pruebas** ✅
