/* ---------------------------------------------------------------------------
 * Lock-Free Skiplist (Intel 32-bit only, __X86_64__ Not SUPPORTED)
 *
 * "A Lock-Free concurrent skiplist with wait-free search" by Maurice Herlihy & Nir Shavit
 *  http://www.cs.brown.edu/courses/csci1760/ch14.ppt
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.01
 * Copyright (C) 2009  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _LOCKFREESKIPLIST_H_
#define _LOCKFREESKIPLIST_H_

#include "common.h"

typedef intptr_t node_stat;
#define MARKED  0
#define UNMARKED 1


typedef struct _tower_ref {
  node_stat mark;
  struct _skiplist_node_t *next_node_ptr;
}__attribute__((packed)) tower_ref;


typedef struct _skiplist_node_t {
  lkey_t key;         /* key */
  val_t val;          /* value */
  int topLevel;       /* level(hight) of this node */
  tower_ref *tower;
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
