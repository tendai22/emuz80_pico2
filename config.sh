#! /bin/sh
case "$1" in
RP2350[Bb]|RP2350B_CoreBoard)
    echo $1
    cp CMakeLists_rp2350b.txt CMakeLists.txt
    cp z80_rp2350b.pio blink.pio
    ;;
RP2350[Aa]|RP2350_Zero)
    echo $1
    cp CMakeLists_rp2350_zero.txt CMakeLists.txt
    cp z80_rp2350a.pio blink.pio
    ;;
RP2040|AE_RP2040)
    echo $1
    cp CMakeLists_ae_rp2040.txt CMakeLists.txt
    cp z80_rp2040.pio blink.pio
    ;;
*)
    echo "Usage: sh config.sh RP2350B|RP2350A|RP2040"
    ;;
esac
