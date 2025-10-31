#!/bin/bash
set -e
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:./lib64";
clang -std=c99 -Wall -pedantic -L./lib64 -lruntime *.c -o pa2
LD_PRELOAD=lib64/libruntime.so ./pa2 -p 2 10 20
