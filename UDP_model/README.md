We have **two separate programs**, a client and a server. The client will
send a file to a server.

``client.c`` - the client implementation

``server.c`` - the server implementation

``list.h`` - the list API

``common.h`` - Some common data, such as the used port definition

Topology:
```
        L1          L2
client <--> router <--> server

Link 1 - 10 Mbps, 5ms delay, 10% packet loss (parameters set at topo.py:57)
Link 2 - 10 Mbps, 5ms delay, 10% packet loss (parameters set at topo.py:60)


client IP (h1) - 192.168.1.100
server IP (h2) - 172.160.0.100
```
## API
We will use the `sockers API`. We will use a list to represent the window.

```C
/* List entry */
struct cel{
  void* info;
  int info_len;
  int seq;
  char type;
  struct cel* next;
};

typedef struct cel list_entry;

/* Window as a list */
typedef struct {
  int size;
  int max_seq;
  list_entry* head;
} list;
```


```C
list *window = create_list();
seq_udp p;
...
add_list_elem(list* window, &p, sizeof(struct seq_udp), p.seq);
```

Our protocol implementation will have the following datagram structure over UDP:

```C
struct seq_udp {    
  uint32_t len;
  uint16_t seq;
  char payload[MAXSIZE];    
};
```

## Usage
To compile the code
```bash
make
# to create a random file of 100KB called file.bin
make random_file
```

This will create two binaries, to run them:
```bash
sudo python3 topo.py
```
It will open several terminals. We will run the server from `h2` and
the client from `h1`.

## C++
Note, if you want to use C++, simply change the extension of `client.c` and
`server.c` to `.cpp` and update the Makefile to use `g++`.
