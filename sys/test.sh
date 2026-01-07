#!/bin/bash

# Test.sh - Script de pruebas para el kernel locOS
# Prueba diferentes configuraciones del scheduler y modos de sincronización

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test duration in seconds
TEST_DURATION=1

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   the_locOS Kernel Test Suite${NC}"
echo -e "${BLUE}   Tests de Scheduler y Sincronización${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Compile the kernel first
echo -e "${YELLOW}[1/16] Compilando el kernel...${NC}"
make clean > /dev/null 2>&1
make > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo -e "${RED}✗ Error al compilar el kernel${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Compilación exitosa${NC}"
echo ""

# ============================================================
# TESTS DE POLÍTICAS CON SINCRONIZACIONES
# ============================================================

# Test 1: Round Robin + Reloj Global
echo -e "${YELLOW}[2/16] Test 1: Round Robin + Reloj Global${NC}"
echo "Parámetros: -q 5 -policy 0 -sync 0 -f 2"
timeout $TEST_DURATION ./kernel -q 5 -policy 0 -sync 0 -f 2 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 2: Round Robin + Timer
echo -e "${YELLOW}[3/16] Test 2: Round Robin + Timer${NC}"
echo "Parámetros: -q 8 -policy 0 -sync 1 -f 3"
timeout $TEST_DURATION ./kernel -q 8 -policy 0 -sync 1 -f 3 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 3: BFS + Reloj Global
echo -e "${YELLOW}[4/16] Test 3: BFS + Reloj Global${NC}"
echo "Parámetros: -q 6 -policy 1 -sync 0 -f 2"
timeout $TEST_DURATION ./kernel -q 6 -policy 1 -sync 0 -f 2 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 4: BFS + Timer
echo -e "${YELLOW}[5/16] Test 4: BFS + Timer${NC}"
echo "Parámetros: -q 10 -policy 1 -sync 1 -f 2"
timeout $TEST_DURATION ./kernel -q 10 -policy 1 -sync 1 -f 2 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 5: Prioridades + Reloj Global
echo -e "${YELLOW}[6/16] Test 5: Prioridades + Reloj Global${NC}"
echo "Parámetros: -q 7 -policy 2 -sync 0 -f 2"
timeout $TEST_DURATION ./kernel -q 7 -policy 2 -sync 0 -f 2 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 6: Prioridades + Timer
echo -e "${YELLOW}[7/16] Test 6: Prioridades + Timer${NC}"
echo "Parámetros: -q 12 -policy 2 -sync 1 -f 2"
timeout $TEST_DURATION ./kernel -q 12 -policy 2 -sync 1 -f 2 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# ============================================================
# TESTS DE VARIACIONES DE PARÁMETROS
# ============================================================

# Test 7: Quantum pequeño
echo -e "${YELLOW}[8/16] Test 7: Round Robin - Quantum Pequeño (2)${NC}"
echo "Parámetros: -q 2 -policy 0 -sync 0 -f 4"
timeout $TEST_DURATION ./kernel -q 2 -policy 0 -sync 0 -f 4 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 8: Quantum grande
echo -e "${YELLOW}[9/16] Test 8: BFS - Quantum Grande (25)${NC}"
echo "Parámetros: -q 25 -policy 1 -sync 1 -f 1"
timeout $TEST_DURATION ./kernel -q 25 -policy 1 -sync 1 -f 1 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 9: Alta frecuencia
echo -e "${YELLOW}[10/16] Test 9: Round Robin - Alta Frecuencia (10 Hz)${NC}"
echo "Parámetros: -q 3 -policy 0 -sync 0 -f 10"
timeout $TEST_DURATION ./kernel -q 3 -policy 0 -sync 0 -f 10 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 10: Cola grande
echo -e "${YELLOW}[11/16] Test 10: Prioridades - Cola Grande (150)${NC}"
echo "Parámetros: -qsize 150 -policy 2 -sync 0 -f 3 -q 8"
timeout $TEST_DURATION ./kernel -qsize 150 -policy 2 -sync 0 -f 3 -q 8 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# ============================================================
# TESTS MULTIPROCESADOR
# ============================================================

# Test 11: Multiprocesador - Round Robin
echo -e "${YELLOW}[12/16] Test 11: Multiprocesador - Round Robin (2 CPUs, 4 cores)${NC}"
echo "Parámetros: -cpus 2 -cores 4 -threads 2 -policy 0 -sync 1 -q 6 -f 3"
timeout $TEST_DURATION ./kernel -cpus 2 -cores 4 -threads 2 -policy 0 -sync 1 -q 6 -f 3 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 12: Multiprocesador - BFS
echo -e "${YELLOW}[13/16] Test 12: Multiprocesador - BFS (2 CPUs, 2 cores, 4 threads)${NC}"
echo "Parámetros: -cpus 2 -cores 2 -threads 4 -policy 1 -sync 0 -q 8 -f 2"
timeout $TEST_DURATION ./kernel -cpus 2 -cores 2 -threads 4 -policy 1 -sync 0 -q 8 -f 2 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 13: Multiprocesador - Prioridades
echo -e "${YELLOW}[14/16] Test 13: Multiprocesador - Prioridades (3 CPUs, 2 cores)${NC}"
echo "Parámetros: -cpus 3 -cores 2 -threads 2 -policy 2 -sync 1 -q 10 -f 2"
timeout $TEST_DURATION ./kernel -cpus 3 -cores 2 -threads 2 -policy 2 -sync 1 -q 10 -f 2 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# ============================================================
# TESTS DE ESTRÉS
# ============================================================

# Test 14: Estrés - Quantum mínimo + Alta frecuencia
echo -e "${YELLOW}[15/16] Test 14: ESTRÉS - Quantum 1 + Frecuencia 15 Hz${NC}"
echo "Parámetros: -q 1 -policy 0 -sync 0 -f 15"
timeout $TEST_DURATION ./kernel -q 1 -policy 0 -sync 0 -f 15 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 15: Estrés Total - Todo al máximo
echo -e "${YELLOW}[16/16] Test 15: ESTRÉS TOTAL - Configuración Extrema${NC}"
echo "Parámetros: -q 1 -policy 2 -sync 0 -f 20 -qsize 200 -cpus 4 -cores 2 -threads 2"
timeout $TEST_DURATION ./kernel -q 1 -policy 2 -sync 0 -f 20 -qsize 200 -cpus 4 -cores 2 -threads 2 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}   ✓ Todos los tests completados${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${YELLOW}Flags disponibles:${NC}"
echo -e "  -f <hz>          Clock frequency (default: 1)"
echo -e "  -q <ticks>       Quantum (default: 3)"
echo -e "  -policy <num>    0=RR, 1=BFS, 2=Prioridades (default: 0)"
echo -e "  -sync <mode>     0=Clock, 1=Timer (default: 0)"
echo -e "  -qsize <num>     Cola de procesos (default: 100)"
echo -e "  -cpus <num>      Número de CPUs (default: 1)"
echo -e "  -cores <num>     Cores por CPU (default: 2)"
echo -e "  -threads <num>   Threads por core (default: 4)"
echo -e "  -t <num>         Número de timers (default: 1)"
echo -e "  -timeri <ticks>  Intervalo del timer (default: 5)"
echo ""
echo -e "${YELLOW}Ejemplo de uso:${NC}"
echo -e "  ./kernel -q 5 -policy 1 -sync 0 -f 3"
echo -e "  ./kernel --help"
echo ""
