/* ---------------------------------------------------------------------------
 * 
 * Lock-Free List : "A Pragmatic Implementation of Non-Blocking Linked-Lists" Timothy L. Harris 
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.02
 * Copyright (C) 2009-2014  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _NONBLOCKINGLIST_H_
#define _NONBLOCKINGLIST_H_

#include "common.h"


typedef intptr_t node_stat;
#define MARKED  0
#define UNMARKED 1

typedef struct _next_ref {
  node_stat mark;
  struct _node_t *node_ptr;
}__attribute__((packed)) next_ref;


typedef struct _node_t
{
  lkey_t key;                   /* key */
  val_t val;                    /* value */
  next_ref next;                /* reference to the next node */
}__attribute__((packed)) node_t;

typedef struct _list_t
{
  node_t *head;
  node_t *tail;
} list_t;

bool_t add (list_t *, const lkey_t, const val_t);
bool_t delete (list_t *, const lkey_t, val_t *);
bool_t find (list_t *, const lkey_t);
list_t * init_list (void);
void free_list (list_t *);

void show_list(const list_t *);

#endif
