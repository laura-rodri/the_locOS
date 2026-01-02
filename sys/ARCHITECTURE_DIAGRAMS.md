# Diagrama de Arquitectura - Memoria Virtual Fase 1

## Vista General del Sistema

```
┌─────────────────────────────────────────────────────────────────┐
│                         KERNEL                                   │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                  Physical Memory (16 MB)                    │ │
│  │  ┌──────────────────────┬──────────────────────────────┐   │ │
│  │  │   Kernel Space       │      User Space              │   │ │
│  │  │   (1 MB, 256 frames) │    (15 MB, 3840 frames)      │   │ │
│  │  │                      │                              │   │ │
│  │  │  ┌────────────────┐  │  ┌────────────────────────┐  │   │ │
│  │  │  │ Page Tables    │  │  │  Process Code/Data     │  │   │ │
│  │  │  │ (for all       │  │  │  Segments              │  │   │ │
│  │  │  │  processes)    │  │  │                        │  │   │ │
│  │  │  └────────────────┘  │  └────────────────────────┘  │   │ │
│  │  └──────────────────────┴──────────────────────────────┘   │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                         Loader                              │ │
│  │  - Reads programs from files                               │ │
│  │  - Creates PCBs with memory info                           │ │
│  │  - Allocates frames                                        │ │
│  │  - Loads code/data into memory                             │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                         MACHINE                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                          CPU 0                              │ │
│  │  ┌──────────────────────────────────────────────────────┐  │ │
│  │  │                       Core 0                          │  │ │
│  │  │  ┌────────────────────────────────────────────────┐  │  │ │
│  │  │  │         Hardware Thread 0                      │  │  │ │
│  │  │  │  ┌──────────────────────────────────────────┐  │  │  │ │
│  │  │  │  │ Registers:                               │  │  │  │ │
│  │  │  │  │  - PC  (Program Counter)                 │  │  │  │ │
│  │  │  │  │  - IR  (Instruction Register)            │  │  │  │ │
│  │  │  │  │  - PTBR (Page Table Base Register) ──────┼──┼──┼──┼─> Page Table
│  │  │  │  └──────────────────────────────────────────┘  │  │  │ │    in Kernel
│  │  │  │  ┌──────────────────────────────────────────┐  │  │  │ │    Space
│  │  │  │  │ MMU (Memory Management Unit):            │  │  │  │ │
│  │  │  │  │  - page_table_base ──────────────────────┼──┼──┼──┼─> Page Table
│  │  │  │  │  - enabled (1=on, 0=off)                 │  │  │  │ │
│  │  │  │  └──────────────────────────────────────────┘  │  │  │ │
│  │  │  │  ┌──────────────────────────────────────────┐  │  │  │ │
│  │  │  │  │ TLB (Translation Lookaside Buffer):      │  │  │  │ │
│  │  │  │  │  Entry 0: [VP:? PF:? Valid:0]            │  │  │  │ │
│  │  │  │  │  Entry 1: [VP:? PF:? Valid:0]            │  │  │  │ │
│  │  │  │  │  ...                                     │  │  │  │ │
│  │  │  │  │  Entry 15: [VP:? PF:? Valid:0]           │  │  │  │ │
│  │  │  │  └──────────────────────────────────────────┘  │  │  │ │
│  │  │  │  ┌──────────────────────────────────────────┐  │  │  │ │
│  │  │  │  │ PCB* ───────────────────────────────────────┼──┼──┼─> Current
│  │  │  │  └──────────────────────────────────────────┘  │  │  │    Process
│  │  │  └────────────────────────────────────────────────┘  │  │ │
│  │  │  │         Hardware Thread 1                         │  │ │
│  │  │  │         ...                                       │  │ │
│  │  │  └───────────────────────────────────────────────────┘  │ │
│  │  │                       Core 1                             │ │
│  │  │                       ...                                │ │
│  │  └──────────────────────────────────────────────────────────┘ │
│  │                          CPU 1                               │ │
│  │                          ...                                 │ │
│  └──────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                           PCB                                    │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ Process ID: 1                                              │ │
│  │ State: RUNNING                                             │ │
│  │ Priority: 0                                                │ │
│  │ TTL: 10                                                    │ │
│  │ ┌──────────────────────────────────────────────────────┐  │ │
│  │ │ MemoryManagement mm:                                 │  │ │
│  │ │  ┌────────────────────────────────────────────────┐  │  │ │
│  │ │  │ code ──> Virtual Address: 0x00000000          │  │  │ │
│  │ │  │          (Start of code segment)              │  │  │ │
│  │ │  └────────────────────────────────────────────────┘  │  │ │
│  │ │  ┌────────────────────────────────────────────────┐  │  │ │
│  │ │  │ data ──> Virtual Address: 0x00001000          │  │  │ │
│  │ │  │          (Start of data segment)              │  │  │ │
│  │ │  └────────────────────────────────────────────────┘  │  │ │
│  │ │  ┌────────────────────────────────────────────────┐  │  │ │
│  │ │  │ pgb  ──> Physical Address in Kernel Space     │  │  │ │
│  │ │  │          Points to Page Table:                │  │  │ │
│  │ │  │          ┌──────────────────────────────────┐ │  │  │ │
│  │ │  │          │ Page 0: Frame 256, R/O, Present │ │  │  │ │
│  │ │  │          │ Page 1: Frame 257, R/W, Present │ │  │  │ │
│  │ │  │          │ ...                             │ │  │  │ │
│  │ │  │          └──────────────────────────────────┘ │  │  │ │
│  │ │  └────────────────────────────────────────────────┘  │  │ │
│  │ └──────────────────────────────────────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## Flujo de Carga de un Programa

```
1. PROGRAMA EN ARCHIVO
   programs/simple_add.txt
   ┌──────────────────────┐
   │ PROGRAM simple_add   │
   │ CODE_SIZE 8          │
   │ DATA_SIZE 4          │
   │ PRIORITY 0           │
   │ TTL 10               │
   │ CODE_SECTION         │
   │ 00000001             │
   │ 00000002             │
   │ ...                  │
   │ DATA_SECTION         │
   │ 0000000A             │
   │ ...                  │
   └──────────────────────┘
           │
           ▼
2. LOADER LEE Y PARSEA
   ┌──────────────────────┐
   │ load_program_from_   │
   │ file()               │
   │  - Lee header        │
   │  - Lee code segment  │
   │  - Lee data segment  │
   └──────────────────────┘
           │
           ▼
3. CREA PCB
   ┌──────────────────────┐
   │ create_pcb()         │
   │  - PID asignado      │
   │  - Priority, TTL set │
   │  - mm inicializado   │
   └──────────────────────┘
           │
           ▼
4. CALCULA PÁGINAS NECESARIAS
   Code: 8 words * 4 bytes = 32 bytes → 1 page
   Data: 4 words * 4 bytes = 16 bytes → 1 page
   Total: 2 pages
           │
           ▼
5. CREA TABLA DE PÁGINAS EN KERNEL SPACE
   ┌──────────────────────────────────┐
   │ Kernel Space                     │
   │ ┌──────────────────────────────┐ │
   │ │ Page Table (2 entries)       │ │
   │ │ [0]: Frame=?, Present=0      │ │
   │ │ [1]: Frame=?, Present=0      │ │
   │ └──────────────────────────────┘ │
   └──────────────────────────────────┘
           │
           ▼
6. ASIGNA MARCOS Y CARGA DATOS
   ┌──────────────────────────────────┐
   │ User Space                       │
   │ ┌──────────────────────────────┐ │
   │ │ Frame 256 (Page 0 - Code)    │ │
   │ │ [0x00000001]                 │ │
   │ │ [0x00000002]                 │ │
   │ │ ...                          │ │
   │ └──────────────────────────────┘ │
   │ ┌──────────────────────────────┐ │
   │ │ Frame 257 (Page 1 - Data)    │ │
   │ │ [0x0000000A]                 │ │
   │ │ [0x0000000B]                 │ │
   │ │ ...                          │ │
   │ └──────────────────────────────┘ │
   └──────────────────────────────────┘
           │
           ▼
7. ACTUALIZA TABLA DE PÁGINAS
   ┌──────────────────────────────────┐
   │ Page Table (actualized)          │
   │ [0]: Frame=256, RO, Present=1    │
   │ [1]: Frame=257, RW, Present=1    │
   └──────────────────────────────────┘
           │
           ▼
8. ACTUALIZA PCB.mm
   ┌──────────────────────────────────┐
   │ PCB.mm                           │
   │  - code = 0x00000000 (virtual)   │
   │  - data = 0x00001000 (virtual)   │
   │  - pgb  = ptr to page table      │
   └──────────────────────────────────┘
           │
           ▼
9. PCB LISTO PARA SCHEDULER
   ┌──────────────────────────────────┐
   │ Ready Queue                      │
   │ [PCB1] → [PCB2] → [PCB3] → ...  │
   └──────────────────────────────────┘
```

## Traducción de Direcciones (Fase 2 - No implementada aún)

```
VIRTUAL ADDRESS (ejemplo: 0x00001234)
┌────────────┬────────────────┐
│ VPN (12b)  │  Offset (12b)  │
│  0x001     │    0x234       │
└────────────┴────────────────┘
      │              │
      │              └──────────────────────────┐
      ▼                                         │
   PTBR ──> Page Table                          │
            ┌───────────────┐                   │
            │ PTE[0]        │                   │
            │ PTE[1] ───────┼─> Frame 257       │
            │ ...           │                   │
            └───────────────┘                   │
                  │                             │
                  ▼                             │
            Frame Number: 257                   │
                  │                             │
                  │                             │
PHYSICAL ADDRESS  │                             │
┌─────────────────┴───────────┬────────────────┤
│ Frame Number (12b)          │  Offset (12b)  │
│       257                   │    0x234       │
└─────────────────────────────┴────────────────┘
                  │
                  ▼
            Physical Memory
            Frame 257 + Offset 0x234
            = Word at address...
```

## Resumen de Componentes

| Componente | Archivo | Descripción |
|------------|---------|-------------|
| Physical Memory | memory.c/h | Simula RAM de 16 MB |
| Page Table | memory.h | Estructura de entrada PT |
| PCB | process.c/h | Añadido campo mm |
| Loader | loader.c/h | Carga programas desde archivo |
| Hardware Thread | machine.c/h | CPU thread con PC, IR, PTBR, MMU, TLB |
| TLB | machine.h | Buffer de traducción (16 entradas) |
| MMU | machine.h | Unidad de gestión de memoria |

## Estado Actual vs. Futuro

### FASE 1 (Actual) - COMPLETADA ✅
- ✅ Estructuras de memoria creadas
- ✅ Loader funcional
- ✅ PCB con información de memoria
- ✅ Hardware Threads con MMU y TLB
- ✅ Programas cargados en memoria física
- ❌ NO hay traducción de direcciones (aún)
- ❌ NO hay ejecución de instrucciones (aún)

### FASE 2 (Futuro) - PENDIENTE
- ⏳ Implementar traducción virtual → física
- ⏳ Implementar búsqueda en TLB
- ⏳ Implementar fetch-decode-execute con MMU
- ⏳ Implementar page faults
- ⏳ Implementar context switch con PTBR
