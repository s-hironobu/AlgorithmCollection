/* ---------------------------------------------------------------------------
 * Cuckoo Hash Table
 * 
 * "R.Pagh, F.F.Rodler, Cuchoo Hashing" http://cs.nyu.edu/courses/fall05/G22.3520-001/cuckoo-jour.pdf
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.17
 * Copyright (C) 2009-2024  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _CUCKOO_HASH_H_
#define _CUCKOO_HASH_H_

#include "common.h"

#define CH_DEFAULT_MAX_SIZE 10

typedef enum { EMP = 0, DEL = 1, OCC = 2 } node_stat;

typedef struct _node_t
{
  lkey_t key;           /* key */
  val_t value;          /* value */
  node_stat stat;       /* state */
} node_t;

typedef struct _hashtable_t {
    unsigned long int setSize;        /* number of nodes */

  node_t *table[2];                   /* hashtable */
  unsigned int table_size;            /* hashtable size(length) */

  node_t *old_table[2];               /* temporary hashtable for keep the orijinal hashtable before resize */
  unsigned int old_table_size;        /* size of old_table[0] */

  pthread_mutex_t mtx;                /* mutex lock */
} hashtable_t;


void show_hashtable(hashtable_t *);
hashtable_t *init_hashtable(const unsigned int);
void free_hashtable(hashtable_t *);
bool_t add(hashtable_t *, const lkey_t, const val_t);
bool_t delete(hashtable_t *, const lkey_t, val_t *);
bool_t find(hashtable_t *, const lkey_t);

#endif
