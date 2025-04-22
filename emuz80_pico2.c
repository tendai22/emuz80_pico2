#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"

#include "blink.pio.h"

#define CLK_PIN 41

void blink_pin_forever(PIO pio, uint sm, uint offset, uint pin, uint freq) {
    printf("2 gpiobase = %d\n", pio_get_gpio_base(pio));
    blink_program_init(pio, sm, offset, pin);
    pio_sm_set_enabled(pio, sm, true);

    printf("Blinking pin %d at %d Hz\n", pin, freq);

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

int my_pio_sm_set_consecutive_pindirs(PIO pio, uint sm, uint pin, uint count, bool is_out) {
    check_pio_param(pio);
    check_sm_param(sm);
    pin -= pio_get_gpio_base(pio);
    printf("pinbase(%p) = %d\n", &(pio->gpiobase), pio->gpiobase);
    printf("pin  = %d\n", pin);
    invalid_params_if_and_return(PIO, pin >= 32u, PICO_ERROR_INVALID_ARG);
    uint32_t pinctrl_saved = pio->sm[sm].pinctrl;
    uint32_t execctrl_saved = pio->sm[sm].execctrl;
    hw_clear_bits(&pio->sm[sm].execctrl, 1u << PIO_SM0_EXECCTRL_OUT_STICKY_LSB);
    uint pindir_val = is_out ? 0x1f : 0;
    while (count > 5) {
        pio->sm[sm].pinctrl = (5u << PIO_SM0_PINCTRL_SET_COUNT_LSB) | (pin << PIO_SM0_PINCTRL_SET_BASE_LSB);
        pio_sm_exec(pio, sm, pio_encode_set(pio_pindirs, pindir_val));
        count -= 5;
        pin = (pin + 5) & 0x1f;
    }
    pio->sm[sm].pinctrl = (count << PIO_SM0_PINCTRL_SET_COUNT_LSB) | (pin << PIO_SM0_PINCTRL_SET_BASE_LSB);
    printf("pinctrl(%p): %08x\n", &(pio->sm[sm].pinctrl), pio->sm[sm].pinctrl);
    printf("pio_sm_exec: %08x\n", pio_encode_set(pio_pindirs, pindir_val));
    pio_sm_exec(pio, sm, pio_encode_set(pio_pindirs, pindir_val));
    pio->sm[sm].pinctrl = pinctrl_saved;
    pio->sm[sm].execctrl = execctrl_saved;
    return PICO_OK;
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
    
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    // PIO Blinking example
    PIO pio = pio0;
    uint base = 0, clk_pin = 41;
    if (clk_pin >= 32)
        base = 16;
    pio_set_gpio_base(pio, base);
    uint offset = pio_add_program(pio, &blink_program);
    printf("Loaded program at %d\n", offset);
        
    //#ifdef CLK_PIN
    printf("1 gpiobase = %d\n", pio_get_gpio_base(pio));

    blink_pin_forever(pio, 0, offset, clk_pin, 2);
    //#else
    //blink_pin_forever(pio, 0, offset, 6, 3);
    //#endif
    // For more pio examples see https://github.com/raspberrypi/pico-examples/tree/master/pio
    
    while (true) {
        printf("H");
        sleep_ms(1000);
    }
}
