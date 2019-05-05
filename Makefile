MCU=atmega328p

CC=avr-gcc
LD=avr-gcc
PRGNAME=rgb_dali
ARCHFLAGS= -mmcu=atmega328
CFLAGS=-pedantic -g -Wall
CPPFLAGS="-DF_CPU=16000000"

all: $(PRGNAME).hex

.c.o: 
	$(CC) $(CFLAGS) $(CPPFLAGS) $(ARCHFLAGS) -c -o $@ $^

$(PRGNAME).bin: $(PRGNAME).o
	$(LD) $(ARCHFLAGS) -o $@ $^

%.hex: %.bin
	avr-objcopy -R .eeprom -O ihex $^ $@

clean:
	-rm *.o
	-rm $(PRGNAME).bin
	-rm $(PRGNAME).hex

AVRDUDE = avrdude -v -c avrisp2 -p m328p
fuse:
	$(AVRDUDE) -U hfuse:w:0b11011001:m -U lfuse:w:0b01110111:m

load: $(PRGNAME).hex
	$(AVRDUDE) -U flash:w:$(PRGNAME).hex
