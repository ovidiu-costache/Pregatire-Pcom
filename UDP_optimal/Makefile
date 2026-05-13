build: server client
CFLAGS = -Wall -Wextra -g -Wno-error=unused-variable
CINCLUDES = -I include/
all: server client

list.o: lib/list.c include/list.h

server: server.c include/utils.h lib/list.o
	gcc  $(CFLAGS) $(CINCLUDES) server.c list.o -o server

client: client.c include/utils.h lib/list.o
	gcc  $(CFLAGS) $(CINCLUDES) client.c list.o -o client

.c.o:
	gcc $(CINCLUDES) $(CFLAGS) -g -c $?


.PHONY: clean run_client run_server random_file

# Construieste un fisier de 100KB (1024B*10^2)==1KB
random_file:
	dd if=/dev/urandom of=file.bin bs=1024 count=100

run_benchmark: server client
	./server &
	./client

clean:
	rm -f server client file.bin received_file.bin *.o include/*.h.gch
