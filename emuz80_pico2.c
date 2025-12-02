#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

//
// Pin Definitions
// This section should be located
// before #include "blink.pio.h"
//
#define D0_Pin 16
#define RD_Pin   32
#define WR_Pin   33
#define MREQ_Pin 34
#define IORQ_Pin 35
#define DEBUG_Pin 36
#define WAIT_Pin 31
#define RFSH_Pin 28
#define M1_Pin   27
#define RESET_Pin 42
#define BUSAK_Pin 29
#define BUSRQ_Pin 43
#define INT_Pin  41
#define CLK_Pin  40
#define TEST_Pin 44

#include "blink.pio.h"

//void clockgen_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint phase) {
//    clockgen_program_init(pio, sm, offset, pin, phase);
//}

//void wait_mreq_forever(PIO pio, uint sm, uint offset, uint rfsh_pin) {
//    wait_mreq_program_init(pio, sm, offset, rfsh_pin);
//}

//void wait_iorq_forever(PIO pio, uint sm, uint offset, uint pin) {
//    wait_iorq_program_init(pio, sm, offset, pin);
//}

//void wait_access_forever(PIO pio, uint sm, uint offset) {
//    wait_access_program_init(pio, sm, offset);
//}

//void databus_forever(PIO pio, uint sm, uint offset, uint wait_pin) {
//    databus_program_init(pio, sm, offset, wait_pin);
//}

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart0
#define BAUD_RATE 115200

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 46 //0
#define UART_RX_PIN 47 //1

static int toggle_value = 1;
#define TOGGLE() do {    gpio_xor_mask64(((uint64_t)1)<<TEST_Pin); sleep_us(1); } while(0)
//#define TOGGLE() do {    (*(volatile uint32_t *)&(sio_hw->gpio_hi_togl)) = 1; } while(0)
//#define TOGGLE() do { gpio_put(TEST_Pin, (toggle_value ^= 1));    } while(0)

void gpio_out_init(uint gpio, bool value) {
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, value);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
}

uint8_t mem[65536];

uint8_t prog1[] = {
0x31, 0x00, 0x80,
0x3A, 0x01, 0xE0,
0xCB, 0x47,
0x28, 0xF9,
0x3A, 0x00, 0xE0,
0xFE, 0x61,
0x38, 0x06,
0xFE, 0x7B,
0x30, 0x02,
0xE6, 0xDF,
0xF5,
0x3A, 0x01, 0xE0,
0xCB, 0x4F,
0x28, 0xF9,
0xF1,
0x32, 0x00, 0xE0,
0x18, 0xDE,
};

#define EMUBASIC 1
#include "emuz80.h"

int main()
{
    stdio_init_all();

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    
    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions
    //uart_puts(UART_ID, " Hello, UART!\r\n");
    //printf("PICO_USE_GPIO_COPROCESSOR = %d, PICO_PIO_USE_GPIO_BASE = %d\n", PICO_USE_GPIO_COPROCESSOR, PICO_PIO_USE_GPIO_BASE);
    
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    // TEST pin
    //gpio_set_dir_out_masked64(((uint64_t)1)<in);
    //gpio_put_masked64(((uint64_t)1)<<TEST_Pin, 0);

    // Z80 Input pin initialize

    // GPIO Out
    gpio_out_init(WAIT_Pin, true);
    gpio_out_init(RESET_Pin, false);
    gpio_out_init(BUSRQ_Pin, true);
    gpio_out_init(INT_Pin, false);      // INT Pin has an inverter, so negate signal is needed

    gpio_out_init(TEST_Pin, false);
    TOGGLE();
    TOGGLE();
    //TOGGLE();

    // GPIO In
    // MREQ, IORQ, RD, RFSH, M1 are covered by PIO
    //
    gpio_init_mask(0xffff);     // A0-A15 input 
    gpio_init(BUSAK_Pin);
    gpio_init(MREQ_Pin);
    gpio_init(IORQ_Pin);
    gpio_init(RFSH_Pin);
    gpio_init(RD_Pin);
    gpio_init(WR_Pin);

    // PIO Blinking example
    PIO pio_clock = pio0;
    PIO pio_wait = pio1;
    //uint sm_clock = 0;

    pio_set_gpio_base(pio0, 16);
    pio_set_gpio_base(pio1, 16);
    // pio_set_gpio_base should be invoked before pio_add_program
    uint offset1;

	// data bus

	for (int i = 0; i < 8; ++i)
        pio_gpio_init(pio0, D0_Pin + i);
	pio_gpio_init(pio0, RD_Pin);

	// PIO0:SM0,1
	//   in: RD_Pin(32), count: 1
	//   sideset: D0_Pin(16),D4_Pin(20), count: 4
    printf("---start---\n");
	offset1 = pio_add_program(pio0, &set_pindirs_program);
    printf("set_pindir: %d\n", offset1);
	set_pindirs_program_init(pio0, 0, offset1, D0_Pin, RD_Pin);
	set_pindirs_program_init(pio0, 1, offset1, D0_Pin + 4, RD_Pin);

	// PIO1:SM0: data_out
	//	 OUT: D0_Pin(16), count: 8
    offset1 = pio_add_program(pio0, &data_out_program);
    data_out_program_init(pio0, 2, offset1, D0_Pin);
    printf("data_out = %d\n", offset1);

    // PIO0:SM3 ... two/one phase clock generator(program clockgen)
	// 	 SET: BASE: 40(CLK_Pin, inverted), 41(INT_Pin, inverted)
    offset1 = pio_add_program(pio0, &clockgen_program);
    printf("clockgen: %d\n", offset1);
    clockgen_program_init(pio0, 3, offset1, CLK_Pin, 1);

	// PIO1: pin assign
	// D0-D7
	// RD,WR,MREQ,IORQ
	//for (int i = 0; i < 8; ++i)
    //    pio_gpio_init(pio1, D0_Pin + i);
	pio_gpio_init(pio1, RD_Pin);
	pio_gpio_init(pio1, WR_Pin);
	pio_gpio_init(pio1, MREQ_Pin);
	pio_gpio_init(pio1, IORQ_Pin);

	// PIO1:SM1: data_in
	//	 IN: D0_Pin(16), count: 8
    offset1 = pio_add_program(pio1, &data_in_program);
    data_in_program_init(pio1, 1, offset1, D0_Pin);
    printf("data_in = %d\n", offset1);

	// PIO1:SM2: control_in
	//	 IN: RD_Pin(32), count: 4 (RD,WR,MREQ,IORQ)
    offset1 = pio_add_program(pio1, &control_in_program);
    control_in_program_init(pio1, 2, offset1, RD_Pin);
    printf("control_in = %d\n", offset1);

	// PIO1:SM3: iorq_wait
	//   IN: IORQ_Pin, count 1
	//	 SET: WAIT_Pin(31), count: 1
    offset1 = pio_add_program(pio1, &iorq_wait_program);
    iorq_wait_program_init(pio1, 3, offset1, D0_Pin, WAIT_Pin);
    printf("iorq_wait = %d\n", offset1);
	// For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio
    //int n = 10;
    //while (n-- > 0) TOGGLE();
    TOGGLE();
    TOGGLE();

    // mem clear
    for (int i = 0 ; i < sizeof mem; ++i)
        mem[i] = 0;
    // copy prog1
    //memcpy(&mem[0], &emuz80_binary[0], sizeof emuz80_binary);
	//memcpy(&mem[0], &prog1[0], sizeof prog1);
    // debug Z80 codes
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
#if 0
    // halt
    mem[2] = 0x76;
#endif
#if 0
    // jr loop
    mem[0] = 0x18;
    mem[1] = 0xfe;
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

    // start clock
    // start clock
    pio_sm_set_enabled(pio0, 0, true);
    pio_sm_set_enabled(pio0, 1, true);
    pio_sm_set_enabled(pio0, 2, true);
    pio_sm_set_enabled(pio0, 3, true);
    //pio_sm_set_enabled(pio1, 1, true);
    //pio_sm_set_enabled(pio1, 2, true);
    //pio_sm_set_enabled(pio1, 3, true);
    sleep_us(10);
    TOGGLE();
    TOGGLE();

    pio_sm_clear_fifos(pio0, 2);
    gpio_put(RESET_Pin, true);

    register uint32_t addr, status;
    register uint32_t data;
    int32_t count = 100;
    register uint8_t c = 0;
    while(true) {
        pio_sm_put(pio0, 2, c++);
    }
    //
    //
    //  
    while(true) {
        if (pio_sm_is_rx_fifo_empty(pio_wait, 2) == 0) {
            data = pio_sm_get_blocking(pio_wait, 2);    // wait for access event occurs
            status = (gpio_get_all() >> 24) & 0xf;  // IORQ,MREQ,RD,M1
            if ((status & 0x4) == 0) {     // Z80 Read
                // Z80 Read mode: RD_Pin Low
                TOGGLE();
                addr = (gpio_get_all() & 0xffff);
                data = mem[addr];
                if ((addr & 0xfffe) == 0xe000) {
                    // UARTCR or DR
                    if (addr == 0xe001) {
                        uint8_t status = 0;
                        if (uart_is_readable(UART_ID))
                            status |= (1<<0);
                        if (uart_is_writable(UART_ID))
                            status |= (1<<1);
                        data = status;
                    } else if (addr == 0xe000) {
                        data = 0;
                        if (uart_is_readable(UART_ID))
                            data = uart_getc(UART_ID);
                    }
                }
                //if (count-- > 0) printf("%04X: %02X RD\r\n", addr, data);
                //if (addr == 0xe000) printf("%04X: %02X RD\r\n", addr, data);
                pio_sm_put(pio_wait, 2, data);
                //pio_sm_put(pio_wait, 2, 0);
                TOGGLE();
            } else {
                // Z80 Write mode: RD_Pin High
                TOGGLE();
                addr = gpio_get_all() & 0xffff;     // A0-A15 ... GPIO0-15
                data = (gpio_get_all() & 0xff0000) >> 16;
                if ((addr & 0xfffe) != 0xe000) 
                    mem[addr] = data;
                else {
                    // UART DR
                    if (addr == 0xe000) {
                        if (uart_is_writable(UART_ID))
                            uart_putc_raw(UART_ID, data);
                    }
                }
                //if (count-- > 0) printf("%04X: %02X WR\r\n", addr, data);
                //sleep_ms(500);
                pio_sm_put(pio_wait, 2, 21);   // notify end of process
                TOGGLE();
            }
        }
    }
}
