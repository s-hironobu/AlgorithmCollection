/* ---------------------------------------------------------------------------
 * Lazy Synchronization Singly-linked List: pthread_mutex version
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Oct.25
 * Copyright (C) 2009-2023  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _LAZYSYNCROLIST_H_
#define _LAZYSYNCROLIST_H_

#include "common.h"

typedef struct _node_t
{
  lkey_t key;           /* key */
  val_t val;            /* value */
  bool_t marked;        /* Logical delete flag */
  pthread_mutex_t mtx;  /* mutex lock */
  struct _node_t *next; /* pointer to the next node */
} node_t;

typedef struct _list_t
{
  node_t *head;
  node_t *tail;
} list_t;


bool_t add (list_t *, const lkey_t, const val_t);
bool_t delete (list_t *, const lkey_t, val_t *);
bool_t find (const list_t *, const lkey_t);
list_t * init_list (void);
void free_list (list_t *);
void show_list(const list_t *);

#endif
