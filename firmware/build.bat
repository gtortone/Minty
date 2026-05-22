
intybasic MiNTY.bas MiNTY.asm "%INTYBASIC_INSTALL%\lib"
as1600 MiNTY.asm -o MiNTY.bin
python bin2header.py MiNTY.bin "__in_flash() mintyfw" > ..\include\mintyrom.h
del MiNTY.asm MiNTY.bin MiNTY.cfg

intybasic PiNTY.bas PiNTY.asm "%INTYBASIC_INSTALL%\lib"
as1600 PiNTY.asm -o PiNTY.bin
python bin2header.py PiNTY.bin "__in_flash() mintyfw" > ..\include\pintyrom.h
del PiNTY.asm PiNTY.bin PiNTY.cfg
