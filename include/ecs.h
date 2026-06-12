#ifndef ECS_H
#define ECS_H

#include "emu2149.h"

#define PWM_WRAP 1024

#define SAMPLING_FREQ      48000
#define FRAME_FREQ         60
#define FRAME_LOOPS_CNT    SAMPLING_FREQ / FRAME_FREQ

void init_ecs(void);

#endif
