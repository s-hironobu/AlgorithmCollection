/* ---------------------------------------------------------------------------
 * Refinable Hash Table
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.17
 * Copyright (C) 2009-2014  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _REFINABLE_HASH_H_
#define _REFINABLE_HASH_H_

#include "common.h"

typedef struct _node_t
{
  lkey_t key;            /* key */
  val_t value;           /* value */

  struct _node_t *next;  /* pointer to the next node */
} node_t;

typedef struct _list_t
{
  node_t *head;
  pthread_mutex_t *mtx;
} list_t;

typedef struct _hashtable_t
{
  unsigned long int setSize;        /* number of nodes */

  list_t *bucket;                   /* hashtable */
  unsigned int table_size;          /* hashtable size(length) */

  list_t *old_bucket;               /* temporary hashtable for keep the original hashtable before resize */
  unsigned int old_table_size;      /* size of old_bucket */

} hashtable_t;


void show_list (const list_t *);
void show_hashtable (hashtable_t *);
hashtable_t * init_hashtable (const unsigned int);
void free_hashtable (hashtable_t *);
unsigned int  hashCode (lkey_t, const hashtable_t *);
bool_t add (hashtable_t *, const lkey_t, const val_t);
bool_t delete (hashtable_t *, const lkey_t, val_t *);
bool_t contains (hashtable_t *, const lkey_t);


#endif
