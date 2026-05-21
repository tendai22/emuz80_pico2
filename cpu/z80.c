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
#include "hardware/dma.h"
#include "hardware/resets.h"
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
    for (int i = 0; i < 16; ++i)
        pio_gpio_init(pio0, A0_Pin + i);
}

void emuz80_pio_init() {
    //
    // PIO StateMachine(SM) initialzation
    //
    uint offset1;

#if defined(RP2350B)
    // pio_set_gpio_base should be invoked before pio_add_program
    //pio_set_gpio_base(pio0, 16);
    pio_set_gpio_base(pio1, 16);
#endif //defined(RP2350B)
    pio_sm_config c;
    // PIO0:SM0 ... ram_read
    //   IN: A0-A15(0), IN_COUNT: 16
    //   OUT/MOV: D0-Pin(24), OUT_COUNT: 8(D0-D7)
    //   JMP_PIN: RD_Pin(20)
    //   SET_BASE: 31 (debug pin)
    printf("---start---\n");
	offset1 = pio_add_program(pio0, &ram_read_program);
    printf("ram_read: %d\n", offset1);
    //pio_sm_set_consecutive_pindirs(pio0, 0, RD_Pin, 1, false);
    pio_sm_set_consecutive_pindirs(pio0, 0, A0_Pin, 21, false);
    c = ram_read_program_get_default_config(offset1);
    sm_config_set_in_pins(&c, A0_Pin);
    sm_config_set_in_pin_count(&c, 16);
    sm_config_set_in_shift(&c, false, true, 16);    // 16bit autopush
    sm_config_set_out_pins(&c, D0_Pin, 8);
    //sm_config_set_out_pin_count(&c, 8);
    sm_config_set_out_shift(&c, true, false, 8);    // 8bit no-autopush
    sm_config_set_jmp_pin(&c, RD_Pin);
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio0, 0, offset1, &c);

    // PIO0:SM1 ... ram_write_addr
    //   IN: A0-A15(0), IN_COUNT: 16, autopush
    //   OUT/MOV: D0-Pin(24), OUT_COUNT: 8
    //   JMP_PIN: WR_Pin(21)
	offset1 = pio_add_program(pio0, &ram_write_addr_program);
    printf("ram_write_addr: %d\n", offset1);
    pio_sm_set_consecutive_pindirs(pio0, 1, WR_Pin, 1, false);
    c = ram_write_addr_program_get_default_config(offset1);
    sm_config_set_in_pins(&c, A0_Pin);
    sm_config_set_in_pin_count(&c, 16);
    sm_config_set_in_shift(&c, false, true, 16);    // 16bit autopush
    sm_config_set_out_pins(&c, D0_Pin, 8);
    //sm_config_set_out_pin_count(&c, 8);
    sm_config_set_out_shift(&c, true, false, 8);    // 8bit no-autopush
    sm_config_set_jmp_pin(&c, WR_Pin);
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio0, 1, offset1, &c);

    // PIO0:SM2: ram_write_data
    //   IN:  D0_Pin(24), count: 8, autopush
    offset1 = pio_add_program(pio0, &ram_write_data_program);
    printf("ram_write_data: %d\n", offset1);
    pio_sm_set_consecutive_pindirs(pio0, 2, D0_Pin, 8, false);  // data as input
    c = ram_write_data_program_get_default_config(offset1);
    sm_config_set_in_pins(&c, D0_Pin);
    sm_config_set_in_pin_count(&c, 8);
    sm_config_set_in_shift(&c, false, false, 32);   // no-autopush
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio0, 2, offset1, &c);
 
    // PIO1:SM2 ... two/one phase clock generator(program clockgen)
	// 	 SET: BASE: 40(CLK_Pin, inverted), 41(INT_Pin, inverted)
    offset1 = pio_add_program(pio1, &clockgen_program);
    printf("clockgen: %d\n", offset1);
    //clockgen_program_init(pio0, 3, offset1, CLK_Pin, 1);
    int phase = 1;
    pio_gpio_init(pio1, CLK_Pin);
    if (phase == 2)
        pio_gpio_init(pio1, CLK_Pin + 1);
    pio_sm_set_consecutive_pindirs(pio1, 2, CLK_Pin, phase, true);
    c = clockgen_program_get_default_config(offset1);
    // set_set_pin_base should have been adjusted by pio->gpiobase
    // so far not so in set_set_pin_base();
    sm_config_set_set_pins(&c, CLK_Pin, phase);
    // two-phase: (4 instruction loop)
    //  16.0 ... 2.33MHz (420ns/cycle)
    //   9.42 ... 4.0MHz  (250ns/cycle)
    // single clock: (2 instruction loop)
    //  50.0 ... 1.5MHz (660-670ns) 
    //  30.0 ... 2.5MHz (400ns) ... no wait, 
    //  18.7 ... 4.0-4.17MHz (250-260ns) .... 0/1 wait in M1, 1 wait in WR, 0/1 wait in RD
    //  10.0 ... 7.1-7.6MHz (130-140ns) ... seems to work
    //   5.0 ... 14-16MHz (60-70ns) ... does not works
    sm_config_set_clkdiv(&c, 30000); // 12.0 ... 6.25MHz max
    pio_sm_init(pio1, 2, offset1, &c);


    // PIO1: SM3 ... IO cycle WAIT handler
    //   SET: BASE: 19(WAIT_Pin)
    //   wait: 18(IORQ_Pin)
    offset1 = pio_add_program(pio1, &iorq_wait_program);
    //iorq_wait_program_init(pio1, 3, offset1, WAIT_Pin, D0_Pin);
	//   IN: IORQ_Pin(18), count 1
	//	 SET: WAIT_Pin(19), count: 1
    //pio_sm_set_consecutive_pindirs(pio, sm, iorq_pin, 1, false);
    pio_sm_set_consecutive_pindirs(pio1, 3, WAIT_Pin, 1, true);
    c = iorq_wait_program_get_default_config(offset1);
    sm_config_set_in_pins(&c, D0_Pin);
    sm_config_set_in_pin_count(&c, 8);
    sm_config_set_in_shift(&c, false, false, 8);
    sm_config_set_out_pins(&c, D0_Pin, 8);
    sm_config_set_out_shift(&c, true, false, 8);
    sm_config_set_set_pins(&c, WAIT_Pin, 1);
    sm_config_set_set_pin_count(&c, 1);
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio1, 3, offset1, &c);

    printf("iorq_wait = %d\n", offset1);

    // PIO0: pin assign
	// RD,WR,MREQ,IORQ,WAIT
	pio_gpio_init(pio0, RD_Pin);
	pio_gpio_init(pio0, WR_Pin);
    // PIO1: pin assign
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
    pio_sm_set_enabled(pio0, 0, true);  // ram_read
    pio_sm_set_enabled(pio0, 1, true);  // ram_write_addr
    pio_sm_set_enabled(pio0, 2, true);  // ram_write_data
    pio_sm_set_enabled(pio1, 2, true);  // clockgen
    // need starting clock before iorq_wait start
    sleep_us(10);
    pio_sm_set_enabled(pio1, 3, true);  // iorq_wait
    sleep_us(10);
    pio_sm_clear_fifos(pio0, 0);
    pio_sm_clear_fifos(pio0, 1);
    pio_sm_clear_fifos(pio0, 2);
}

void emuz80_unreset(void) {
    gpio_put(RESET_Pin, true);
    printf("reset High, start\n");   
}

void emuz80_dma_init()
{
    int ch_r_addr = dma_claim_unused_channel(true);
    int ch_r_data = dma_claim_unused_channel(true);
    int ch_w_addr = dma_claim_unused_channel(true);
    int ch_w_data = dma_claim_unused_channel(true);

    // Ch_R_Data: RAM -> PIO TX FIFO
    dma_channel_config cr_data = dma_channel_get_default_config(ch_r_data);
    channel_config_set_transfer_data_size(&cr_data, DMA_SIZE_8);
    channel_config_set_read_increment(&cr_data, false);
    channel_config_set_write_increment(&cr_data, false);
    channel_config_set_dreq(&cr_data, pio_get_dreq(pio0, 0, true));
    dma_channel_configure(ch_r_data, &cr_data, &pio0->txf[0], mem, 1, false);

    // Ch_R_Addr: PIO RX FIFO -> READ_ADDR Register in CH_R_data (16bit ring buffer)
    dma_channel_config cr_addr = dma_channel_get_default_config(ch_r_addr);
    channel_config_set_transfer_data_size(&cr_addr, DMA_SIZE_16);
    channel_config_set_read_increment(&cr_addr, false);
    channel_config_set_write_increment(&cr_addr, false);
    channel_config_set_dreq(&cr_addr, pio_get_dreq(pio0, 0, true));
    channel_config_set_chain_to(&cr_addr, ch_r_data);
    channel_config_set_ring(&cr_addr, true, 2);     // write to lower 2 byte
    dma_channel_configure(ch_r_addr, &cr_addr, &dma_hw->ch[ch_r_data].al1_read_addr, 
        &pio0->rxf[0], 1, true);

    // Ch W_Data: PIO RX FIFO -> RAM
    dma_channel_config cw_data = dma_channel_get_default_config(ch_w_data);
    channel_config_set_transfer_data_size(&cw_data, DMA_SIZE_8);
    channel_config_set_read_increment(&cw_data, false);
    channel_config_set_write_increment(&cw_data, false);
    channel_config_set_dreq(&cw_data, pio_get_dreq(pio0, 2, false));
    dma_channel_configure(ch_w_data, &cw_data, mem, &pio0->rxf[1], 1, false);
    
    // Ch W_Addr: PIO RX FIFO -> WRITE_ADDR register in Ch W_Data (ring buffer)
    dma_channel_config cw_addr = dma_channel_get_default_config(ch_w_addr);
    channel_config_set_transfer_data_size(&cw_addr, DMA_SIZE_16);
    channel_config_set_write_increment(&cw_addr, false);
    channel_config_set_dreq(&cw_addr, pio_get_dreq(pio0, 1, false));
    channel_config_set_chain_to(&cw_addr, ch_w_data);
    channel_config_set_ring(&cw_addr, true, 2);     // write to lower 2 byte
    dma_channel_configure(ch_w_addr, &cw_addr, &dma_hw->ch[ch_w_data].al2_write_addr_trig, 
        &pio0->rxf[1], 1, false);
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
    uint32_t a32;

    multicore_fifo_push_blocking(FLAG_VALUE);
    uint32_t g = multicore_fifo_pop_blocking();
loop:
    while (((port = gpio_get_all()) & (1 << RD_Pin)) != 0)
        ;
    a32 = pio_sm_get_blocking(pio0, 0);
    printf("%08lX: %08lX\n", a32, port);
    goto loop;
    while (((port = gpio_get_all()) & (1 << IORQ_Pin)) != 0)
        ;
    if ((port & (1 << IORQ_Pin)) == 0) {
        if ((port & (1 << RD_Pin)) == 0) {
            // IO Read cycle
            // EMUZ80 UART emulation
            addr = port & 0xff;
            // UARTCR or DR
            if (addr == 1) {
                // read status register
                status = (rx_rdy | tx_rdy);
                pio_sm_put(pio0, 2, status);
            } else if (addr == 0) {
                // read data register
                uint8_t data = rx_data;
                rx_rdy = 0;
                pio_sm_put(pio0, 2, data);
            }
        } else if ((port & (1 << WR_Pin)) == 0) {
            // IO Write cycle
            addr = port & 0xff;
            data = ((port >> D0_Pin) & 0xff);
            if (addr == 0) {
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
    }
    goto loop;
}
