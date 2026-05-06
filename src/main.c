/*
//   Minty - MultiCart for Mattel Intellivision by Gennaro Tortone 2025
//
//   based on PiRTO II Flash MultiCART by Andrea Ottaviani 2024
//   parts of code are directly from the A8PicoCart project by Robin Edwards 2023
*/

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "inty_cart.h"
#include "usb_tasks.h"

#include "hardware/vreg.h"
#include "hardware/clocks.h"

int main(void) {

#if PICO_RP2040
   vreg_set_voltage(VREG_VOLTAGE_1_15);
   sleep_ms(200);
   set_sys_clock_khz(200000, true);
#elif PICO_RP2350
   vreg_set_voltage(VREG_VOLTAGE_1_15);
   sleep_ms(200);
   set_sys_clock_khz(250000, true);
#endif

   gpio_init(MSYNC_PIN);
   gpio_set_dir(MSYNC_PIN, false);
   gpio_init(RST_PIN);
   gpio_set_dir(RST_PIN, true);

#ifdef UART_ID
#if UART_TX != -1
   gpio_set_function(UART_TX, UART_FUNCSEL_NUM(UART_ID, UART_TX));
#endif
#if UART_RX != -1
   gpio_set_function(UART_RX, UART_FUNCSEL_NUM(UART_ID, UART_RX));
#endif
   uart_init(UART_ID, UART_BAUDRATE);
   stdio_uart_init_full(UART_ID, 115200, UART_TX, UART_RX);
#else
   stdio_init_all();
#endif

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
