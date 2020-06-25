#
# Makefile for 3dsconv
# TOS version
#
RM = rm -f
CC = gcc
CFLAGS = -m68020 -m68881 -O -Wall -g
TARGET = 3dsconv.ttp

OBJS = 3dsconv.o 3dsfile.o lwfile.o internal.o n3dout.o jagout.o cry.o targa.o cout.o cfout.o

all: 3dsconv.ttp 3dsconv.sym

3dsconv.ttp: $(OBJS)
	gcc $(CFLAGS) -o $@ $(OBJS)

3dsconv.sym: $(OBJS)
	gcc $(CFLAGS) -Bc:\c\gnu\bin\sym- -o $@ $(OBJS)

clean:
	$(RM) $(OBJS)
