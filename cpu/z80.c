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
#include "hardware/regs/addressmap.h"
#include "tusb.h"
#include "pico/stdio_usb.h"

#include "emuz80.h"

#if defined(RP2350B)
#include "conf/z80_rp2350b.h"
#include "z80_rp2350b.pio.h"
#endif

#if defined(RP2040)
#include "conf/z80_rp2040.h"
#include "z80_rp2040.pio.h"
#endif
//
// configure switches
//
#define USE_DMA

//
// Narrow IO Register write
//
// By ORing bit14 of the write_addr register alias, 
// in DMA write operation with size DMA_SIZE_16 or DMA_SIZE_8,
// only the corresponding width (bytes) of the target register is written.
//
// Actually, this feature seems not to work well, so I decide to forget it
//#if !defined(REG_ALIAS_NARROW_ZERO_BITS)
//#define REG_ALIAS_NARROW_ZERO_BITS (_u(0x1)<<_u(14))
//#endif
//#define hw_narrow_zero_alias(p) ((void *)((unsigned long int)(p)|REG_ALIAS_NARROW_ZERO_BITS))

extern volatile int rx_rdy, tx_rdy, rx_data, tx_data, txbuf_full;
//
// No `sm_config_set_in_pin_count()` is provided for RP2040
// So, here we have a stub function
//
#if defined(RP2040)
#define sm_config_set_in_pin_count(c, num)
#endif

                            // On 250MHz RP2350B clock.
float clk_divider = 30000;     // 31 ... about 4MHz
                            // 30000 for debugging
                            // 9 ... 13.89MHz seems to OK
                            // 8 ... 15.62MHz seems to OK
                            // 7 ... 17.85MHz no good.
                            // On 150MHz RP2350B clock
                            // 10 ... 7.5MHz OK
                            // 9  ... 8.333MHz OK
                            // 8  ... 9.375MHz NG
void emuz80_gpio_init()
{
    printf("IORQ:%d, RD:%d, WR:%d, RESET:%d, WAIT:%d\n", IORQ_Pin, RD_Pin, WR_Pin, RESET_Pin, WAIT_Pin);
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

    for (int i = 0; i < 8; ++i) {
        pio_gpio_init(pio0, D0_Pin + i);
#if defined(RP2040)
        pio_gpio_init(pio1, D0_Pin + i);
#endif
    }
    for (int i = 0; i < 16; ++i)
        pio_gpio_init(pio0, A0_Pin + i);
}

#if defined(RP2040)
void set_pindirs_program_init(PIO pio, uint sm, uint offset, uint data_pin, uint rd_pin) {
	//   in: RD_Pin(24), count: 1
	//   sideset: D0_Pin(16),D4_Pin(20), count: 4
    printf("pindirs: D0:%d, RD:%d\n", data_pin, rd_pin);
    pio_sm_set_consecutive_pindirs(pio, sm, rd_pin, 1, false);
    pio_sm_config c = set_pindirs_program_get_default_config(offset);
    sm_config_set_in_pins(&c, rd_pin);
    //sm_config_set_in_pin_count(&c, 1);    // RP2040 has no IN_PIN_COUNT
    sm_config_set_sideset_pins(&c, data_pin);
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio, sm, offset, &c);
}
#endif

void emuz80_pio_init() {
    //
    // PIO StateMachine(SM) initialzation
    //

    // PIO0: pin assign
	// RD,WR,MREQ,IORQ,WAIT
	pio_gpio_init(pio0, RD_Pin);
#if defined(RP2040)
	pio_gpio_init(pio1, RD_Pin);
#endif
	pio_gpio_init(pio0, WR_Pin);
    // PIO1: pin assign
	//pio_gpio_init(pio1, MREQ_Pin);
	pio_gpio_init(pio1, IORQ_Pin);
	pio_gpio_init(pio1, WAIT_Pin);

    uint offset1;

#if defined(RP2350B)
    // pio_set_gpio_base should be invoked before pio_add_program
    pio_set_gpio_base(pio1, 16);
#endif //defined(RP2350B)
    pio_sm_config c;
    // PIO0:SM0 ... ram_read
    //   IN: A0-A15(0), IN_COUNT: 16
    //   OUT/MOV: D0-Pin(24), OUT_COUNT: 8(D0-D7)
    //   JMP_PIN: RD_Pin(20)
    //   SET_BASE: 31 (debug pin)
    printf("---start---\n");
	offset1 = pio_add_program(pio0, &ram_read_addr_program);
    pio_sm_set_consecutive_pindirs(pio0, 0, A0_Pin, 16, false);
    c = ram_read_addr_program_get_default_config(offset1);
    sm_config_set_in_pins(&c, A0_Pin);
    sm_config_set_in_pin_count(&c, 16);
    sm_config_set_in_shift(&c, false, false, 32);    // 16bit autopush
    sm_config_set_out_pins(&c, D0_Pin, 8);
    sm_config_set_out_shift(&c, false, false, 32);    // 8bit autopull
    sm_config_set_jmp_pin(&c, IORQ_Pin);
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio0, 0, offset1, &c);

    // PIO0:SM1 ... ram_write_addr
    //   IN: A0-A15(0), IN_COUNT: 16, autopush
    //   OUT/MOV: D0-Pin(24), OUT_COUNT: 8
    //   JMP_PIN: WR_Pin(21)
	offset1 = pio_add_program(pio0, &ram_write_addr_program);
    pio_sm_set_consecutive_pindirs(pio0, 1, A0_Pin, 16, false);
    pio_sm_set_consecutive_pindirs(pio0, 1, WR_Pin, 1, false);
    pio_sm_set_consecutive_pindirs(pio0, 1, IORQ_Pin, 1, false);
    c = ram_write_addr_program_get_default_config(offset1);
    sm_config_set_in_pins(&c, A0_Pin);
    sm_config_set_in_pin_count(&c, 16);
    sm_config_set_in_shift(&c, false, false, 32); //16);    // 16bit autopush
    sm_config_set_out_pins(&c, D0_Pin, 8);
    sm_config_set_out_pin_count(&c, 8);
    sm_config_set_out_shift(&c, false, false, 8);    // 8bit no-autopush
    sm_config_set_jmp_pin(&c, IORQ_Pin);
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio0, 1, offset1, &c);

    // PIO0:SM2: ram_write_data
    //   IN:  D0_Pin(24), count: 8, autopush
    offset1 = pio_add_program(pio0, &ram_write_data_program);
    pio_sm_set_consecutive_pindirs(pio0, 2, D0_Pin, 8, false);  // data as input
    pio_sm_set_consecutive_pindirs(pio0, 2, WR_Pin, 1, false);
    pio_sm_set_consecutive_pindirs(pio0, 2, IORQ_Pin, 1, false);
    c = ram_write_data_program_get_default_config(offset1);
    sm_config_set_in_pins(&c, D0_Pin);
    sm_config_set_in_pin_count(&c, 8);
    sm_config_set_in_shift(&c, false, false, 8);   // no-autopush
    sm_config_set_jmp_pin(&c, IORQ_Pin);
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio0, 2, offset1, &c);

    // PIO0:SM3 ... data_out
    //   OUT/MOV: D0-Pin(24), OUT_COUNT: 8(D0-D7)
    //   SET_BASE: WAIT
	offset1 = pio_add_program(pio0, &data_out_program);
    printf("data_out: %d\n", offset1);
    pio_sm_set_consecutive_pindirs(pio0, 3, RD_Pin, 1, false);
    c = data_out_program_get_default_config(offset1);
    sm_config_set_out_pins(&c, D0_Pin, 8);
    sm_config_set_out_shift(&c, true, false, 32);    // 8bit autopull
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio0, 3, offset1, &c);

    #if defined(RP2040)
    // PIO1:SM0,1
	//   in: RD_Pin(32), count: 1
	//   sideset: D0_Pin(16),D4_Pin(20), count: 4
	offset1 = pio_add_program(pio1, &set_pindirs_program);
    printf("set_pindir: %d\n", offset1);
	set_pindirs_program_init(pio1, 0, offset1, D0_Pin, RD_Pin);
	set_pindirs_program_init(pio1, 1, offset1, D0_Pin + 4, RD_Pin);
#endif
 
    // PIO1:SM2 ... two/one phase clock generator(program clockgen)
	// 	 SET: BASE: 40(CLK_Pin, inverted), 41(INT_Pin, inverted)
    offset1 = pio_add_program(pio1, &clockgen_program);
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
    // single clock: (2 instruction loop) (On RP2350B 150MHz)
    //  50.0 ... 1.5MHz (660-670ns) 
    //  30.0 ... 2.5MHz (400ns) ... no wait, 
    //  18.7 ... 4.0-4.17MHz (250-260ns) .... 0/1 wait in M1, 1 wait in WR, 0/1 wait in RD
    //  10.0 ... 7.1-7.6MHz (130-140ns) ... seems to work
    //   5.0 ... 14-16MHz (60-70ns) ... does not works
    sm_config_set_clkdiv(&c, clk_divider); // 12.0 ... 6.25MHz max
    pio_sm_init(pio1, 2, offset1, &c);

    // PIO1: SM3 ... IO cycle WAIT handler
    //   SET: BASE: 19(WAIT_Pin)
    //   wait: 18(IORQ_Pin)
    offset1 = pio_add_program(pio1, &iorq_wait_program);
    //iorq_wait_program_init(pio1, 3, offset1, WAIT_Pin, D0_Pin);
	//   IN: IORQ_Pin(18), count 1
	//	 SET: WAIT_Pin(19), count: 1
    pio_sm_set_consecutive_pindirs(pio1, 3, WAIT_Pin, 1, true);
    c = iorq_wait_program_get_default_config(offset1);
    sm_config_set_in_pins(&c, D0_Pin);
    sm_config_set_in_pin_count(&c, 8);
    sm_config_set_in_shift(&c, false, false, 8);
    sm_config_set_out_pins(&c, D0_Pin, 8);
    sm_config_set_out_shift(&c, false, false, 8);
    sm_config_set_set_pins(&c, WAIT_Pin, 1);
    sm_config_set_set_pin_count(&c, 1);
    sm_config_set_clkdiv(&c, 1);         // 1 ... full speed 
    pio_sm_init(pio1, 3, offset1, &c);

    // input override
    // These should be below pio_gpio_init
#if !defined(USE_DMA) && defined(RP2040)
    for (int i = WAIT_Pin; i < 30 ; i++) {
        printf ("inover: %d\n", i);
        gpio_set_input_enabled(i, false);
        gpio_set_inover(i, GPIO_OVERRIDE_LOW);
        gpio_set_slew_rate(i, GPIO_SLEW_RATE_FAST);
    }
#endif
}

void emuz80_unreset(void) {
    gpio_put(RESET_Pin, true);
}

int ch_r_base, ch_r_addr, ch_r_data, ch_w_addr, ch_w_data, ch_w_base;
int ch_r_start, ch_w_start;
#if defined(RP2040)
int ch_r_mask, ch_w_mask;
static uint32_t *mask_pattern = (uint32_t *)0xffff0000;
#endif
volatile uint32_t addr_temp;
volatile uint32_t *dummy = (uint32_t *)0x12345678;
const uint32_t *wr_addr = (uint32_t *)&mem[0x5638];
static uint32_t *base_addr = (uint32_t *)&mem[0];

volatile uint32_t *r_read_addr;
volatile uint32_t *w_write_addr;

//
// DMA configure support function
//
void dma_channel_init(int ch, int dma_size, int chain_to, int dreq, volatile void *dest, const volatile void *src)
{
    dma_channel_config c = dma_channel_get_default_config(ch);
    channel_config_set_transfer_data_size(&c, dma_size);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    if (chain_to >= 0)
        channel_config_set_chain_to(&c, chain_to);
    if (dreq >= 0)
        channel_config_set_dreq(&c, dreq);
    dma_channel_configure(ch, &c, dest, src, 1, false);
}

void emuz80_dma_init()
{
#if defined(USE_DMA)
    ch_r_base = dma_claim_unused_channel(true);
    ch_r_addr = dma_claim_unused_channel(true);
    ch_r_data = dma_claim_unused_channel(true);
    ch_w_base = dma_claim_unused_channel(true);
    ch_w_addr = dma_claim_unused_channel(true);
    ch_w_data = dma_claim_unused_channel(true);
#if defined(RP2040)
    ch_r_mask = dma_claim_unused_channel(true);
    ch_w_mask = dma_claim_unused_channel(true);
#endif
    r_read_addr = &dma_hw->ch[ch_r_data].read_addr;
#if defined(RP2350)
    // Ch_R_Data: RAM -> PIO TX FIFO
    dma_channel_init(ch_r_data, DMA_SIZE_8, ch_r_base, -1, &pio0_hw->txf[3], base_addr);
    // Ch_R_Addr: PIO RX FIFO -> READ_ADDR Register in CH_R_data (16bit ring buffer)
    dma_channel_init(ch_r_addr, DMA_SIZE_32, ch_r_data, pio_get_dreq(pio0, 0, false), 
                        hw_set_alias(r_read_addr), &pio0_hw->rxf[0]);
    // Ch_R_Base: &mem[0] -> ch_r_addr->read_addr
    dma_channel_init(ch_r_base, DMA_SIZE_32, ch_r_addr, -1, 
                        r_read_addr, &base_addr);
    ch_r_start = ch_r_base;
#endif
#if defined(RP2040)
    // Ch_R_Data: RAM -> PIO TX FIFO
    dma_channel_init(ch_r_data, DMA_SIZE_8, ch_r_addr, -1, &pio0_hw->txf[3], NULL);
    // Ch_R_Base: &mem[0] -> ch_r_addr->read_addr
    dma_channel_init(ch_r_base, DMA_SIZE_32, ch_r_data, -1, 
                        hw_set_alias(r_read_addr), &base_addr);
    // Ch_R_Mask: &mask_pattern -> ch_r_addr->read_addr
    dma_channel_init(ch_r_mask, DMA_SIZE_32, ch_r_base, -1, 
                        hw_clear_alias(r_read_addr), &mask_pattern);
    // Ch_R_Addr: PIO RX FIFO -> READ_ADDR Register in CH_R_data (16bit ring buffer)
    dma_channel_init(ch_r_addr, DMA_SIZE_32, ch_r_mask, pio_get_dreq(pio0, 0, false), 
                        r_read_addr, &pio0_hw->rxf[0]);
    ch_r_start = ch_r_addr;
#endif
    // hw_narrow_zero_aliasesは効いていないようだ。readと同じく base->addr->dataの
    // 3段構えとする。
    w_write_addr = &dma_hw->ch[ch_w_data].al1_write_addr;
#if defined(RP2350)
    // Ch W_Data: PIO RX FIFO -> RAM
    dma_channel_init(ch_w_data, DMA_SIZE_8, ch_w_base, pio_get_dreq(pio0, 2, false), 
                        NULL, &pio0->rxf[2]);
    // Ch W_Addr: PIO RX FIFO -> WRITE_ADDR register in Ch W_Data (ring buffer)
    dma_channel_init(ch_w_addr, DMA_SIZE_32, ch_w_data, pio_get_dreq(pio0, 1, false), 
                        hw_set_alias(w_write_addr), &pio0->rxf[1]);
    // Ch_W_Base: &mem[0] -> ch_w_addr->read_addr
    dma_channel_init(ch_w_base, DMA_SIZE_32, ch_w_addr, -1, 
                        w_write_addr, &base_addr);
    ch_w_start = ch_w_base;
#endif 
#if defined(RP2040)
    // Ch W_Data: PIO RX FIFO -> RAM
    volatile uint32_t *w_write_addr = &dma_hw->ch[ch_w_data].al1_write_addr;
    dma_channel_init(ch_w_data, DMA_SIZE_8, ch_w_mask, pio_get_dreq(pio0, 2, false), 
                        NULL, &pio0->rxf[2]);
    // Ch_W_Base: &mem[0] -> ch_w_addr->read_addr
    dma_channel_init(ch_w_base, DMA_SIZE_32, ch_w_data, -1, 
                        hw_set_alias(w_write_addr), &base_addr);
    // Ch_R_Mask: &mask_pattern -> ch_r_addr->read_addr
    dma_channel_init(ch_w_mask, DMA_SIZE_32, ch_w_base, pio_get_dreq(pio0, 1, false), 
                        hw_clear_alias(w_write_addr), &mask_pattern);
    dma_channel_init(ch_w_addr, DMA_SIZE_32, ch_w_mask, -1, 
                        w_write_addr, &pio0->rxf[1]);
    // Ch W_Addr: PIO RX FIFO -> WRITE_ADDR register in Ch W_Data (ring buffer)
    ch_w_start = ch_w_addr;
#endif 
#endif //USE_DMA
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
    volatile register uint32_t a32, d32;
    int xcount = 10;


    multicore_fifo_push_blocking(FLAG_VALUE);
    uint32_t g = multicore_fifo_pop_blocking();
#if defined (USE_DMA)
    // safe DMA abort
    // 1. エラッタ RP2350-E5 対策: 先にチャンネルの有効化を解除する
    int n_dma = 6;
#if defined(RP2040)
    n_dma = 8;
#endif
    for (int i = 0; i < n_dma; ++i) {
        dma_hw->ch[i].al1_ctrl &= ~DMA_CH0_CTRL_TRIG_EN_BITS;
    }
    // （もし別チャンネルへのチェーン設定がある場合、その先のチャンネルも同様にクリアする）

    // 2. チャンネルのアボート（強制終了）を要求
    for (int i = 0; i < n_dma; ++i) {
        dma_channel_abort(i);
    }

    // 3. アボートが完全に完了するまでループで待機（RP2350の安全な作法）
    // アボート中は該当ビットが1になり、完了すると0に戻ります
    while (dma_channel_is_busy(0) ||
           dma_channel_is_busy(1) ||
           dma_channel_is_busy(2) || 
           dma_channel_is_busy(3) || 
           dma_channel_is_busy(4) || 
#if defined (RP2040)
           dma_channel_is_busy(6) || 
           dma_channel_is_busy(7) || 
#endif
           dma_channel_is_busy(5)) {
            tight_loop_contents();
    }
#endif //USE_DMA

    // Z80 Input pin initialize
    emuz80_gpio_init();
    emuz80_pio_init();
    emuz80_dma_init();

    for (int i = 0; i < 0x10; ++i) {
        printf("%02X ", mem[i]);
        if (i % 8 == 7) printf("\n");
    }

    // restart pio state machnes
    for (int i = 0; i < 4; ++i) {
        pio_sm_set_enabled(pio0, i, false);  // ram_read
        pio_sm_clear_fifos(pio0, i);
        pio_sm_restart(pio0, i);
    }
    for (int i = 0; i < 4; ++i) {
        pio_sm_set_enabled(pio1, i, false);  // ram_read
        pio_sm_clear_fifos(pio1, i);
        pio_sm_restart(pio1, i);
    }

    // start target CPU clock
    pio_sm_set_enabled(pio1, 2, true);  // clockgen
    // wait for some time, which seems to be needed
    // CPU clock: (1000 * 2 / 250) (ns) * clk_divider
    float period = ((1000 * 2 / 250) * clk_divider) * 10 / 1000;
    printf("period: %.0f\n", period);
    sleep_us((int)period);  // clk_divider / 250MHz
    // start target CPU
    emuz80_unreset();

#if defined(USE_DMA)
    dma_channel_start(ch_r_start);
    dma_channel_start(ch_w_start);
#endif
    // start pio state machines and dma's
    pio_sm_set_enabled(pio0, 0, true);  // ram_read
    pio_sm_set_enabled(pio0, 1, true);  // ram_write_addr
    pio_sm_set_enabled(pio0, 2, true);  // ram_write_data
    pio_sm_set_enabled(pio0, 3, true);  // data_out
#if defined (RP2040)
    pio_sm_set_enabled(pio1, 0, true);  // set_pindirs (D0-D3)
    pio_sm_set_enabled(pio1, 1, true);  // set_pindirs (D4-D7)
#endif
    pio_sm_set_enabled(pio1, 3, true);  // iorq_wait

loop:
#define DEBUG
#ifdef DEBUG
    while (((port = gpio_get_all()) & (1<<RD_Pin)) != 0)
        ;
    sleep_us(2);
    port = gpio_get_all();
    if (xcount > 0) {
        printf("%08X %08X %02X\n", port, *r_read_addr, mem[port & ADDR_MASK]);
        --xcount;
    }
    while (((port = gpio_get_all()) & (1<<RD_Pin)) == 0)
        ;
    goto loop;
#endif
#if 0
    // IO R/W cycle
    while (((port = gpio_get_all()) & (1<<IORQ_Pin)) != 0)
        ;
    if ((port & (1 << IORQ_Pin)) == 0) {
        if ((port & (1 << RD_Pin)) == 0) {
            // IO Read cycle
            // EMUZ80 UART emulation
            addr = port & 0xff;
            // UARTCR or DR
            if (addr == 1) {
                status = (rx_rdy | (tx_rdy & ~txbuf_full));
                // read status register
                pio_sm_put(pio0, 3, status);
            } else if (addr == 0) {
                // read data register
                data = rx_data;
                rx_rdy = 0;
                pio_sm_put(pio0, 3, data);
            }
        } else if ((port & (1 << WR_Pin)) == 0) {
            // IO Write cycle
            addr = port & 0xff;
            data = ((port >> D0_Pin) & 0xff);
            if (addr == 0) {
                // UART DR
                tx_data = data;
                txbuf_full = 2;
            }
        }
        pio_sm_put(pio1, 3, 0);       // notify IO process finished to the state machine
        pio_sm_get_blocking(pio1, 3); // wait for WAIT set High
    while (((port = gpio_get_all()) & (1<<IORQ_Pin)) == 0)
        ;
    }
    goto loop;
#endif
}
