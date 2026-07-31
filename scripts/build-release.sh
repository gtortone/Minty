#!/bin/bash

rm -rf build

boards=("pintycard" "pirto_ii_default" "pirto_ii_duo" "pirto_ii_sd" "pirto")

for board in "${boards[@]}"; do

   cmake -B build/$board/release -DPICO_BOARD=$board -DCMAKE_BUILD_TYPE=Release
   make -j -C build/$board/release

done

##

boards_ecs=("pintycard" "pirto_ii_duo")

for board in "${boards_ecs[@]}"; do

   cmake -B build/$board/release-ecs -DPICO_BOARD=$board -DCMAKE_BUILD_TYPE=Release \
      -DCONFIG_ECS_AUDIO=ON -DOUTPUT_SUFFIX=_ecs
   make -j -C build/$board/release-ecs

done


