#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include "blink.pio.h"

#define WAIT_Pin 31
#define M1_Pin   28
#define RD_Pin   27
#define RFSH_Pin 26
#define MREQ_Pin 25
#define RESET_Pin 42
#define BUSAK_Pin 29
#define BUSRQ_Pin 43
#define INT_Pin  41
#define CLK_Pin  40
#define TEST_Pin 45

void clockgen_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint count) {
    clockgen_program_init(pio, sm, offset, pin);
    //pio_sm_set_enabled(pio, sm, true);

    //printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    //pio->txf[sm] = (125000000 / (2 * freq)) - 3;
    pio->txf[sm] = count;
}

void wait_control_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    wait_control_program_init(pio, sm, offset, pin);
    //pio_sm_set_enabled(pio, sm, true);

    //printf("Blinking pin %d at %d Hz\n", pin, freq);

    // PIO counter program takes 3 more cycles in total than we pass as
    // input (wait for n + 1; mov; jmp)
    pio->txf[sm] = (125000000 / (2 * freq)) - 3;
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
    gpio_out_init(RESET_Pin, true);
    gpio_out_init(BUSRQ_Pin, true);
    gpio_out_init(INT_Pin, false);      // INT Pin has an inverter, so negate signal is needed

    gpio_out_init(TEST_Pin, true);
    TOGGLE();
    TOGGLE();
    //TOGGLE();

    // GPIO In
    // MREQ, IORQ, RD, RFSH, M1 are covered by PIO
    // 
    gpio_init(BUSAK_Pin);
    gpio_init(MREQ_Pin);

    // PIO Blinking example
    PIO pio = pio0;
    int n = 100;
        
    //uint clk_pin = 41;
    //uint wait_pin = 31;
    pio_set_gpio_base(pio, 16);
    // pio_set_gpio_base should be invoked before pio_add_program
    uint offset1 = pio_add_program(pio, &clockgen_program);
    //printf("Loaded program at %d\n", offset1);
    clockgen_pin_forever(pio, 0, offset1, CLK_Pin, 1000);
    uint offset2 = pio_add_program(pio, &wait_control_program);
    //printf("Loaded program at %d\n", offset2);
    wait_control_pin_forever(pio, 1, offset2, WAIT_Pin, 200000);
    // For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio
    //while (n-- > 0) TOGGLE();
    TOGGLE();
    TOGGLE();
    // RESET assert
    gpio_put(RESET_Pin, false); 
    // start clock
    pio_sm_set_enabled(pio, 0, true);
    pio_sm_set_enabled(pio, 1, true);

    TOGGLE();
    TOGGLE();
    gpio_put(RESET_Pin, true);
    TOGGLE();

    while (true) {
        //printf("H");
        //TOGGLE();
        sleep_ms(1);
    }
}
