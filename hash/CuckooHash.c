/* ---------------------------------------------------------------------------
 * Cuckoo Hash Table
 * 
 * "R.Pagh, F.F.Rodler, Cuchoo Hashing" http://cs.nyu.edu/courses/fall05/G22.3520-001/cuckoo-jour.pdf
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.17
 * Copyright (C) 2009-2022  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <assert.h>

#include "CuckooHash.h"

static bool_t init_tables(hashtable_t *, const unsigned int);
static void free_tables(node_t **);
static void resize(hashtable_t *);
static void del_op(hashtable_t *, node_t *);
static unsigned int hashCode0(lkey_t, const hashtable_t *);
static unsigned int hashCode1(lkey_t, const hashtable_t *);
static bool_t find_op(hashtable_t *, const lkey_t);
static void set_node(node_t *, const lkey_t, const val_t,
		     const node_stat);
static bool_t swap_node(hashtable_t *, const int, node_t, node_t *);
static node_t *get_node(hashtable_t *, const int, const lkey_t);


#define lock(mtx)      pthread_mutex_lock(&(mtx))
#define unlock(mtx)    pthread_mutex_unlock(&(mtx))

static void
set_node(node_t * node, const lkey_t key, const val_t val,
	 const node_stat stat)
{
    node->key = key;
    node->value = val;
    node->stat = stat;
}

static bool_t
swap_node(hashtable_t * ht, const int no, node_t node, node_t * next)
{
    node_t *tmp;
    bool_t ret = false;

    tmp = get_node(ht, no, node.key);
    set_node(next, tmp->key, tmp->value, tmp->stat);
    set_node(tmp, node.key, node.value, OCC);
    if (next->stat != OCC)
	ret = true;

    return ret;
}

static node_t *get_node(hashtable_t * ht, const int no, const lkey_t key)
{
    unsigned int myBucket;
    node_t *node;
    if (no == 0) {
	myBucket = hashCode0(key, ht);
	node = &ht->table[0][myBucket];
    } else {
	myBucket = hashCode1(key, ht);
	node = &ht->table[1][myBucket];
    }
    return node;
}


/*
 * bool_t add(hashtable_t * ht, const lkey_t key, const val_t val)
 *
 * Add node '(key,val)' to hashtable ht.
 *
 * success : return true
 * failure : return false
 */
bool_t add(hashtable_t * ht, const lkey_t key, const val_t val)
{
    unsigned int i;
    node_t node;
    bool_t ret = false;
    node_t tmp;
    int try = 10;

    lock(ht->mtx);

    if (find_op(ht, key) == true) {
	unlock(ht->mtx);
	return false;
    }

    set_node(&node, key, val, OCC);

  retry:
    for (i = 0; i < ht->table_size; i++) {
	if ((ret = swap_node(ht, 1, node, &tmp)) == true) {
	    ht->setSize++;
	    break;
	} else if ((ret = swap_node(ht, 2, tmp, &node)) == true) {
	    ht->setSize++;
	    break;
	}
    }

    if (ret == false && try-- > 0) {
	resize(ht);
	printf ("Resized\n");
	goto retry;
    }

    unlock(ht->mtx);

    return ret;
}


static void del_op(hashtable_t * ht, node_t * node)
{
    set_node(node, (lkey_t) NULL, (val_t) NULL, DEL);
    ht->setSize--;
}


/*
 * bool_t delete(hashtable_t * ht, const lkey_t key, val_t * getval)
 *
 * Delete node '(key, val)' by the key from hashtable ht, and write the val to *getval.
 *
 * success : return true
 * failure(key not found): return false
 */
bool_t delete(hashtable_t * ht, const lkey_t key, val_t * getval)
{
    node_t *node;
    bool_t ret = false;
    int i;

    lock(ht->mtx);

    for (i = 0; i <= 1; i++) {
	node = get_node(ht, i, key);
	if (node->key == key && node->stat == OCC) {
	    *getval = node->value;
	    del_op(ht, node);
	    ret = true;
	    break;
	}
    }

    unlock(ht->mtx);

    return ret;
}


static bool_t find_op(hashtable_t * ht, const lkey_t key)
{
    node_t *node;
    bool_t ret = false;
    int i;

    for (i = 0; i <= 1; i++) {
	node = get_node(ht, i, key);
	if (node->stat == OCC && node->key == key) {
	    ret = true;
	    break;
	}
    }
    return ret;
}


/*
 * bool_t find(hashtable_t * ht, const lkey_t key)
 *
 * Find node '(key, val)' by the key from hashtable ht, and write the val to *getval.
 * 
 * success : return true
 * failure(not found): return false
 */
bool_t find(hashtable_t * ht, const lkey_t key)
{
    bool_t ret;

    lock(ht->mtx);
    ret = find_op(ht, key);
    unlock(ht->mtx);

    return ret;
}


/*
 * bool_t init_tables(hashtable_t * ht, const unsigned int table_size)
 *
 * Initialize hashtable ht (table[0] and table[1])
 *
 * success : return true
 * failure : return false
 */
static bool_t init_tables(hashtable_t * ht, const unsigned int table_size)
{
    unsigned int i;

    if ((ht->table[0] =
	 (node_t *) calloc(table_size, sizeof(node_t))) == NULL) {
      elog("calloc error");
      return false;
    }
    if ((ht->table[1] =
	 (node_t *) calloc(table_size, sizeof(node_t))) == NULL) {
      elog("calloc error");
	free(ht->table[0]);
	return false;
    }

    for (i = 0; i < table_size; i++) {
	set_node(&ht->table[0][i], (lkey_t) NULL, (val_t) NULL, EMP);
	set_node(&ht->table[1][i], (lkey_t) NULL, (val_t) NULL, EMP);
    }

    ht->setSize = 0;

    return true;
}


static void free_tables(node_t ** table)
{
    free(table[0]);
    free(table[1]);
}


/*
 * hashtable_t *init_hashtable(const unsigned int size)
 *
 * Create new hashtable of size (2^size).
 *
 * success : return pointer of new hashtable
 * failure : return NULL
 */
hashtable_t *init_hashtable(const unsigned int size)
{
    hashtable_t *ht;
    unsigned int s;
    unsigned int table_size;

    if (CH_DEFAULT_MAX_SIZE <= size)
	s = CH_DEFAULT_MAX_SIZE;
    else
	s = size;

    table_size = (0x00000001 << s);
    if ((ht = (hashtable_t *) calloc(1, sizeof(hashtable_t))) == NULL) {
      elog("calloc error");
	return NULL;
    }
    ht->table_size = table_size;

    ht->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

    if (init_tables(ht, table_size) != true) {
	free(ht);
	return NULL;
    }

    return ht;
}

void free_hashtable(hashtable_t * ht)
{
    free_tables(ht->table);
    free(ht);
}


static unsigned int hashCode0(lkey_t key, const hashtable_t * ht)
{
  return ((key * 269) % ht->table_size);
}

static unsigned int hashCode1(lkey_t key, const hashtable_t * ht)
{
  return ((key * 271) % ht->table_size);
}


static void resize(hashtable_t * ht)
{
    node_t *old_node;
    node_t new_node;
    unsigned int i, j, k;
    node_t tmp;

    ht->old_table_size = ht->table_size;
    ht->old_table[0] = ht->table[0];
    ht->old_table[1] = ht->table[1];

    if (init_tables(ht, (ht->table_size * 2)) == false)
	return;

    ht->table_size *= 2;

    for (i = 0; i <= 1; i++) {
	for (j = 0; j < ht->old_table_size; j++) {
	    old_node = &ht->old_table[i][j];
	    if (old_node->stat == OCC) {
		set_node(&new_node, old_node->key, old_node->value, OCC);
		for (k = 0; k < ht->table_size; k++) {
		    if (swap_node(ht, 1, new_node, &tmp) == true) {
			ht->setSize++;
			break;
		    } else if (swap_node(ht, 2, tmp, &new_node) == true) {
			ht->setSize++;
			break;
		    }
		}
	    }
	}
    }

    free_tables(ht->old_table);
}


void show_hashtable(hashtable_t * ht)
{
  unsigned int i, j;

  lock(ht->mtx);
  for (i = 0; i <= 1; i++) {
    printf("table[%d]\t", i);
    for (j = 0; j < ht->table_size; j++) {
      if (ht->table[i][j].stat == EMP)
	printf("[NiL]");
      else if (ht->table[i][j].stat == DEL)
	printf("[DeL]");
      else
	printf("[%3d]", (int) ht->table[i][j].value);
    }
    printf("\n");
  }
  unlock(ht->mtx);
}


#ifdef _SINGLE_THREAD_

hashtable_t *ht;

int main(int argc, char **argv)
{
    val_t getval;
    int i;

    ht = init_hashtable(4);

    for (i = 0; i < 10; i++) {
      printf("add i = %d, setSize = %lu\n", i, ht->setSize);
      add(ht, i, i);
      show_hashtable(ht);
    }


    for (i = 0; i < 10; i++) {
      printf("del i = %d, setSize = %lu\n", i, ht->setSize);
      delete(ht, i, &getval);
      show_hashtable(ht);
    }

    free_hashtable(ht);

    return 0;
}

#endif
