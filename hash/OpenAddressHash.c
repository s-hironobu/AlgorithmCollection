/* ---------------------------------------------------------------------------
 * Open-Addressed Hash Table
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.17
 * Copyright (C) 2009-2025  suzuki hironobu
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <assert.h>

#include "OpenAddressHash.h"

static bool_t init_bucket(hashtable_t *, const unsigned int);
static void free_bucket(node_t *);
static bool_t policy(hashtable_t *);
static void resize(hashtable_t *);
static void add_op(hashtable_t *, node_t *, const lkey_t, const val_t);
static void del_op(hashtable_t *, node_t *);
static void set_node(node_t *, const lkey_t, const val_t,
		     const node_stat);
static unsigned int hashCode(lkey_t, unsigned int, const hashtable_t *);


#define lock(mtx)      pthread_mutex_lock(&(mtx))
#define unlock(mtx)    pthread_mutex_unlock(&(mtx))


static void
set_node(node_t * node, const lkey_t key, const val_t val,
	 const node_stat stat)
{
    assert(node != NULL);
    node->key = key;
    node->value = val;
    node->stat = stat;
}


static void
add_op(hashtable_t * ht, node_t * node, const lkey_t key,
       const val_t val)
{
    set_node(node, key, val, OCC);
    ht->setSize++;
}


/*
 * bool_t add(hashtable_t * ht, const lkey_t key, const val_t val)
 *
 * Add node '(key, val)' to hashtable 'ht'.
 *
 * success : return true
 * failure : return false
 */
bool_t add(hashtable_t * ht, const lkey_t key, const val_t val)
{
    unsigned int i, myBucket;
    node_t *node;
    bool_t ret = false;

    lock(ht->mtx);

    for (i = 0; i < ht->table_size; i++) {
	myBucket = hashCode(key, i, ht);
	node = &ht->bucket[myBucket];

	if (node->stat != OCC) {
	    add_op(ht, node, key, val);
	    ret = true;
	    break;
	}
    }

    if (policy(ht)) {
      resize(ht);
      fprintf (stderr, "Resized\n");
    }
    unlock(ht->mtx);

    return ret;
}

static void del_op(hashtable_t * ht, node_t * node)
{
    set_node(node, (lkey_t) NULL, (lkey_t) NULL, DEL);
    ht->setSize--;
}


/*
 * bool_t delete(hashtable_t * ht, const lkey_t key, val_t * getval)
 *
 * Delete node'(key, val)' by the key from hashtable ht, and write the val to *getval.
 *
 * success : return true
 * failure(key not found): return false
 */
bool_t delete(hashtable_t * ht, const lkey_t key, val_t * getval)
{
    unsigned int i, myBucket;
    node_t *node;
    bool_t ret = false;

    lock(ht->mtx);

    for (i = 0; i < ht->table_size; i++) {
	myBucket = hashCode(key, i, ht);
	node = &ht->bucket[myBucket];
	if (node->stat == EMP) {
	    ret = false;
	    break;
	} else if (node->stat != DEL && node->key == key) {
	    ret = true;
	    *getval = node->value;
	    del_op(ht, node);
	    break;
	}
    }

    unlock(ht->mtx);

    return ret;
}

/*
 * bool_t find(hashtable_t * ht, const lkey_t key)
 *
 * Find node'(key, val)' by the key from hashtable ht, and write the val to *getval.
 *
 * success : return true
 * failure(not found): return false
 */
bool_t find(hashtable_t * ht, const lkey_t key)
{
    unsigned int i, myBucket;
    node_t *node;
    bool_t ret = false;

    lock(ht->mtx);

    for (i = 0; i < ht->table_size; i++) {
	myBucket = hashCode(key, i, ht);
	node = &ht->bucket[myBucket];
	if (node->stat == EMP) {
	    ret = false;
	    break;
	} else if (node->stat != DEL && node->key == key) {
	    ret = true;
	    break;
	}
    }

    unlock(ht->mtx);

    return ret;
}


/*
 * bool_t init_bucket(hashtable_t * ht, const unsigned int table_size)
 *
 * Initialize all buckets of hashtable 'ht'.
 *
 * success : return true
 * failure : return false
 */
static bool_t init_bucket(hashtable_t * ht, const unsigned int table_size)
{
    unsigned int i;

    if ((ht->bucket =
	 (node_t *) calloc(table_size, sizeof(node_t))) == NULL) {
      elog("calloc error");
      return false;
    }

    for (i = 0; i < table_size; i++)
	set_node(&ht->bucket[i], (lkey_t) NULL, (val_t) NULL, EMP);

    ht->setSize = 0;

    return true;
}

static void free_bucket(node_t * bucket)
{
    free(bucket);
}


/*
 * hashtable_t *init_hashtable(const unsigned int table_size)
 *
 * Create hashtable size of 'table_size'.
 *
 * success : return pointer to this hashtable
 * failure : return NULL
 */
hashtable_t *init_hashtable(const unsigned int size)
{
    hashtable_t *ht;
    unsigned int table_size = (0x0001 << size);

    if ((ht = (hashtable_t *) calloc(1, sizeof(hashtable_t))) == NULL) {
      elog("calloc error");
	return NULL;
    }

    ht->table_size = table_size;

    ht->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

    if (init_bucket(ht, table_size) != true) {
	free(ht);
	return NULL;
    }

    return ht;
}

void free_hashtable(hashtable_t * ht)
{
    free_bucket(ht->bucket);
    free(ht);
}


static bool_t policy(hashtable_t * ht)
{
    //  return ((ht->table_size * 3 / 4) < ht->setSize ? true : false);
    return ((ht->table_size * 4 / 5) < ht->setSize ? true : false);
}

static void resize(hashtable_t * ht)
{
    node_t *old_node, *new_node;
    unsigned int i, j, myBucket;

    ht->old_table_size = ht->table_size;
    ht->old_bucket = ht->bucket;

    if (init_bucket(ht, (ht->table_size * 2)) == false)
	return;

    ht->table_size *= 2;

    for (i = 0; i < ht->old_table_size; i++) {
	old_node = &ht->old_bucket[i];

	if (old_node->stat == OCC) {
	    for (j = 0; j < ht->table_size; j++) {
		myBucket = hashCode(old_node->key, j, ht);
		new_node = &ht->bucket[myBucket];
		if (new_node->stat != OCC) {
		    add_op(ht, new_node, old_node->key, old_node->value);
		    break;
		}
	    }
	}
    }
    free_bucket(ht->old_bucket);
}


static unsigned int hashCode(lkey_t key, unsigned int i,
			     const hashtable_t * ht)
{
    return ((key + i) % ht->table_size);
}


void show_hashtable(hashtable_t * ht)
{
    unsigned int i;

    lock(ht->mtx);
    for (i = 0; i < ht->table_size; i++) {
	if (ht->bucket[i].stat == EMP)
	    printf("[NiL]");
	else if (ht->bucket[i].stat == DEL)
	    printf("[DeL]");
	else
	    printf("[%3d]", (int) ht->bucket[i].value);
    }
    printf("\n");
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
