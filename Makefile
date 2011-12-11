CC			= gcc
CFLAGS		= -Iinclude

%.o: %.c
	gcc -c $(CFLAGS) -o $@ $<

SRC_OBJS	=	src/read.o

TEST_OBJS	=	test/udp_dump.o

obj: $(SRC_OBJS)

test/udp_dump_test: $(SRC_OBJS) test/udp_dump.c
	gcc $(CFLAGS) -o test/udp_dump_test $(SRC_OBJS) test/udp_dump.c

tests: test/udp_dump_test

clean:
	find . -name '*.o' -delete
	rm -f test/*_test

