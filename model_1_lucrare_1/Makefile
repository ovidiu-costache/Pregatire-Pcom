CFLAGS= -Wall -Werror -Wno-error=unused-variable -I../common/ -MMD -MP

# Am schimbat 'send recv' cu 'client server'
all: client server

../common/utils.o:
	$(MAKE) -C ../common

link_emulator/lib.o:
	$(MAKE) -C link_emulator

# Construim executabilul 'client' folosind send.o
client: send.o link_emulator/lib.o common.o ../common/utils.o
	$(CC) $(CFLAGS) $^ -o client

# Construim executabilul 'server' folosind recv.o
server: recv.o link_emulator/lib.o common.o ../common/utils.o
	$(CC) $(CFLAGS) $^ -o server

common.o: common.c common.h

# Include auto-generated dependency files (silently ignored on first build)
-include $(wildcard *.d)

clean:
	$(MAKE) -C link_emulator clean
	-rm -f *.o *.d client server common.h.gch

.PHONY: all clean