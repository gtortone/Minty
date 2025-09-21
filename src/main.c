/*
//   Minty - MultiCart for Mattel Intellivision by Gennaro Tortone 2025
//
//   based on PiRTO II Flash MultiCART by Andrea Ottaviani 2024
//   parts of code are directly from the A8PicoCart project by Robin Edwards 2023
*/

#include <stdio.h>
#include <string.h>
#include "board.h"
#include "pico/stdlib.h"
#include "inty_cart.h"
#include "usb_tasks.h"

#include "hardware/vreg.h"
#include "hardware/clocks.h"

int main(void) {

#ifdef DEFAULT_BOARD
   vreg_set_voltage(VREG_VOLTAGE_1_15);
   // wait for the voltage to stabilize
   sleep_ms(500);

   // increase frequency due to low optimization level (-O1) required 
   // to avoid timing issues with cart firmware
   set_sys_clock_khz(230000, true);
#endif

   gpio_init(MSYNC_PIN);
   gpio_set_dir(MSYNC_PIN, false);
   gpio_init(RST_PIN);
   gpio_set_dir(RST_PIN, true);

   stdio_init_all();

   tud_init(BOARD_TUD_RHPORT);

   printf("START\n");

   // reset interval in ms
   int t = 100;

   while (gpio_get(MSYNC_PIN) == 0 && to_ms_since_boot(get_absolute_time()) < 2000) {   // wait for Inty powerup
      if (to_ms_since_boot(get_absolute_time()) > t) {
         t += 100;
         gpio_put(RST_PIN, false);
         sleep_ms(5);
         gpio_put(RST_PIN, true);
      }
   }

   // check why loop is ended...
   if (gpio_get(MSYNC_PIN) == 1)
      Inty_cart_main();

   while (1) {
      tud_task();    // tinyusb device task
      cdc_task();
   }

   return 0;
}
