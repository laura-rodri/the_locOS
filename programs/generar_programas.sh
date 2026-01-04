#!/bin/bash

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # Sin color

# Verificar número de argumentos
if [ $# -eq 1 ] && [ "$1" == "clean" ]; then
    echo -e "${YELLOW}Limpiando archivos .elf generados...${NC}"
    rm -f prog*.elf
    exit 0
fi
if [ $# -ne 3 ]; then
    echo "Uso: $0 <num_programas> <min_longitud> <max_longitud>"
    echo "Parámetros:"
    echo "  num_programas: Número de programas a generar"
    echo "  min_longitud:  Longitud mínima (en líneas)"
    echo "  max_longitud:  Longitud máxima (en líneas)"
    exit 1
fi

# Leer parámetros
NUM_PROGRAMAS=$1
MIN_LONGITUD=$2
MAX_LONGITUD=$3

# Validar parámetros
if ! [[ "$NUM_PROGRAMAS" =~ ^[0-9]+$ ]] || [ "$NUM_PROGRAMAS" -le 0 ]; then
    echo -e "${RED}Error: num_programas debe ser un número positivo${NC}"
    exit 1
fi

if ! [[ "$MIN_LONGITUD" =~ ^[0-9]+$ ]] || [ "$MIN_LONGITUD" -le 0 ]; then
    echo -e "${RED}Error: min_longitud debe ser un número positivo${NC}"
    exit 1
fi

if ! [[ "$MAX_LONGITUD" =~ ^[0-9]+$ ]] || [ "$MAX_LONGITUD" -le 0 ]; then
    echo -e "${RED}Error: max_longitud debe ser un número positivo${NC}"
    exit 1
fi

if [ "$MIN_LONGITUD" -gt "$MAX_LONGITUD" ]; then
    echo -e "${RED}Error: min_longitud no puede ser mayor que max_longitud${NC}"
    exit 1
fi

# Directorio de prometheus
PROMETHEUS_DIR="$(dirname "$0")/prometheus"
PROMETHEUS_BIN="$PROMETHEUS_DIR/prometheus"

# Verificar que prometheus existe
if [ ! -f "$PROMETHEUS_BIN" ]; then
    echo -e "${RED}Error: No se encuentra prometheus en $PROMETHEUS_BIN${NC}"
    echo -e "${YELLOW}Compilando prometheus...${NC}"
    
    # Intentar compilar prometheus
    if [ -f "$PROMETHEUS_DIR/Makefile" ]; then
        (cd "$PROMETHEUS_DIR" && make)
        if [ $? -ne 0 ]; then
            echo -e "${RED}Error al compilar prometheus${NC}"
            exit 1
        fi
    else
        echo -e "${RED}No se encuentra Makefile en $PROMETHEUS_DIR${NC}"
        exit 1
    fi
fi

# Banner
echo "╔═══════════════════════════════════════════════╗"
echo "║     Generador de Programas - Prometheus       ║"
echo "╚═══════════════════════════════════════════════╝"
echo ""
echo -e "${GREEN}Configuración:${NC}"
echo "  Número de programas: $NUM_PROGRAMAS"
echo "  Longitud mínima:     $MIN_LONGITUD líneas"
echo "  Longitud máxima:     $MAX_LONGITUD líneas"
echo ""

# Generar programas
SEED=$RANDOM
PRIMER_NUM=0
rm -f prog*.elf
echo -e "${YELLOW}Generando programas...${NC}"
echo ""

# Generar cada programa con una longitud aleatoria entre min y max
PROGRAMAS_GENERADOS=0
LONGITUD_ACTUAL=$MIN_LONGITUD

for ((i=0; i<NUM_PROGRAMAS; i++)); do
    # Calcular longitud aleatoria entre MIN y MAX
    RANGO=$((MAX_LONGITUD - MIN_LONGITUD + 1))
    LONGITUD=$((MIN_LONGITUD + RANDOM % RANGO))
    
    # Calcular número del programa
    NUM_PROG=$((PRIMER_NUM + i))
    
    echo -e "${GREEN}[$(($i + 1))/$NUM_PROGRAMAS]${NC} Generando prog$(printf "%03d" $NUM_PROG).elf con ~$LONGITUD líneas..."
    
    # Ejecutar prometheus para generar el programa
    $PROMETHEUS_BIN -s $SEED -n prog -f $NUM_PROG -l $LONGITUD -p 1 > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        PROGRAMAS_GENERADOS=$((PROGRAMAS_GENERADOS + 1))
    else
        echo -e "${RED}  Error al generar prog$(printf "%03d" $NUM_PROG).elf${NC}"
    fi
    
    # Incrementar seed para variedad
    SEED=$((SEED + 1))
done

echo ""
echo "╔═══════════════════════════════════════════════╗"
echo "║              Generación Completa              ║"
echo "╚═══════════════════════════════════════════════╝"
echo -e "${GREEN}Programas generados exitosamente: $PROGRAMAS_GENERADOS/$NUM_PROGRAMAS${NC}"
echo ""

# Listar archivos generados
if [ $PROGRAMAS_GENERADOS -gt 0 ]; then
    echo "Archivos .elf generados:"
    ls -lh prog*.elf 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
fi

echo ""
