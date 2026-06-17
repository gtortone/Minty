#if CONFIG_ECS_AUDIO

#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"

#include "board.h"
#include "ecs.h"

#if CONFIG_ECS_AUDIO


static repeating_timer_t timer;
PSG* psg0;
uint8_t ecs_AudioVolume;

const uint8_t ECS_LUT[16] = {
   0x00,
   0x02,
   0x04,
   0x0B,
   0x01,
   0x03,
   0x05,
   0x0C,
   0x07,
   0x06,
   0x0D,
   0x08,
   0x09,
   0x0A,
   0x0E,
   0x0F
};

bool ay_callback(repeating_timer_t *rt) {   
   PSG_calc(psg0);
   // mix 3 channels, apply 8 bits volume control, normalise and map to 10 bits output
   uint16_t EcsAudioOut = ( abs((int32_t)psg0->ch_out[0] + 
                                (int32_t)psg0->ch_out[1] + 
                                (int32_t)psg0->ch_out[2]) * (int32_t)(ecs_AudioVolume + 1) / 3 ) >> 10;

   pwm_set_gpio_level(ECS_AUDIO, EcsAudioOut);

   return true;
}

#endif

void init_ecs(uint8_t tv_mode, uint8_t volume) {

#if CONFIG_ECS_AUDIO
   gpio_set_function(ECS_AUDIO, GPIO_FUNC_PWM);

   uint audioSlice = pwm_gpio_to_slice_num(ECS_AUDIO);

   pwm_config cfg = pwm_get_default_config();
   pwm_config_set_clkdiv(&cfg, 1.0f);
   pwm_config_set_wrap(&cfg, PWM_WRAP);
   pwm_init(audioSlice, &cfg, true);
   ecs_AudioVolume = volume;

   if (tv_mode == 0) 
      psg0 = PSG_new(2000000, SAMPLING_FREQ);
   else
      psg0 = PSG_new(1789772, SAMPLING_FREQ);

   PSG_reset(psg0);
   PSG_setVolumeMode(psg0, EMU2149_VOL_AY_3_8910);

   add_repeating_timer_us(-(1.0/SAMPLING_FREQ) * 1000000, ay_callback, NULL, &timer);
#endif

}

#endif
