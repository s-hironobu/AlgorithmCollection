/* ---------------------------------------------------------------------------
 * Open-Addressed Hash Table
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.17
 * Copyright (C) 2009-2024  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _OPEN_ADDRESS_HASH_H_
#define _OPEN_ADDRESS_HASH_H_

#include "common.h"

typedef enum {EMP = 0, DEL = 1, OCC = 2} node_stat;

typedef struct _node_t
{
  lkey_t key;           /* key */
  val_t value;          /* value */
  node_stat stat;       /* status */
} node_t;


typedef struct _hashtable_t
{
  unsigned long int setSize;        /* number of nodes */

  node_t *bucket;                   /* hashtable */
  unsigned int table_size;          /* hashtable size(length) */

  node_t *old_bucket;               /* temporary hashtable for keep the original hashtable refore resize */
  unsigned int old_table_size;      /* size of old_bucket */

  pthread_mutex_t mtx;              /* mutex lock */
} hashtable_t;


void show_hashtable (hashtable_t *);
hashtable_t * init_hashtable (const unsigned int);
void free_hashtable (hashtable_t *);
bool_t add (hashtable_t *, const lkey_t, const val_t);
bool_t delete (hashtable_t *, const lkey_t, val_t *);
bool_t find (hashtable_t *, const lkey_t);
#endif
