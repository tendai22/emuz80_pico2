#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

//
// Pin Definitions
// This section should be located
// before #include "blink.pio.h"
//
#define DEBUG_Pin 31
#define WAIT_Pin 30
#define RFSH_Pin 28
#define M1_Pin   27
#define RD_Pin   26
#define MREQ_Pin 25
#define IORQ_Pin 24
#define RESET_Pin 42
#define BUSAK_Pin 29
#define BUSRQ_Pin 43
#define INT_Pin  41
#define CLK_Pin  40
#define TEST_Pin 45

#include "blink.pio.h"

void clockgen_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint phase) {
    clockgen_program_init(pio, sm, offset, pin, phase);
}

void wait_mreq_forever(PIO pio, uint sm, uint offset, uint pin) {
    wait_mreq_program_init(pio, sm, offset, pin);
}

void wait_iorq_forever(PIO pio, uint sm, uint offset, uint pin) {
    wait_iorq_program_init(pio, sm, offset, pin);
}

void wait_access_forever(PIO pio, uint sm, uint offset) {
    wait_access_program_init(pio, sm, offset);
}

void databus_forever(PIO pio, uint sm, uint offset, uint debug_pin) {
    databus_program_init(pio, sm, offset, debug_pin);
}

// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart0
#define BAUD_RATE 115200

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 46 //0
#define UART_RX_PIN 47 //1

static int toggle_value = 1;
#define TOGGLE() do {    gpio_xor_mask64(((uint64_t)1)<<TEST_Pin); } while(0)
//#define TOGGLE() do {    (*(volatile uint32_t *)&(sio_hw->gpio_hi_togl)) = 1; } while(0)
//#define TOGGLE() do { gpio_put(TEST_Pin, (toggle_value ^= 1));    } while(0)

void gpio_out_init(uint gpio, bool value) {
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, value);
    gpio_set_function(gpio, GPIO_FUNC_SIO);
}

uint8_t mem[65536];

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
    uart_puts(UART_ID, " Hello, UART!\r\n");
    printf("PICO_USE_GPIO_COPROCESSOR = %d, PICO_PIO_USE_GPIO_BASE = %d\n", PICO_USE_GPIO_COPROCESSOR, PICO_PIO_USE_GPIO_BASE);
    
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    // TEST pin
    //gpio_set_dir_out_masked64(((uint64_t)1)<in);
    //gpio_put_masked64(((uint64_t)1)<<TEST_Pin, 0);

    // Z80 Input pin initialize

    // GPIO Out
    gpio_out_init(WAIT_Pin, true);
    gpio_out_init(RESET_Pin, false);
    gpio_out_init(BUSRQ_Pin, true);
    //gpio_out_init(INT_Pin, false);      // INT Pin has an inverter, so negate signal is needed

    gpio_out_init(TEST_Pin, true);
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

    // PIO Blinking example
    PIO pio_clock = pio0;
    PIO pio_wait = pio1;
    uint sm_clock = 0;
        
    pio_set_gpio_base(pio_clock, 16);
    pio_set_gpio_base(pio_wait, 16);
    // pio_set_gpio_base should be invoked before pio_add_program
    uint offset1;
    offset1 = pio_add_program(pio_clock, &clockgen_program);
    clockgen_pin_forever(pio_clock, sm_clock, offset1, CLK_Pin, 1);

    offset1 = pio_add_program(pio_wait, &wait_mreq_program);
    wait_mreq_forever(pio_wait, 0, offset1, WAIT_Pin);
    offset1 = pio_add_program(pio_wait, &wait_iorq_program);
    wait_iorq_forever(pio_wait, 1, offset1, WAIT_Pin);
    offset1 = pio_add_program(pio_wait, &databus_program);
    wait_access_forever(pio_wait, 2, offset1);

    offset1 = pio_add_program(pio, &databus_program);
    databus_forever(pio, 1, offset1, DEBUG_Pin);
    // For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio
    //int n = 10;
    //while (n-- > 0) TOGGLE();
    TOGGLE();
    TOGGLE();

    // mem clear
    for (int i = 0 ; i < sizeof mem; ++i)
        mem[i] = 0;
    // start clock
    // start clock
    pio_sm_set_enabled(pio_clock, sm_clock, true);
    pio_sm_set_enabled(pio_wait, 0, true);
    pio_sm_set_enabled(pio_wait, 1, true);  //IORQ not yet be ready 
    pio_sm_set_enabled(pio_wait, 2, true);
    sleep_us(5);
    TOGGLE();
    TOGGLE();

    gpio_put(RESET_Pin, true);

    uint32_t addr, data, status;
    while(true) {
        // Wait for PIO1:IRQ0 High
        while ((pio_wait->irq & 0x1) == 0)
            ;
        status = (gpio_get_all() >> 24) & 0xf;  // IORQ,MREQ,RD,M1
        data = pio_sm_get_blocking(pio, 1);    // wait for access event occurs
        if (status & 0x4) {     // Z80 Write mode: RD_Pin High
            // Z80 Write
            TOGGLE();
            addr = gpio_get_all() & 0xffff;     // A0-A15 ... GPIO0-15
            mem[addr] = data;
            pio_sm_put(pio, 1, data);   // notify end of process
            TOGGLE();
        } else {            // Z80 Read mode: RD_Pin Low
            // Z80 Read
            TOGGLE();
            addr = gpio_get_all() & 0xffff;     // A0-A15 ... GPIO0-15
            data = mem[addr];
            pio_sm_put(pio, 1, data);
            TOGGLE();
        }
    }
}
