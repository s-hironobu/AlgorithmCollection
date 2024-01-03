/* ---------------------------------------------------------------------------
 * Concurrent Cuckoo Hash Table
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.17
 * Copyright (C) 2009-2024 suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _CONCURRENT_CUCKOO_HASH_H_
#define _CONCURRENT_CUCKOO_HASH_H_

#include "common.h"

#define CH_DEFAULT_MAX_SIZE 16


typedef struct _node_t {
  lkey_t key;            /* key */
  val_t value;           /* value  */
  struct _node_t *next;  /* pointer to the next node */
} node_t;


typedef struct _list_t {
  int size;              /* list size */
  node_t *head;          /* pointer to the head */
} list_t;


typedef struct _hashtable_t {
  unsigned long int setSize;        /* number of nodes */
  
  int probe_size;
  int threshold;
  
  list_t **table[2];                /* hashtable */
  unsigned int table_size;          /* hashtable size(length) */
  
  list_t **old_table[2];            /* temporary hashtable for keep the original hashtable before resize */
  unsigned int old_table_size;      /* size of old_table[0] */
  
  pthread_mutex_t *mtx[2];          /* mutex lock array */
  int mtx_size;                     /* length of mtx[2] */
} hashtable_t;


hashtable_t *init_hashtable(const unsigned int, const int, const int);
void free_hashtable(hashtable_t *, const unsigned int);
void show_hashtable(hashtable_t *);

bool_t add(hashtable_t *, const lkey_t, const val_t);
bool_t delete(hashtable_t *, const lkey_t, val_t *);
bool_t contains(hashtable_t *, const lkey_t);

#endif
