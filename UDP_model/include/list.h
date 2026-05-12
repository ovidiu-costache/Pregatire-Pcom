#ifndef _LIST_H_
#define _LIST_H_

// List entry
struct cel {
  void *info;
  int info_len;
  int seq;
  char type;
  struct cel *next;
};

typedef struct cel list_entry;

// Window as a list
typedef struct {
  int size;
  int max_seq;
  list_entry *head;
} list;

// Creates a list
list *create_list();
// Adds a segment to the window
void add_list_elem(list *window, void *segment, int segment_size, int seq);

#endif // _LIST_H_
