;
;
;
    org 0
    LD SP, 0x8000
loop0:
    IN A, (0x1)
    BIT 0, A
    JR Z, loop0
    IN A, (0x0)
    CP A, 'a'
    JR C, label1
    CP A, 'z'+1
    JR NC, label1
    AND A, 0xDF
label1:
    PUSH AF
loop1:
    IN A, (0x1)
    BIT 1, A
    JR Z, loop1
    POP AF
    OUT (0x0), A
    JR loop0
    end
