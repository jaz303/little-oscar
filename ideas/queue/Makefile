CC		= gcc
CFLAGS	=
LDFLAGS	= -lpthread

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

OBJ		=	queue.o \
			main.o

default: test

obj: $(OBJ)

test: obj
	$(CC) $(LDFLAGS) -o test $(OBJ)

clean:
	rm -f test
	rm -f *.o
