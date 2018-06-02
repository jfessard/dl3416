CROSS	?= avr-
PROJ	= dlclock
OBJS	= main.o rtc.o twi.o twi-lowlevel.o
F_CPU ?= 8000000

CFLAGS += -g -O$(OPT) \
-fshort-enums \
-mmcu=atmega328p -O2 -ffunction-sections -fdata-sections -Wall -fdiagnostics-color=always -std=gnu99 -funsigned-bitfields -funsigned-char

ifneq ($(F_CPU),)
	  CFLAGS += -DF_CPU=$(F_CPU)
endif

all: $(PROJ).hex

clean:
	rm -f $(PROJ).hex $(PROJ).elf $(OBJS)

flash:	$(PROJ).hex
	sudo avrdude -c usbtiny -p atmega328p -U flash:w:$(PROJ).hex:i -B1

$(PROJ).hex: $(PROJ).elf
	$(CROSS)objcopy $(PROJ).elf $(PROJ).hex -O ihex -j.text -j.rodata -j.data

$(PROJ).elf: $(OBJS)
	$(CROSS)gcc -o $(PROJ).elf $(OBJS) -mmcu=atmega328p -lm -lc -Wl,--gc-sections

%.o: %.c Makefile
	$(CROSS)gcc -o $@ -c $< $(CFLAGS)
