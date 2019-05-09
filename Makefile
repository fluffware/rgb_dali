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

AVRDUDE = avrdude 
AVRDUDE_CMD = $(AVRDUDE) -v -c avrisp2 -p m328p 
fuse:
	$(AVRDUDE_CMD) -u -U hfuse:w:0b11011001:m -U lfuse:w:0b11111111:m

load: $(PRGNAME).hex
	$(AVRDUDE_CMD) -U flash:w:$(PRGNAME).hex
