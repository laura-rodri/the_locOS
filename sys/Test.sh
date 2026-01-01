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
TEST_DURATION=5

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   locOS Kernel Test Suite${NC}"
echo -e "${BLUE}   Tests de Scheduler y Sincronización${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Compile the kernel first
echo -e "${YELLOW}[1/13] Compilando el kernel...${NC}"
make clean > /dev/null 2>&1
make > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo -e "${RED}✗ Error al compilar el kernel${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Compilación exitosa${NC}"
echo ""

# Test 1: Round Robin con reloj global (configuración por defecto)
echo -e "${YELLOW}[2/13] Test 1: Round Robin + Reloj Global (Default)${NC}"
echo "Parámetros: -q 5 -policy 0 -sync 0 -f 2"
timeout $TEST_DURATION ./kernel -q 5 -policy 0 -sync 0 -f 2 -pmin 2 -pmax 4 -tmin 5 -tmax 10 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 2: Round Robin con timer
echo -e "${YELLOW}[3/13] Test 2: Round Robin + Timer${NC}"
echo "Parámetros: -q 8 -policy 0 -sync 1 -f 2"
timeout $TEST_DURATION ./kernel -q 8 -policy 0 -sync 1 -f 2 -pmin 2 -pmax 5 -tmin 8 -tmax 15 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 3: BFS con reloj global
echo -e "${YELLOW}[4/13] Test 3: BFS (Virtual Deadlines) + Reloj Global${NC}"
echo "Parámetros: -q 5 -policy 1 -sync 0 -f 2"
timeout $TEST_DURATION ./kernel -q 5 -policy 1 -sync 0 -f 2 -pmin 2 -pmax 4 -tmin 5 -tmax 10 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 4: BFS con timer
echo -e "${YELLOW}[5/13] Test 4: BFS (Virtual Deadlines) + Timer${NC}"
echo "Parámetros: -q 10 -policy 1 -sync 1 -f 3"
timeout $TEST_DURATION ./kernel -q 10 -policy 1 -sync 1 -f 3 -pmin 3 -pmax 6 -tmin 10 -tmax 20 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 5: Prioridades con reloj global
echo -e "${YELLOW}[6/13] Test 5: Prioridades Estáticas + Reloj Global${NC}"
echo "Parámetros: -q 6 -policy 2 -sync 0 -f 2"
timeout $TEST_DURATION ./kernel -q 6 -policy 2 -sync 0 -f 2 -pmin 2 -pmax 4 -tmin 5 -tmax 12 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 6: Prioridades con timer
echo -e "${YELLOW}[7/13] Test 6: Prioridades Estáticas + Timer${NC}"
echo "Parámetros: -q 7 -policy 2 -sync 1 -f 2"
timeout $TEST_DURATION ./kernel -q 7 -policy 2 -sync 1 -f 2 -pmin 2 -pmax 5 -tmin 8 -tmax 15 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 7: Alta frecuencia de reloj
echo -e "${YELLOW}[8/13] Test 7: Alta Frecuencia de Reloj (10 Hz)${NC}"
echo "Parámetros: -f 10 -q 3 -policy 0"
timeout $TEST_DURATION ./kernel -f 10 -q 3 -policy 0 -pmin 1 -pmax 3 -tmin 5 -tmax 10 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 8: Múltiples CPUs y Cores
echo -e "${YELLOW}[9/13] Test 8: Múltiples CPUs (2 CPUs, 4 cores, 2 threads)${NC}"
echo "Parámetros: -cpus 2 -cores 4 -threads 2 -policy 1"
timeout $TEST_DURATION ./kernel -cpus 2 -cores 4 -threads 2 -policy 1 -f 2 -q 5 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 9: Quantum pequeño
echo -e "${YELLOW}[10/13] Test 9: Quantum Pequeño (2 ticks)${NC}"
echo "Parámetros: -q 2 -policy 0 -f 3"
timeout $TEST_DURATION ./kernel -q 2 -policy 0 -f 3 -pmin 1 -pmax 3 -tmin 4 -tmax 8 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 10: Quantum grande
echo -e "${YELLOW}[11/13] Test 10: Quantum Grande (20 ticks)${NC}"
echo "Parámetros: -q 20 -policy 1 -f 2"
timeout $TEST_DURATION ./kernel -q 20 -policy 1 -f 2 -pmin 5 -pmax 10 -tmin 15 -tmax 30 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 11: Generación rápida de procesos
echo -e "${YELLOW}[12/13] Test 11: Generación Rápida de Procesos${NC}"
echo "Parámetros: -pmin 1 -pmax 2 -qsize 50 -policy 2"
timeout $TEST_DURATION ./kernel -pmin 1 -pmax 2 -qsize 50 -policy 2 -f 3 -q 5 -tmin 5 -tmax 10 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

# Test 12: Procesos de larga duración
echo -e "${YELLOW}[13/13] Test 12: Procesos de Larga Duración${NC}"
echo "Parámetros: -tmin 50 -tmax 100 -policy 0"
timeout $TEST_DURATION ./kernel -tmin 50 -tmax 100 -policy 0 -f 2 -q 10 -pmin 5 -pmax 10 > /dev/null 2>&1
if [ $? -eq 124 ]; then
    echo -e "${GREEN}✓ Test completado${NC}"
else
    echo -e "${RED}✗ Test falló${NC}"
fi
echo ""

echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}   Tests completados${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${YELLOW}Notas sobre el sistema:${NC}"
echo -e "  - El Clock decrementa TTL de procesos en ejecución"
echo -e "  - El Scheduler gestiona asignación y expulsión"
echo -e "  - Sincronización: Clock (cada tick) o Timer (cada quantum)"
echo ""
echo -e "${YELLOW}Ejecuta tests individuales con:${NC}"
echo -e "  ./kernel -q <quantum> -policy <0|1|2> -sync <0|1> [opciones]"
echo ""
echo -e "${YELLOW}Políticas disponibles:${NC}"
echo -e "  0 = Round Robin"
echo -e "  1 = BFS (Brain Fuck Scheduler con virtual deadlines)"
echo -e "  2 = Prioridades Estáticas (40 colas de prioridad/nice)"
echo ""
echo -e "${YELLOW}Modos de sincronización:${NC}"
echo -e "  0 = Reloj Global (scheduler se activa cada tick)"
echo -e "  1 = Timer dedicado (scheduler se activa cada quantum ticks)"
echo ""
echo -e "${YELLOW}Para ver todas las opciones:${NC}"
echo -e "  ./kernel --help"
echo ""
