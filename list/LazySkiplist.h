/* ---------------------------------------------------------------------------
 * Lazy Skiplist
 * "A Simple Optimistic skip-list Algorithm" Maurice Herlihy, Yossi Lev, Victor Luchangco, Nir Shavit
 * http://www.cs.brown.edu/~levyossi/Pubs/LazySkipList.pdf
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.16
 * Copyright (C) 2009-2014  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _LAZYSKIPLIST_H_
#define _LAZYSKIPLIST_H_

#include "common.h"

typedef struct _skiplist_node_t {
  lkey_t key;                        /* key */
  val_t val;                         /* value  */

  int topLevel;                      /* level(hight) of this node */

  bool_t marked;		     /* Logical delete flag */
  bool_t fullyLinked;

  pthread_mutex_t mtx;
  struct _skiplist_node_t *next[1];  /* pointer to the next node */
} skiplist_node_t;

typedef struct _skiplist_t {
  int	maxLevel;                    /* maximum level(hight) of this list */
  skiplist_node_t *head;
  skiplist_node_t *tail;

  pthread_key_t workspace_key; 
} skiplist_t;

typedef struct _workspace_t { 
  skiplist_node_t **preds;
  skiplist_node_t **succs;
} workspace_t;


bool_t add(skiplist_t *, const lkey_t, const val_t);
bool_t delete(skiplist_t *, const lkey_t, val_t *);
val_t find(skiplist_t *, const lkey_t);
void show_list(skiplist_t *);
skiplist_t *init_list(const int, const lkey_t, const lkey_t);
void free_list(skiplist_t *);

#endif
