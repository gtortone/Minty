#ifndef INTERFACE_H_
#define INTERFACE_H_

#define resetLow()  gpio_set_dir(RST_PIN,true); gpio_put(RST_PIN,true);    // Minty to INTV BUS ; RST Output set to 0
#define resetHigh() gpio_set_dir(RST_PIN,true); gpio_put(RST_PIN,false);   // RST is INPUT; B->A, INTV BUS to Pico

#define GPIO_GET_LOW_32(v)    pico_default_asm_volatile ("mrc p0, #0, %0, c0, c8" : "=r" (v));

// Inty bus values (BC1+BC2+BDIR) GPIO 18-17-16
#define BUS_NACT  0b000         //0
#define BUS_BAR   0b001         //1
#define BUS_IAB   0b010         //2
#define BUS_DWS   0b011         //3
#define BUS_ADAR  0b100         //4
#define BUS_DW    0b101         //5
#define BUS_DTB   0b110         //6
#define BUS_INTAK 0b111         //7

// Inty RAM addresses shared with cart firmware
#define CMD_ADDR     0x889
#define DONE_ADDR    0x119       // 0: cart wait, 1: cart run
#define DEV_ADDR     0x120       // 0: flash, 1: SD
#define HAS_SD_ADDR  0x121       // 0: no SD support, 1: SD support

#define COMPILER_BARRIER() asm volatile("" ::: "memory")

#endif
