PWD = $(notdir $(shell pwd | sed 's/b//'))

TARGET = pcsx1

CC = gcc

CFLAGS = -pedantic -Wall -Wextra -g -MMD
LDFLAGS = -lSDL2

O = linux
SRC = src

OBJS = $(O)/cdriso.o $(O)/cdrom.o $(O)/decode_xa.o $(O)/gpu.o $(O)/gpu_prim.o $(O)/gpu_soft.o $(O)/gte.o $(O)/mdec.o $(O)/misc.o $(O)/psxbios.o $(O)/psxcounters.o $(O)/psxdma.o $(O)/psxhle.o $(O)/psxhw.o $(O)/psxinterpreter.o $(O)/psxmem.o $(O)/r3000a.o $(O)/sio.o $(O)/spu.o $(O)/spu_adsr.o $(O)/spu_externals.o $(O)/spu_registers.o $(O)/spu_reverb.o $(O)/spu_xa.o $(O)/pad.o

OBJS += $(O)/main.o $(O)/system.o

all:	dir $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(O)/main.o:
	$(CC) $(CFLAGS) -DBUILD=\"$(PWD)\" -c $(SRC)/main.c -o $(O)/main.o

$(O)/%.o:	$(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(O) $(TARGET)

install: all
	cp $(TARGET) ~/.local/bin/

uninstall:
	rm ~/.local/bin/$(TARGET)

dir:
	@mkdir -p $(O)

-include $(O)/*.d
