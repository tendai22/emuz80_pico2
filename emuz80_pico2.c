#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "tusb.h"
#include "pico/stdio_usb.h"
//
// Pin Definitions
// This section should be located
// before #include "blink.pio.h"
//

//
// Pico2 A0-A10(2k) board
//
#define D0_Pin 11
#define ADDR_MASK 0x7ff
#define RD_Pin 19
#define WR_Pin 20
#define IORQ_Pin 21
#define WAIT_Pin 22
#define RESET_Pin 26
#define CLK_Pin  27
#define TEST_Pin 28

#define FLAG_VALUE 123

#include "blink.pio.h"

static int toggle_value = 1;
#define TOGGLE() do {    gpio_xor_mask64(((uint64_t)1)<<TEST_Pin); } while(0)
#define TOGGLE1() do {    gpio_xor_mask64(((uint64_t)1)<<TEST_Pin); sleep_us(1); } while(0)
//#define TOGGLE() do {    (*(volatile uint32_t *)&(sio_hw->gpio_hi_togl)) = 1; } while(0)
//#define TOGGLE() do { gpio_put(TEST_Pin, (toggle_value ^= 1));    } while(0)

void gpio_out_init(uint gpio, bool value) {
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, value);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
}

uint8_t mem[65536];

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
// UART Status/Data registers
//
uint8_t uart_status = 0;
uint8_t uart_data_tx = 0;
uint8_t uart_data_rx = 0;

#undef EMUBASIC
#include "emuz80.h"
#define EMUBASIC_IO
#include "emubasic_io.h"

//
// usb cdc putchar/getchar
//
static int saved_char = PICO_ERROR_TIMEOUT;

int kbhit(void)
{
    int c;
    if (saved_char != PICO_ERROR_TIMEOUT) {
        return 1;
    }
    saved_char = stdio_getchar_timeout_us(0);
    return saved_char != PICO_ERROR_TIMEOUT;
}

int getch(void)
{
    int c;
    uint8_t data = 'Y';
    if (kbhit()) {
        data = saved_char;
        saved_char = PICO_ERROR_TIMEOUT;
        return data;
    }
    c = stdio_getchar();
    if (c != PICO_ERROR_TIMEOUT) {
        data = c;
        saved_char = PICO_ERROR_TIMEOUT;
    } else {
        data = 'O';
    }
    return data;
}

void putch(uint32_t c)
{
    putchar_raw(c);
}

//
// core0 のメインループ
// この関数からリターンしない。
__attribute__((noinline)) void __time_critical_func(core0_entry)(void)
{
    // usb serial handling
    int c;
    uint32_t data;
    uint32_t cmd;
    while (1) {
        cmd = multicore_fifo_pop_blocking();
        if (cmd & 0x200) {
            // status register read
            data = 0;
            if (kbhit())
                data |= (1<<0);
            if (tud_cdc_write_available() > 0)
                data |= (1<<1);
        } else if (cmd & 0x100) {
            // data register read
            data = getch();
        } else {
            // data register write
            putch(cmd);
            data = 'Y';
        }
        multicore_fifo_push_blocking(data);
    }

}

// コア1のエントリポイント
// core1_entry()はPIOの状態マシンを実行し、ROMデータを送信する  
__attribute__((noinline)) void __time_critical_func(core1_entry)(void) {
    // main loop
    register uint32_t port;
    int32_t count = 100;
    uint16_t c = 0;
    int32_t temp;
    uint32_t status;
    uint16_t addr;
    uint8_t data;

    multicore_fifo_push_blocking(FLAG_VALUE);
    uint32_t g = multicore_fifo_pop_blocking();
loop:
    while(((port = gpio_get_all()) & ((1<<IORQ_Pin)|(1<<WR_Pin))) == ((1<<IORQ_Pin)|(1<<WR_Pin))) {
        // All other cycles, except neither IORQ nor WR.
        // output mem[addr] asynchronously
        //TOGGLE();
        pio_sm_put(pio0, 2, mem[port & ADDR_MASK]);
        //TOGGLE();
    }
    if ((port & ((1<<IORQ_Pin)|(1<<WR_Pin))) == (1<<IORQ_Pin)) {
        // Memory Write Cycle
        // store data to mem[addr], asynchronously
        TOGGLE();
        mem[port & ADDR_MASK] = (port >> D0_Pin) & 0xff;
        TOGGLE();
        //while (((port = gpio_get_all()) & (1<<WR_Pin)) == 0);
        goto loop;
    }
    port = gpio_get_all();      // re-read to confirm status lines
    if ((port & (1<<IORQ_Pin)) == 0) {
        if ((port & (1<<RD_Pin)) == 0) {
            // IO Read cycle
            // EMUZ80 UART emulation
            addr = port & 0xff;
            // UARTCR or DR
            if (addr == 1) {
                // read status register
                multicore_fifo_push_blocking(0x200);    // read status cmd 0x200
                status = multicore_fifo_pop_blocking();
                pio_sm_put(pio0, 2, status);
            } else if (addr == 0) {
                uint8_t data = 0;
                multicore_fifo_push_blocking(0x100);    // read rx data cmd 0x100
                data = multicore_fifo_pop_blocking();
                pio_sm_put(pio0, 2, data);
            }
        } else if ((port & (1<<WR_Pin)) == 0) {
            // IO Write cycle
            addr = port & 0xff;
            data = ((port>>D0_Pin)&0xff);
            if (addr == 0) {
                // UART DR
                multicore_fifo_push_blocking(data & 0xff);     // write tx data cmd 0-0xff
                multicore_fifo_pop_blocking();
            }
        }
        pio_sm_put(pio1, 3, 0); // notify IO process finished to the state machine
        pio_sm_get_blocking(pio1, 3);    // wait for WAIT set High
        while (((port = gpio_get_all()) & (1<<IORQ_Pin)) == 0);   // wait for cycle end
                                // wait for IORQ is High
        goto loop;
    }
    goto loop;
}


__attribute__((noinline)) int __time_critical_func(main)(void) 
{
    stdio_init_all();
    setbuf(stdout, NULL);
    sleep_ms(300);     // needed for starting USB printf
    printf("---start---\n");

    // Z80 Input pin initialize

    // GPIO Out
    gpio_out_init(WAIT_Pin, true);
    gpio_out_init(RESET_Pin, false);
    //gpio_out_init(BUSRQ_Pin, true);
    //gpio_out_init(INT_Pin, false);      // INT Pin has an inverter, so negate signal is needed

    gpio_out_init(TEST_Pin, false);

    // GPIO In
    // MREQ, IORQ, RD, RFSH, M1 are covered by PIO
    //
    gpio_init_mask(0x7fff);     // A0-A15 input 
    //gpio_init(BUSAK_Pin);
    //gpio_init(MREQ_Pin);
    gpio_init(IORQ_Pin);
    //gpio_init(RFSH_Pin);
    gpio_init(RD_Pin);
    gpio_init(WR_Pin);

    // PIO Blinking example
    PIO pio_clock = pio0;
    PIO pio_wait = pio1;
    //uint sm_clock = 0;


    uint offset1;


	// data bus

	for (int i = 0; i < 8; ++i)
        pio_gpio_init(pio0, D0_Pin + i);

    //
    // PIO StateMachine(SM) initialzation
    //
    // pio_set_gpio_base should be invoked before pio_add_program
    //pio_set_gpio_base(pio0, 16);
    //pio_set_gpio_base(pio1, 16);
    //
    // PIO0:SM0,1
	//   in: RD_Pin(32), count: 1
	//   sideset: D0_Pin(16),D4_Pin(20), count: 4
	offset1 = pio_add_program(pio0, &set_pindirs_program);
    //printf("set_pindir: %d\n", offset1);
	set_pindirs_program_init(pio0, 0, offset1, D0_Pin, RD_Pin);
	set_pindirs_program_init(pio0, 1, offset1, D0_Pin + 4, RD_Pin);

	// PIO0:SM2: data_out
	//	 OUT: D0_Pin(16), count: 8
    offset1 = pio_add_program(pio0, &data_out_program);
    data_out_program_init(pio0, 2, offset1, D0_Pin);
    //printf("data_out = %d\n", offset1);

    // PIO0:SM3 ... two/one phase clock generator(program clockgen)
	// 	 SET: BASE: 40(CLK_Pin, inverted), 41(INT_Pin, inverted)
    offset1 = pio_add_program(pio0, &clockgen_program);
    //printf("clockgen: %d\n", offset1);
    clockgen_program_init(pio0, 3, offset1, CLK_Pin, 1);

    // PIO1: SM3 ... IO cycle WAIT handler
    //   SET: BASE: 31(WAIT_Pin)
    //   wait: 24(IORQ_Pin)
    offset1 = pio_add_program(pio1, &iorq_wait_program);
    iorq_wait_program_init(pio1, 3, offset1, WAIT_Pin, D0_Pin);
    //printf("iorq_wait = %d\n", offset1);

    // PIO1: pin assign
	// RD,WR,MREQ,IORQ,WAIT
	pio_gpio_init(pio1, RD_Pin);
	pio_gpio_init(pio1, WR_Pin);
	//pio_gpio_init(pio1, MREQ_Pin);
	pio_gpio_init(pio1, IORQ_Pin);
	pio_gpio_init(pio1, WAIT_Pin);

    // mem clear
    for (int i = 0 ; i < sizeof mem; ++i)
        mem[i] = 0;
    // copy prog1
#ifdef EMUBASIC_IO
    //memcpy(&mem[0], &emuz80_binary[0], sizeof emuz80_binary);
#endif
#ifdef EMUBASIC
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
    mem[1] = 0x76;
#endif
#if 0
    mem[0] = 0xc3;
    mem[1] = 0x00;
    mem[2] = 0x00;
#endif
#if 0
    // jr loop
    mem[0] = 0x18;
    mem[1] = 0xfe;
    mem[2] = 0x18;
    mem[3] = 0xfc;
#endif
#if 0
    // JP 0000H
    mem[0] = 0xc3;  // LD HL, 7F00H
    mem[1] = 0x00;
    mem[2] = 0x00;
    mem[3] = 0x00;
    mem[4] = 0x34;
    mem[5] = 0x34;
    mem[6] = 0x34;
    mem[7] = 0x34;
#endif
#if 0
    // INC (HL), JR 0xfc
    mem[0] = 0x21;  // LD HL, 7F00H
    mem[1] = 0x4f;
    mem[2] = 0x7f;
    mem[3] = 0x34;  // INC (HL)
    mem[4] = 0x18;  // JR
    mem[5] = 0xfd;  // -3
#endif
#if 1
    // inc (hl) loop
    mem[0] = 0x21;
    mem[1] = 0x00;
    mem[2] = 0x01;
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

    // start PIO state machines
    pio_sm_set_enabled(pio0, 0, true);  // set_pindir(low 4 bit)
    pio_sm_set_enabled(pio0, 1, true);  // set_pindir(high 4 bit)
    pio_sm_set_enabled(pio0, 2, true);  // data_out
    pio_sm_set_enabled(pio0, 3, true);  // clockgen
    // need starting clock before iorq_wait start
    sleep_us(10);
    pio_sm_set_enabled(pio1, 3, true);  // iorq_wait
    sleep_us(10);
    pio_sm_clear_fifos(pio0, 2);

    //
    // core1 (bus read/write loop)
    //
    multicore_launch_core1(core1_entry);
    uint32_t g = multicore_fifo_pop_blocking();
    if (g != FLAG_VALUE) {
        printf("core1 start failure, stopping\n");
        while(1);
    } else {
        printf("core1 start, push core0 status!\n");
    }

    multicore_fifo_push_blocking(FLAG_VALUE);   // start core1
    sleep_us(2);

    // start Z80 CPU

    gpio_put(RESET_Pin, true);
    printf("reset High, start\n");

    core0_entry();
    // NOT REACHED
}
