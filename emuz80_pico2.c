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
#include "tusb.h"
#include "pico/stdio_usb.h"

#include "emuz80.h"

void gpio_out_init(uint gpio, bool value) {
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, value);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
}

uint8_t __aligned(65536) mem[65536];

uint8_t uart_test[] = {
0x31, 0x00, 0x80,   // LD SP, 0x8000
//loop0:
0xDB, 0x01,         // IN A, (0x1)
0xCB, 0x47,         // BIT 0, A
0x28, 0xF9,         // JR Z, loop0(0003H)
0xDB, 0x00,         // LD A, (0x0)
0xFE, 0x61,         // CP A, 'a'
0x38, 0x06,         // JR C, label1
0xFE, 0x7B,         // CP A, 'z'+1
0x30, 0x02,         // JR NC, label1
0xE6, 0xDF,         // AND A, DFH(clear Bit5)
//label1:
0x47,               // MOV B,A
//loop1:
0xDB, 0x01,         // LD A, (0xE001)
0xCB, 0x4F,         // BIT 1, A
0x28, 0xFA,         // JR Z, loop1
0x78,               // MOV A,B
0xD3, 0x00,   // OUT (0x0), A
0x18, 0xE2,         // JR loop0
};

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

//#define EMUBASIC_IO

__attribute__((noinline)) int __time_critical_func(main)(void) 
{

    stdio_init_all();
    setbuf(stdout, NULL);
    sleep_ms(1000);     // needed for starting USB printf

    // Z80 Input pin initialize
    emuz80_gpio_init();
    emuz80_pio_init();
    emuz80_dma_init();

    // mem clear
    for (int i = 0 ; i < sizeof mem; ++i)
        mem[i] = 0;
    // copy prog1
#ifdef EMUBASIC_IO
    printf("loading: EMUBASIC_IO\n");
#include "emubasic_io.h"
    memcpy(&mem[0], &emuz80_binary[0], sizeof emuz80_binary);
#endif
#ifdef EMUBASIC
    printf("loading: EMUBASIC\n")
    memcpy(&mem[0], &emuz80_binary[0], sizeof emuz80_binary);
#endif

    //
    // debug Z80 codes
    //
#if 0
    for (int i = 0 ; i < sizeof emuz80_binary; ++i) {
        if (i % 8 == 0)
            printf("%04X ", i);
        printf("%02X ", mem[i]);
        if (i % 8 == 7)
            printf("\n");
    }
    printf("\n");
#endif
    //
    // Z80 test codes
    // 
#if 0
    // halt
    mem[0] = 0x76;
#endif
#if 1
    // jr loop
    mem[0] = 0x18;
    mem[1] = 0xfe;
#endif
#if 0
    // JP 0000H
    mem[0] = 0xc3;
    mem[1] = 0x00;
    mem[2] = 0x00;
#endif
#if 0
    // INC (HL), JR 0xfc
    mem[0] = 0x21;  // LD HL, 7F00H
    mem[1] = 0x00;
    mem[2] = 0x7f;
    mem[3] = 0x34;  // INC (HL)
    mem[4] = 0x18;  // JR
    mem[5] = 0xfd;  // -3
#endif
#if 0
    // inc (hl) loop
    mem[0] = 0x21;
    mem[1] = 0x38;
    mem[2] = 0x00;
    mem[3] = 0x34;
    mem[4] = 0x18;
    mem[5] = 0xfd;
    mem[6] = 0x0;
#endif
#if 0
    // in 0h loop
    mem[0] = 0xdb;  // IN 0H
    mem[1] = 0x00;
    mem[2] = 0x18;  // jr
    mem[3] = 0xfc;  // -4 
#endif
#if 0
    // out 0h loop
    mem[0] = 0xd3;  // OUT 0H
    mem[1] = 0x00;
    mem[2] = 0x3c;  // INC A
    mem[3] = 0x18;  // jr
    mem[4] = 0xfb;  // -5 
#endif
# if 0
    // UART TEST (IO port version)
    uint8_t mem0[] = {
        0x31, 0x00, 0x80,
        0xDB, 0x01,
        0xCB, 0x4F,
        0x28, 0xFA,
        0x3E, 0x41,
        0xD3, 0x00,
        0x18, 0xF4,
    };
    for (int i = 0; i < sizeof mem0; ++i) {
        mem[i] = mem0[i];
        printf("%02x ", mem[i]);
    }
    printf("\n");
#endif
#if 0
    // UART R/W test
    for (int i = 0; i < sizeof uart_test; ++i)
        mem[i] = uart_test[i];

#endif
#if 0
    uart_putc_raw(UART_ID, 'X');
    while (uart_is_readable(UART_ID) == 0) {
        while (uart_is_writable(UART_ID) == 0);
        uart_putc_raw(UART_ID, 'A');
        sleep_ms(500);
    }
    uart_putc_raw(UART_ID, 'Y');
#endif

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
    sleep_us(2);

    // start target CPU
    //emuz80_unreset();
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