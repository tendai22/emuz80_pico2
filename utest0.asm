    org 0
    LD SP, 0x8000
loop1:
    IN A, (0x1)
    BIT 1, A
    JR Z, loop1
    LD A,'A'
    OUT (0x0), A
    JR loop1
    end
