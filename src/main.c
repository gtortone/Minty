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
#include "tusb.h"
#include "inty_cart.h"
#include "fatfs_disk.h"
// FIXME
#include "hardware/clocks.h"
#include "hardware/vreg.h"

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void) {
   // connected() check for DTR bit
   // Most but not all terminal client set this when making connection
   // if ( tud_cdc_connected() )
   {
      // connected and there are data available
      if (tud_cdc_available()) {
         // read data
         char buf[64];
         uint32_t count = tud_cdc_read(buf, sizeof(buf));

         (void) count;

         // Echo back
         // Note: Skip echo by commenting out write() and write_flush()
         // for throughput test e.g
         //    $ dd if=/dev/zero of=/dev/ttyACM0 count=10000
         tud_cdc_write(buf, count);
         tud_cdc_write_flush();
      }
   }
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
   (void) itf;
   (void) rts;

   // TODO set some indicator
   if (dtr) {
      // Terminal connected
   } else {
      // Terminal disconnected
   }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) {
   (void) itf;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
   printf("Device mounted\n");
   if (!mount_fatfs_disk())
      create_fatfs_disk();
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
   printf("Device unmounted\n");
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
   (void) remote_wakeup_en;
//  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
//  blink_interval_ms = BLINK_MOUNTED;
}

int main(void) {
   gpio_init(MSYNC_PIN);
   gpio_set_dir(MSYNC_PIN, false);
   gpio_init(RST_PIN);
   gpio_set_dir(RST_PIN, true);

   // FIXME
   //set_sys_clock_khz(250000, true);
   //vreg_set_voltage(VREG_VOLTAGE_1_10);

#ifdef DEFAULT_BOARD
   // debug UART
   stdio_uart_init_full(uart1, 115200, UART_TX, UART_RX);
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
