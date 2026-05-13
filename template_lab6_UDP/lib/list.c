#include "list.h"
#include <stdlib.h>
#include <string.h>

list *create_list() {
  list *l = (list *)calloc(1, sizeof(list));

  l->size = 0;
  return l;
}

// append order by seq
void add_list_elem(list *list, void *info, int info_len, int seq) {

  /* first check for duplicates seq */
  list_entry *l_check = list->head;

  while (l_check != NULL) {
    if (l_check->seq == seq)
      return;
    l_check = l_check->next;
  }

  // create list entry and set seq and type
  list_entry *l = (list_entry *)calloc(1, sizeof(list_entry));
  l->seq = seq;

  // buffer info
  if (info_len > 0) {
    l->info = calloc(info_len, sizeof(char));
    memcpy(l->info, info, info_len);
    l->info_len = info_len;
  } else {
    l->info = NULL;
  }

  // first elem
  if (list->head == NULL) {
    list->head = l;
  } else {
    list_entry *_l = list->head;

    // first elem
    if (_l->seq > seq) {
      l->next = _l;
      list->head = l;
      list->size++;
      return;
    }

    // find elem place
    while (_l->next && _l->next->seq < seq) {
      _l = _l->next;
    }

    l->next = _l->next;
    _l->next = l;
  }

  list->size++;
}
