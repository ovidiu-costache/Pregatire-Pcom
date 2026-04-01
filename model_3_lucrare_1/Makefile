CFLAGS= -Wall -Werror -Wno-error=unused-variable -I../common/ -MMD -MP

all: send recv

../common/utils.o:
		$(MAKE) -C ../common

link_emulator/lib.o:
	$(MAKE) -C link_emulator

send: send.o link_emulator/lib.o common.o ../common/utils.o

recv: recv.o link_emulator/lib.o common.o ../common/utils.o

common.o: common.c common.h

# Include auto-generated dependency files (silently ignored on first build)
-include $(wildcard *.d)

clean:
	$(MAKE) -C link_emulator clean
	-rm -f *.o *.d send recv common.h.gch

.PHONY: all clean
