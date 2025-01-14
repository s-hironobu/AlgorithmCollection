/* ---------------------------------------------------------------------------
 * Lock-Free list
 *
 * "Lock-Free Linked Lists and Skip Lists" Mikhail Fomitchev, Eric Ruppert
 * http://www.cse.yorku.ca/~ruppert/papers/lfll.pdf
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.16
 * Copyright (C) 2009-2025  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */

#ifndef _LOCKFREELIST_H_
#define _LOCKFREELIST_H_

#include "common.h"

#define MARKED        0x00000001
#define UNMARKED      0x00000000
#define MARKED_MASK   0x00000001

#define FLAGGED       0x00000002
#define UNFLAGGED     0x00000000
#define FLAGGED_MASK  0x00000002



typedef struct _next_ref {
  intptr_t mark;
  struct _node_t *node_ptr;
}__attribute__((packed)) next_ref;

typedef struct _node_t
{
  lkey_t key;                   /* key */
  val_t val;                    /* value */
  next_ref succ;                /* reference to the next node */
  struct _node_t *backlink;
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
