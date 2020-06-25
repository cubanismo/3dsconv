#
# Makefile for 3dsconv
#
RM = rm -f
CFLAGS = -Wall -g

OBJS = 3dsconv.o 3dsfile.o lwfile.o internal.o n3dout.o jagout.o cry.o targa.o cout.o cfout.o

all: 3dsconv

3dsconv: $(OBJS)
	gcc $(CFLAGS) -o $@ $(OBJS) -lm

clean:
	$(RM) $(OBJS) 3dsconv
