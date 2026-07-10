#ifndef ECS_H
#define ECS_H

#include "emu2149.h"

#define PWM_WRAP 1024

#define PAL_ECS_FREQ       4000000
#define NTSC_ECS_FREQ      3579545
#define SAMPLING_FREQ      48000
#define ECS_PERIOD         25

void init_ecs(uint8_t tv_mode, uint8_t volume);

#endif
