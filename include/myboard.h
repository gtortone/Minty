/*
//   Minty - MultiCart for Mattel Intellivision by Gennaro Tortone 2025
//
//   based on PiRTO II Flash MultiCART by Andrea Ottaviani 2024
//   parts of code are directly from the A8PicoCart project by Robin Edwards 2023
*/

#ifndef MYBOARD_H_
#define MYBOARD_H_

// setting first overrides the value in the default header

//#define PICO_FLASH_SPI_CLKDIV 2     // use for winbond flash (e.g. Pico Purple)
#define PICO_FLASH_SPI_CLKDIV 4 // use for slower flash (e.g. zbit)

#endif
