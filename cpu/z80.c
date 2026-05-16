// license: BSD 3-Clause License
// copyright-holders: Norihiro Kumagai
//
// gpio config for z80
//
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "tusb.h"
#include "pico/stdio_usb.h"

#include "conf/z80_rp2350b.h"

#include "emuz80.h"
#include "z80_rp2350b.pio.h"

extern volatile int rx_rdy, tx_rdy, rx_data, tx_data, txbuf_full;
//
// No `sm_config_set_in_pin_count()` is provided for RP2040
// So, here we have a stub function
//
#if defined(RP2040)
#define sm_config_set_in_pin_count(c, num)
#endif

void emuz80_gpio_init()
{
    // GPIO Out
    gpio_out_init(WAIT_Pin, true);
    gpio_out_init(RESET_Pin, false);
#if defined(BUSRQ_Pin)
    gpio_out_init(BUSRQ_Pin, true);
#endif
#if defined(INT_Pin)
    gpio_out_init(INT_Pin, false); // INT Pin has an inverter, so negate signal is needed
#endif
#if defined(TEST_Pin)
    gpio_out_init(TEST_Pin, false);
#endif
    // GPIO In
    // MREQ, IORQ, RD, RFSH, M1 are covered by PIO
    //
    gpio_init_mask(ADDR_MASK); // A0-A15 input
    // gpio_init(BUSAK_Pin);
    // gpio_init(MREQ_Pin);
    gpio_init(IORQ_Pin);
    // gpio_init(RFSH_Pin);
    gpio_init(RD_Pin);
    gpio_init(WR_Pin);

    // data bus

    for (int i = 0; i < 8; ++i)
        pio_gpio_init(pio0, D0_Pin + i);
}

void emuz80_pio_init() {
    //
    // PIO StateMachine(SM) initialzation
    //
    uint offset1;

#if defined(RP2350B)
    // pio_set_gpio_base should be invoked before pio_add_program
    pio_set_gpio_base(pio0, 16);
    pio_set_gpio_base(pio1, 16);
#endif //defined(RP2350B)
    //
    // PIO0:SM0,1
	//   in: RD_Pin(16), count: 1
	//   sideset: D0_Pin(24),D4_Pin(28), count: 4
    printf("---start---\n");
	offset1 = pio_add_program(pio0, &set_pindirs_program);
    printf("set_pindir: %d\n", offset1);
	set_pindirs_program_init(pio0, 0, offset1, D0_Pin, RD_Pin);
	set_pindirs_program_init(pio0, 1, offset1, D0_Pin + 4, RD_Pin);

	// PIO0:SM2: data_out
	//	 OUT: D0_Pin(24), count: 8
    offset1 = pio_add_program(pio0, &data_out_program);
    data_out_program_init(pio0, 2, offset1, D0_Pin);
    printf("data_out = %d\n", offset1);

    // PIO0:SM3 ... two/one phase clock generator(program clockgen)
	// 	 SET: BASE: 40(CLK_Pin, inverted), 41(INT_Pin, inverted)
    offset1 = pio_add_program(pio0, &clockgen_program);
    printf("clockgen: %d\n", offset1);
    clockgen_program_init(pio0, 3, offset1, CLK_Pin, 1);

    // PIO1: SM3 ... IO cycle WAIT handler
    //   SET: BASE: 19(WAIT_Pin)
    //   wait: 18(IORQ_Pin)
    offset1 = pio_add_program(pio1, &iorq_wait_program);
    iorq_wait_program_init(pio1, 3, offset1, WAIT_Pin, D0_Pin);
    printf("iorq_wait = %d\n", offset1);

    // PIO1: pin assign
	// RD,WR,MREQ,IORQ,WAIT
	pio_gpio_init(pio1, RD_Pin);
	pio_gpio_init(pio1, WR_Pin);
	//pio_gpio_init(pio1, MREQ_Pin);
	pio_gpio_init(pio1, IORQ_Pin);
	pio_gpio_init(pio1, WAIT_Pin);

    // input override
    // These should be below pio_gpio_init
#if defined(RP2040)
    for (int i = WAIT_Pin; i < 30 ; i++) {
        printf ("inover: %d\n", i);
        gpio_set_input_enabled(i, false);
        gpio_set_inover(i, GPIO_OVERRIDE_LOW);
        gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
    }
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
}

void emuz80_unreset(void) {
    gpio_put(RESET_Pin, true);
    printf("reset High, start\n");   
}

// コア1のエントリポイント
// core1_entry()はPIOの状態マシンを実行し、ROMデータを送信する
__attribute__((noinline)) void __time_critical_func(emuz80_core1_entry)(void)
{
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
    while (((port = gpio_get_all()) & ((1 << IORQ_Pin) | (1 << WR_Pin))) == ((1 << IORQ_Pin) | (1 << WR_Pin)))
    {
        // All other cycles, except neither IORQ nor WR.
        // output mem[addr] asynchronously
        // TOGGLE();
        pio_sm_put(pio0, 2, mem[port & ADDR_MASK]);
        // TOGGLE();
    }
    port = gpio_get_all(); // re-read to confirm status lines
    if ((port & ((1 << IORQ_Pin) | (1 << WR_Pin))) == (1 << IORQ_Pin))
    {
        // Memory Write Cycle
        // store data to mem[addr], asynchronously
        TOGGLE();
        mem[port & ADDR_MASK] = (port >> D0_Pin);
        TOGGLE();
        goto loop;
    }
    // port = gpio_get_all();      // re-read to confirm status lines
    if ((port & (1 << IORQ_Pin)) == 0)
    {
        if ((port & (1 << RD_Pin)) == 0)
        {
            // IO Read cycle
            // EMUZ80 UART emulation
            addr = port & 0xff;
            // UARTCR or DR
            if (addr == 1)
            {
                // read status register
                status = (rx_rdy | tx_rdy);
                pio_sm_put(pio0, 2, status);
            }
            else if (addr == 0)
            {
                // read data register
                uint8_t data = rx_data;
                rx_rdy = 0;
                pio_sm_put(pio0, 2, data);
            }
        }
        else if ((port & (1 << WR_Pin)) == 0)
        {
            // IO Write cycle
            addr = port & 0xff;
            data = ((port >> D0_Pin) & 0xff);
            if (addr == 0)
            {
                // UART DR
                tx_data = data;
                txbuf_full = 1;
            }
        }
        pio_sm_put(pio1, 3, 0);       // notify IO process finished to the state machine
        pio_sm_get_blocking(pio1, 3); // wait for WAIT set High
        while (((port = gpio_get_all()) & (1 << IORQ_Pin)) == 0)
            ; // wait for cycle end
              // wait for IORQ is High
        goto loop;
    }
    goto loop;
}
