#!/bin/bash

IB_HOME="/home/tortone/fun/intellivision/IntyBASIC"
IB_BIN=$IB_HOME/intybasic
AS_BIN="/home/tortone/fun/intellivision/jzintv/bin/as1600"

$IB_BIN Minty.bas Minty.asm $IB_HOME
$AS_BIN Minty.asm -o Minty.bin

bin2header Minty.bin "__not_in_flash() mintyfw" > rom.h && mv rom.h ../include
rm Minty.asm Minty.bin
