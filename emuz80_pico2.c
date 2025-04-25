#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include "blink.pio.h"

#define CLK_PIN 41

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
#define TEST_PIN 45

static int toggle_value = 0;
#define TOGGLE() do {    gpio_xor_mask64(((uint64_t)1)<<TEST_PIN); } while(0)
//#define TOGGLE() do {    (*(volatile uint32_t *)&(sio_hw->gpio_hi_togl)) = 1; } while(0)
//#define TOGGLE() do { gpio_put(TEST_PIN, (toggle_value ^= 1));    } while(0)

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
    printf("PICO_USE_GPIO_COPROCESSOR = %d\n", PICO_USE_GPIO_COPROCESSOR);
    
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    // TEST pin
    //gpio_set_dir_out_masked64(((uint64_t)1)<<TEST_PIN);
    //gpio_put_masked64(((uint64_t)1)<<TEST_PIN, 0);
    gpio_set_dir(TEST_PIN, true);
    gpio_put(TEST_PIN, 0);

    // PIO Blinking example
    PIO pio = pio0;
    int n = 20;
        
    uint clk_pin = 41;
    uint wait_pin = 31;
    pio_set_gpio_base(pio, 16);
    // pio_set_gpio_base should be invoked before pio_add_program
    uint offset1 = pio_add_program(pio, &clockgen_program);
    //printf("Loaded program at %d\n", offset1);
    clockgen_pin_forever(pio, 0, offset1, clk_pin, 10000);
    uint offset2 = pio_add_program(pio, &wait_control_program);
    //printf("Loaded program at %d\n", offset2);
    wait_control_pin_forever(pio, 1, offset2, wait_pin, 200000);
    // For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio
    gpio_set_function(TEST_PIN, GPIO_FUNC_SIO);
    while (n-- > 0) TOGGLE();
    TOGGLE();
    TOGGLE();
    pio_sm_set_enabled(pio, 0, true);
    pio_sm_set_enabled(pio, 1, true);

    while (true) {
        //printf("H");
        //TOGGLE();
        sleep_ms(1);
    }
}
