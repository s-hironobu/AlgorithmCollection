/* ---------------------------------------------------------------------------
 * Skiplist
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.01
 * Copyright (C) 2009-2022  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _SKIPLIST_H_
#define _SKIPLIST_H_

#include "common.h"

typedef struct _skiplist_node_t {
  lkey_t key;                        /* key */
  val_t val;                         /* value  */

  int topLevel;                      /* level(hight) of this node */
  struct _skiplist_node_t *next[1];  /* pointer to the next node */
} skiplist_node_t;

typedef struct _skiplist_t {
  int	maxLevel;                    /* maximum level(hight) of this list */

  skiplist_node_t *tail;
  skiplist_node_t *head;

  pthread_mutex_t mtx;

  skiplist_node_t **preds;
  skiplist_node_t **succs;
} skiplist_t;

bool_t add(skiplist_t *, const lkey_t, const val_t);
bool_t delete(skiplist_t *, const lkey_t, val_t *);
val_t find(skiplist_t *, const lkey_t);
void show_list(skiplist_t *);
skiplist_t *init_list(const int, const lkey_t, const lkey_t);
void free_list(skiplist_t *);

#endif
