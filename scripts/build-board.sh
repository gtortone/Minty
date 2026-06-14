#!/bin/bash

board=$1

cmake -B build/$board/debug -DPICO_BOARD=$board -DCMAKE_BUILD_TYPE=Debug
make -j -C build/$board/debug

cmake -B build/$board/release -DPICO_BOARD=$board -DCMAKE_BUILD_TYPE=Release
make -j -C build/$board/release
