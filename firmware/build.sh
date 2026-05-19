#!/bin/bash

IB_HOME="/home/tortone/fun/intellivision/IntyBASIC"
IB_BIN=$IB_HOME/intybasic
AS_BIN="/home/tortone/fun/intellivision/jzintv/bin/as1600"

$IB_BIN PiNTY.bas launcher.asm $IB_HOME
$AS_BIN launcher.asm -o launcher.bin

bin2header launcher.bin "__in_flash() mintyfw" > rom.h && mv rom.h ../include
rm launcher.asm launcher.bin
