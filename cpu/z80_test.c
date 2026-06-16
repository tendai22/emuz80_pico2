//
// z80_test.c: test codes for Z80
//

#include <stdio.h>
#include <string.h>
#include "emuz80.h"

#define EMUBASIC_IO

void cpu_loader(void) {
#ifdef EMUBASIC_IO
    printf("loading: EMUBASIC_IO\n");
#include "emubasic_io.h"
    memcpy(&mem[0], &emuz80_binary[0], sizeof emuz80_binary);
#endif
#ifdef EMUBASIC
    printf("loading: EMUBASIC\n")
    memcpy(&mem[0], &emuz80_binary[0], sizeof emuz80_binary);
#endif

    //
    // debug Z80 codes
    //
    int a = 0;
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
    mem[0] = 0x76;
#endif
#if 0
    // jr loop
    mem[0] = 0x18;
    mem[1] = 0xfe;
#endif
#if 0
    // 00 -> FF, halt in 0x0076
    for (int i = 0; i <= 0xffff; i++)
        mem[i] = (i + 1)%256;
#endif
#if 0
    // JP 0000H
    mem[0] = 0xc3;
    mem[1] = 0x00;
    mem[2] = 0x00;
#endif
#if 0
    // INC (HL), JR 0xfc
    mem[0] = 0x21;  // LD HL, 7F00H
    mem[1] = 0x00;
    mem[2] = 0x10;
    mem[3] = 0x34;  // INC (HL)
    mem[4] = 0x18;  // JR
    mem[5] = 0xfd;  // -3
#endif
#if 0
    // long range INC (HL)
    for (int i = 4; i < 0x2000; i++) {
        mem[i] = 0;
        if (i % 55 == 0)
            mem[i] = 0x34;
    }
    mem[0] = 0x21;  // LD HL, 7F00H
    mem[1] = 0x00;
    mem[2] = 0x81;
    mem[3] = 0x34;  // INC (HL)
    mem[0x2000] = 0xC3;  // JR
    mem[0x2001] = 0x03;  // -3
    mem[0x2002] = 0x00;  // -3
#endif


#if 0
    // inc (hl) loop
    a = 0;
    mem[a++] = 0x21;
    mem[a++] = 0x38;
    mem[a++] = 0x56;
    mem[a++] = 0x34;
    mem[a++] = 0x34;
    mem[a++] = 0x34;
    mem[a++] = 0x34;
    mem[a++] = 0x18;
    mem[a++] = 0xfa;
    mem[0x5638] = 0x22;
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
#if 0
    // UART TX TEST (IO port version)
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
    static uint8_t uart_test[] = {
        0x31, 0x00, 0x80,   // LD SP, 0x8000
    //loop0:
        0xDB, 0x01,         // IN A, (0x1)
        0xCB, 0x47,         // BIT 0, A
        0x28, 0xFA,         // JR Z, loop0(0003H)
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
    // UART R/W test
    for (int i = 0; i < sizeof uart_test; ++i)
        mem[i] = uart_test[i];
#endif
#if 0
    //mem[0x0058] = 0x34;
    //mem[0x1c95] = 0x21;
    for (int i = 0x8000; i < 0x8100; ++i)
        mem[i] = ((255 - i) & 0xff);
    mem[0] = 0x21;  // LD HL, 7F00H
    mem[1] = 0x76;
    mem[2] = 0x80;
    mem[3] = 0x34;  // INC (HL)
    mem[4] = 0; //0x23;  // INC HL
    mem[5] = 0x18;  // JR -4
    mem[6] = 0xfc;
    mem[0x2000] = 0xC3;  // JR
    mem[0x2001] = 0x03;  // -3
    mem[0x2002] = 0x00;  // -3
#endif
#if 0
    // LD (0x1c95),a でおかしくなるのでそこだけ切り出して
    // 0番地においてみた→再現せず(期待通りに動く)
    int base = 0x1c93;
    for (int i = 0; i < 16; ++i) {
        if (i % 8 == 0)
            printf("%04X", i + base);
        printf (" %02X", mem[i + base]);
        if (i % 8 == 7)
            printf("\n");
    }
    static uint8_t test1[] = {
        0x3e, 0x00,         // LD A,00h
        0x32, 0x92, 0x80,   // LD (8092H),A
        0x3c,               // INC A
        0xc3, 0x02, 0x00    // JP 0002H
    };
    for (int i = 0; i < sizeof test1; ++i)
        mem[i] = test1[i];
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
#if 0
    mem[0x1c94] = 0x23;
    mem[0x1c95] = 0x34;
    mem[0x1c96] = 0x34;
    mem[0x1c97] = 0x34;
    mem[0x1c98] = 0x34;
    mem[0x1c99] = 0xf5;
    mem[0x1c9a] = 0x34;
#endif
}

