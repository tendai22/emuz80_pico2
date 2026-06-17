// license: BSD 3-Clause License
// copyright-holders: Norihiro Kumagai
//
// emuz80_pico2.c ... A Z80 manipulator on RP2350A/B/RP2020
//

// Three configuration macros
// RP2350B_CoreBoard ... WeAct RP2350B_CoreBoard
// RP2350_Zero ... Waveshare RP2350-Zero/One
// AE_RP2040 .... Akizuki Denshi AE-RP2040
//
// You can specify one of the three in CMakeLists.txt
//
#if defined(RP2350B_CoreBoard) || defined(RP2350_Zero) || defined(AE_RP2040)
#else
#error Either one of the following macro definition is needed, RP2350B_CoreBoard|RP2350_Zero|AE_RP2040
#endif
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "tusb.h"
#include "pico/stdio_usb.h"

#include "emuz80.h"


void gpio_out_init(uint gpio, bool value) {
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, value);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
}

uint8_t __aligned(65536) mem[65536];


//
// USB CDC
//
static int cdc_itf = 0;

// 
// serial status registers
//
volatile int tx_rdy = 0;
volatile int rx_rdy = 0;
volatile int tx_data = 0;
volatile int rx_data = 0;
volatile int txbuf_full = 0;

//
// core0 のメインループ
// この関数からリターンしない。
__attribute__((noinline)) void __time_critical_func(emuz80_core0_entry)(void)
{
    // usb serial handling
    int c;
    uint8_t cb;
    uint32_t data;
    uint32_t cmd;
    while (1) {
        tud_task();
        if (rx_rdy == 0 && tud_cdc_n_available(cdc_itf) > 0) {
            tud_cdc_n_read(cdc_itf, &cb, 1);
            rx_data = cb;
            rx_rdy = 1;
        }
        if (tx_rdy == 0 && tud_cdc_n_write_available(cdc_itf) > 0) {
            tx_rdy = 2;
        }
        if (tx_rdy && txbuf_full) {
            cb = tx_data;
            tud_cdc_n_write(cdc_itf, &cb, 1);
            tud_cdc_n_write_flush(cdc_itf);     // flush seems to be needed
            txbuf_full = 0;
            tx_rdy = 0;
        }
    }

}


__attribute__((noinline)) int __time_critical_func(main)(void) 
{
    set_sys_clock_khz(150000, true);

    stdio_init_all();
    setbuf(stdout, NULL);
    sleep_ms(1000);     // needed for starting USB printf

    // mem clear
    for (int i = 0 ; i < sizeof mem; ++i)
        mem[i] = 0;
    // copy z80 program
    cpu_loader();

    //
    // core1 (bus read/write loop)
    //
    multicore_launch_core1(emuz80_core1_entry);
    uint32_t g = multicore_fifo_pop_blocking();
    if (g != FLAG_VALUE) {
        printf("core1 start failure, stopping\n");
        while(1);
    } else {
        printf("core1 start, push core0 status!\n");
    }

    multicore_fifo_push_blocking(FLAG_VALUE);   // start core1

    // start peripheral emulation loop
    emuz80_core0_entry();
    // NOT REACHED
}

//
// Insufficient definition warning
//
#if defined(RP2350B_CoreBoard) || defined(RP2350_Zero) || defined(AE_RP2040)
#else
#error Either one of the following macro definition is needed, RP2350B_CoreBoard, RP2350_Zero, or AE_RP2040
#endif