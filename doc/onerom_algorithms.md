# Introduction

This file contains a completely autonomous PIO and DMA based ROM serving
implementation.  Once started, the PIO state machines and DMA channels
serve ROM data in response to external chip select and address lines
without any further CPU intervention.

# Algorithm Summary

The implementation uses three PIO state machines and 2 DMA channels, with
the following overall operation:
- PIO SM0 - Chip Select/Output Data Handler
- PIO SM1 - Address Reader
- DMA0    - Address Forwarder
- DMA1    - Data Byte Fetcher
- PIO SM2 - Data Byte Writer 

```
    CS active   Data to Outputs                 CS Inactive  Data to Inputs
            |   |                                         |  |
            v   v                                         v  v
SM0 ----------+-------------------------------------------------->
    ^         |                                                  |
    |         | (Optional IRQ0)                                  |
    |         v                                                  |
    |        SM1 ------> DMA0 --------> DMA1 -------> SM2        |
    |         |            |             |             |         |
    |         v            v             v             v         |
    |     Read Addr  Forward Addr  Get Data Byte  Write Data     |
    |  (Optional Loop)                                           |
    |                                                            v
    <-------------------------------------------------------------
                                                  (Not to scale)
```

# Timings

It is difficult to be sure, but based on observed data, and theoretical
estimates, the timings are estimated as follows:
- Address valid to correct data byte is 11-14 cycles
- Previous data valid after address change delay 14-11 cycles (although
  it is much less than this is CS is made inactive, which is very likely)
- CS active to data output is 5-6 cycles
- CS inactive to data inputs is 3 cycles

Physical settling time of lines will add to this.  Also, experience has
shown that the system is likely to introduce other, unplanned for, stalls
and other delays.  In particular if running _anything_ else, such as having
an SWD debug probe connected, may introduce delays and jitter due to bus
contention.

At a max rated RP2350 clock speed of 150MHz this is:
- 73-93ns from address to data
- 33-40ns from CS active to data output
- 20ns from CS inactive to data inputs

At 50MHz:
- 220-280ns from address to data
- 280-220ns from previous data valid after address change
- 100-120ns from CS active to data output
- 60ns from CS inactive to data inputs

Overclocked to 300MHz:
- 37-47ns from address to data
- 17-20ns from CS active to data output
- 10ns from CS inactive to data inputs

Address to data breakdown:
- 2 cycle delay in GPIO state reaching PIO due to input-sync
- SM1 address read 3-4 cycles:
  - 3 is best case scenario
  - 6 is worst case, but this "swallows" the input-sync delay, leading to 4
- Triggering DMA via DREQ from SM1 RX FIFO 1 cycle
- DMAs take 2-3 cycles each:
  - 3 cycles is likely due to single cycle stall due to contention, likely
    with other DMA channel.
  - Assume no stall in transfer between them.
- SM2 data byte output 1 cycle

Previous data valid after address change breakdown:
- Inverse of address to data breakdown

CS active to data output breakdown:
- 2 cycle delay in GPIO state reaching PIO due to input-sync
- SM0 best case is 3 cycles - mov x, pins; jmp x--, N; mov pindirs, ~null
- SM0 worst case adds 3 cycles, 2 of which "swallow" the input-sync delay

CS active to inactive breakdown:
- 2 cycle delay in GPIO state reaching PIO due to input-sync
- SM0 best case is 3 cycles - mov x, pins; jmp !x, N; mov pindirs, null
- SM0 worst case add 2 cycles, but these "swallow" the input-sync delay

These timings do not quite add up.  The C64 character ROM is a 2332A, with
350ns access time - the maximum time allowed to go from address valid to
valid.  As we can serve this ROM successfully at around 50MHz - with our
worse cast estimate of 280ns for this time - either our estimates are wrong,
or the C64 VIC-II requires better of the ROM than its specification - or
both.  Worst case it seems like our estimates may be 20% under (i.e add 25%
to them).

Therefore 50ns operation may require the RP2350 to be clocked closer to
400MHz than 300Mhz.  This is still likely to be within the RP2350's
capabilities.

# Detailed Operation

The detailed operation is as follows:

PIO0 SM0 - CS Handler
 - (Initially ensures data pins are inputs.)
 - Monitors the chip select lines.
 - When all CS lines are active, optionally triggers an IRQ to signal the
   address read SM to read the address lines.
 - Sets the data pins to outputs after an optional delay.  The data lines
   will not be serving the correct byte yet.
 - Tight loops, checking for CS going inactive.
 - When CS goes inactive again, sets data pins back to inputs and starts
   over.

PIO0 SM1 - Address Read
 - (One time - reads high 16 bits of ROM table address from its TX FIFO.
   This is preloaded to the TX FIFO by the CPU before starting the PIOs.)
 - Prepares by pushing high 16 bits of ROM table address into its OSR.
 - Optionally waits for IRQ from CS Handler SM.
 - After optional delay (used in non-IRQ case), reads the address lines (16
   bits) into OSR, completing the ROM table lookup address for the byte to
   be served.
 - Pushes the complete 32 bit ROM table lookup address into its RX FIFO 
   (triggering DMA Channel 0).
 - Loops back to 2nd step (pushing high 16 bits of ROM table address into
   OSR).

DMA Channel 0 - Address Forwarder
 - Triggered by PIO0 SM1 RX FIFO using DREQ_PIO0_RX1 (SM1 RX FIFO).
 - Reads the 32 bit ROM table lookup address from PIO0 SM1 RX FIFO.
 - Writes the address into DMA Channel 1 READ_ADDR or READ_ADDR_TRIG
   register.

DMA Channel 1 - Data Byte Fetcher
 - Triggered either DMA Channel 0 writing to this channels READ_ADDR_TRIG
   or using DREQ_PIO0_RX1 (SM1 RX FIFO) - in which case this DMA is paced
   identically to DMA Channel 0.
 - Reads the ROM byte from the address specified in its READ_ADDR register.
 - Writes the byte into PIO0 SM2 TX FIFO.
 - Waits to be re-triggered by DMA Channel 0 writing to READ_ADDR_TRIG or
   DREQ_PIO_RX1 (SM1 RX FIFO).

PIO0 SM2 - Data Byte Output
 - Waits for a data byte to become available in its TX FIFO.
 - When data byte available, outputs the data byte on the data pins.
 - Loops back to waiting for next data byte.

There are a number of hardware pre-requisites for this to work correctly:
- RP2350, not the RP2040.  This implementation uses:
  - pinsdirs as a mov destination
  - mov using pins as a source, only moving the configured "IN" pins.
  Neither of these are supported by the RP2040's PIOs.
- All Chip Select (or CE/OE) lines must be connected to contiguous GPIOs.
- Any active high chip select lines must be inverted prior to use, by
  using GPIO input inversion (INOVER).
- All Data lines must be connected to contiguous GPIOs.
- All Address lines must be connected to contiguous GPIOs, and be limited
  to a 64KB address space.  (Strictly other powers of two could be
  supported.)

In order to minimise jitter, it is advisable to ensure the following:
- The DMA channels have high AHB5 bus priority for both reads
  and writes using the BUS_PRIORITY register.
- Nothing else attempts to read or write to the 4 banks of SRAM the
  64KB ROM table is striped across.
- If other DMAs are enabled, the DMAs within this module should have a
  higher priority set.
- Nothing else accesses peripherals on the AHB5 splitter during operation.

Possible enhancements:
- May want to check CS is still active before setting data pins to outputs
  in SM2.

Note that a combined PIO/CPU implementation has also been explored (see
PIO_CONFIG_NO_DMA).  This is discussed further below, but in summary, it
matches DMA performance, while consuming a CPU core.

# Supported PIO configuration options

Note where min/max clock speeds are given below they tended to vary by
1-2Mhz, based on the day.  Likely due to temperature variations affecting
the host's timing.  It is unlikely the RP2350's timing varies, given it
has a modern, extremely accurate, clock source.

For these tests, the RP2350 was not overclocked - the max supported clock
speed it known to be higher than 150MHz for these ROMs, but there is a max
speed, particularly for character ROMs, due to the video chip requiring a
byte to be held after CS is dectivated.

# PIO_CONFIG_DEFAULT

- READ_IRQ = 1
- ADDR_READ_DELAY = 0

Here the IRQ from CS handler SM is used to trigger the address read SM.
This works well serving a C64 charaxcter ROM at higher clock speeds
(roughly 115-150MHz).

Min/Max speeds:
- PAL C64 Char ROM: 115-150MHz
- PAL C64 Kernal ROM: 45-150MHz
- PAL VIC-20 Char ROM: 44-150MHz

# PIO_CONFIG_SLOW_CLOCK_KERNAL

- READ_IRQ = 0
- ADDR_READ_DELAY = 1

Here 1 cycles is sufficient time to allow DMA chain to avoid backing up.
However, the VIC-II requires a 2 cycle delay from the character ROM - see
PIO_CONFIG_SLOW_CLOCK_CHAR.

Min/Max speeds:
- PAL C64 Kernal ROM: 41-150MHz
- PAL VIC-20 Kernal ROM: 22-150MHz

# PIO_CONFIG_SLOW_CLOCK_CHAR

- READ_IRQ = 0
- ADDR_READ_DELAY = 2

Add an additional cycle of delay before reading address lines to allow the
byte to remain on the bus slightly later, as seems to be required by a
VIC-II chip of a character ROM

Min/Max speeds:
- PAL C64 Char ROM: 51-150MHz
- PAL VIC-20 Char ROM: 51-150MHz

Whether to use DMA (or instead, use the CPU to read bytes).  If set,
ADDR_READ_IRQ is ignored.

This option is not maintained any may be broken.  It was implemented to test
which was faster - DMA or CPU.  It turns out to be identical performance -
both serve a C64 character from down to 51MHz but no further without
glitches.  Similarly, both serve a kernal down to 41MHz.

Therefore the DMA approach has been selected as superior as it frees up the
CPU for other applications.

(Actually it is possible to implement an even more pathological assembly
CPU loop which shaves the char ROM down to 50MHz, but it's likely fragile,
breaking if the CPU loop ever takes an extra cycle, such as when a debug
probe is connected.)

