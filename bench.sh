#!/bin/bash

COMPILERS=('g++' 'clang++')
OPTIONS='-std=c++20 -fno-exceptions -fno-rtti -march=native'
OPTIMIZATIONS=('-O1' '-O2' '-O3' '-Os')
FILE='array.cpp'

rm results.dat
for compiler in ${COMPILERS[@]}; do
    for optimization in ${OPTIMIZATIONS[@]}; do
        echo "# $compiler $optimization" > results.dat
        $compiler $FILE $OPTIONS $optimization -DOPTION=\""$compiler $optimization\"" -o bench.bin
        ./bench.bin >> results.dat
        rm bench.bin
        gnuplot plot.gp
        mv results.png results/${compiler}${optimization}.png
    done
done
rm results.dat