# Practica: Busqueda Paralela Local con OpenMP

Este proyecto implementa un sistema de busqueda paralela en C++ utilizando memoria dinamica y paralelismo multihilo con OpenMP. Permite evaluar y comparar el rendimiento de búsquedas secuenciales y binarias sobre un arreglo ordenado mediante Parallel Merge Sort.

## Contenido del Repositorio

- `busqueda_local.cpp`: Codigo fuente principal del programa.
- `.gitignore`: Archivo para omitir temporales, ejecutables y logs.

## Requisitos

- Compilador de C++ (g++ o clang) con soporte para OpenMP.

## Compilacion

Para compilar el programa ejecutando optimizaciones y activando OpenMP:

```bash
g++ -O3 -fopenmp busqueda_local.cpp -o busqueda_local