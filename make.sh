#! /bin/sh

# fuse settings compared to defaults:
#    external crystal oscillator, 3 ... 8 MHz, JTAG disabled, brownout at 2.7V.
export MICRO=../Micro
make -f $MICRO/Makefile LFUSE=0xDD HFUSE=0xD1 EFUSE=0xF5 NAME=nmeamixer MCU=atmega1280 SPI=200000 CFLAGS="-DF_CPU=3686400 -DGPS_IGNORE_FIX=1" "SRC=usart.c compass.c gps.c setup.c main.c"  LDFLAGS="-Wl,-u,vfprintf -lm" IFACE=avrdude $*

# do not use "-lprintf_flt"

