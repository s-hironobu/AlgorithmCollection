/* ---------------------------------------------------------------------------
 * Refinable Hash Table
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.17
 * Copyright (C) 2009-2024  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <assert.h>

#include "RefinableHash.h"

static bool_t add_node_op(list_t *, node_t *);
static bool_t add_node(list_t *, const lkey_t, const val_t);
static bool_t delete_node(list_t *, const lkey_t, val_t *);
static bool_t find_node(list_t *, lkey_t);
static bool list_init(list_t *);
static bool_t init_bucket(hashtable_t *, const unsigned int,
			const unsigned int);
static bool_t policy(hashtable_t *);
static void resize(hashtable_t *);


#define lock(mtx)      pthread_mutex_lock((mtx))
#define unlock(mtx)    pthread_mutex_unlock((mtx))

/*
 * node_t *create_node(const lkey_t key, const val_t val)
 *
 * Create node '(key, val)'
 *
 * success : return pointer to this node
 * failure : return NULL
 */
static node_t *create_node(const lkey_t key, const val_t val)
{
    node_t *newNode;

    if ((newNode = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
      elog("calloc error");
	return NULL;
    }

    newNode->key = key;
    newNode->value = val;
    return newNode;
}

static void free_node(node_t * node)
{
    free(node);
}

/*
 * bool_t add_node_op(list_t * l, node_t * newNode)
 *
 * Add node 'newNode' to list 'l'
 *
 * success : return true
 * failure : return false
 */
static bool_t add_node_op(list_t * l, node_t * newNode)
{
    node_t *pred, *curr;
    bool_t ret = true;

    assert(l->head != NULL);
    pred = l->head;
    curr = pred->next;

    if (curr == NULL) {
	l->head->next = newNode;
	newNode->next = NULL;
    } else {

	while (curr != NULL && curr->key < newNode->key) {
	    pred = curr;
	    curr = curr->next;
	}

	if (curr == NULL || newNode->key != curr->key) {
	    newNode->next = curr;
	    pred->next = newNode;
	} else
	    ret = false;

    }
    return ret;
}


/*
 * bool_t add_node(list_t * l, const lkey_t key, const val_t val)
 *
 * Create node '(key, val)', and add to list 'l'.
 *
 * success : return true
 * failure : return false
 */
static bool_t add_node(list_t * l, const lkey_t key, const val_t val)
{
    node_t *newNode;

    if ((newNode = create_node(key, val)) == 0)
	return false;

    if (add_node_op(l, newNode) != true) {
	free_node(newNode);
	return false;
    }

    return true;
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
    unsigned int myBucket, table_size;
    bool_t ret = false;
    int retry = 2;


    do {
	table_size = ht->table_size;
	myBucket = hashCode(key, ht);	/* T1: */
	lock(ht->bucket[myBucket].mtx);	/* T2: */

	if (table_size == ht->table_size)
	{
	  /* resize() executed between T1: and T2:. */
	    if (add_node(&ht->bucket[myBucket], key, val) == true) {
		ht->setSize++;
		ret = true;
		unlock(ht->bucket[myBucket].mtx);
		break;
	    }
	}

	unlock(ht->bucket[myBucket].mtx);
    }
    while (retry-- > 0);

    if (policy(ht)) {
      resize(ht);
      fprintf (stderr, "Resized\n");
    }
    return ret;
}


/*
 * bool_t delete_node(list_t * l, const lkey_t key, val_t * getval)
 *
 * Delete node'(key, val)' by the key from list l, and write the val to *getval.
 *
 * success : return true
 * failure : return false
 */
static bool_t delete_node(list_t * l, const lkey_t key, val_t * getval)
{
    node_t *pred, *curr;

    pred = l->head;
    curr = pred->next;

    if (curr == NULL)
	return false;

    while (curr != NULL && curr->key < key) {
	pred = curr;
	curr = curr->next;
    }

    if (key == curr->key && curr != NULL) {
	*getval = curr->value;
	pred->next = curr->next;
	free_node(curr);
    } else
	return false;

    return true;
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
    unsigned int myBucket, table_size;
    bool_t ret = false;
    int retry = 2;

    do {
	table_size = ht->table_size;
	myBucket = hashCode(key, ht);	/* T1: */
	lock(ht->bucket[myBucket].mtx);	/* T2: */

	if (table_size == ht->table_size)
	{
	  /* resize() executed between T1: and T2:. */
	    if (delete_node(&ht->bucket[myBucket], key, getval) == true) {
		ht->setSize--;
		ret = true;
		unlock(ht->bucket[myBucket].mtx);
		break;
	    }
	}
	unlock(ht->bucket[myBucket].mtx);
    }
    while (retry-- > 0);

    return ret;
}

/*
 * bool_t find_node(list_t * l, lkey_t key)
 *
 * Find node'(key, val)' by the key from list l.
 *
 * success(found) : return true
 * failure(not found) : return false
 */
static bool_t find_node(list_t * l, lkey_t key)
{
    node_t *pred, *curr;

    pred = l->head;
    curr = pred->next;

    if (curr == NULL) {
	return false;
    } else {

	while (curr != NULL && curr->key < key) {
	    pred = curr;
	    curr = curr->next;
	}

	if (curr == NULL || key != curr->key)
	    return false;
    }
    return true;
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
    unsigned int myBucket = hashCode(key, ht);
    bool_t ret;

    lock(ht->bucket[myBucket].mtx);
    ret = find_node(&ht->bucket[myBucket], key);
    unlock(ht->bucket[myBucket].mtx);

    return ret;
}


/*
 * bool_t *init_list(list_t *l)
 *
 * Create node 'head', and add to list 'l'.
 *
 * success : return true
 * failure : return false
 */
static bool_t list_init(list_t * l)
{

  if ((l->head = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    return false;
  }
  l->head->next = NULL;

  /* Create l->mtx. Set l->mtx's value in init_bucket() */
  if ((l->mtx = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t))) == NULL) {
    elog("calloc error");
    free (l->head);
    return false;
  }

  return true;
}


/*
 * bool_t init_bucket(hashtable_t * ht, const unsigned int init_table_size, const unsigned int new_table_size)
 *
 * Initialize all buckets of hashtable 'ht'.
 *
 * success : return true
 * failure : return false
 */
static bool_t
init_bucket(hashtable_t * ht, const unsigned int init_table_size,
	    const unsigned int new_table_size)
{
    unsigned int i;
#ifdef PTHREAD_MUTEX_FAST_NP
    pthread_mutexattr_t mtx_attr;
#endif

    if ((ht->bucket =
	 (list_t *) calloc(new_table_size, sizeof(list_t))) == NULL) {
      elog("calloc error");
      return false;
    }

    for (i = 0; i < new_table_size; i++) {
      if (list_init(&ht->bucket[i]) != true)
	return false;

      if (i < init_table_size) {
	ht->bucket[i].mtx = ht->old_bucket[i].mtx;
      }
      else {
#ifdef PTHREAD_MUTEX_FAST_NP
	pthread_mutexattr_init(&mtx_attr);
	pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_FAST_NP);
	pthread_mutex_init(ht->bucket[i].mtx, mtx_attr);
	pthread_mutexattr_destroy(&mtx_attr);
#else
	pthread_mutex_init(ht->bucket[i].mtx, NULL);
#endif
      }
    }

    return true;
}

static void free_bucket(list_t * bucket, const unsigned int table_size)
{
    int i;
    for (i = 0; i < table_size; i++) {
      pthread_mutex_destroy(bucket[i].mtx);
      free(&(*bucket[i].head));
    }
    free(bucket);
}


/*
 * hashtable_t *init_hashtable(const unsigned int table_size)
 *
 * Create hashtable size of 'table_size'.
 *
 * success : return pointer of hashtable
 * failure : return NULL
 */

hashtable_t *init_hashtable(const unsigned int table_size)
{
    hashtable_t *ht;

    if ((ht = (hashtable_t *) calloc(1, sizeof(hashtable_t))) == NULL) {
      elog("calloc error");
      return NULL;
    }

    ht->table_size = table_size;
    ht->setSize = 0;

    if (init_bucket(ht, 0, table_size) != true) {
	free(ht);
	return NULL;
    }

    return ht;
}

void free_hashtable(hashtable_t * ht)
{
    free_bucket(ht->bucket, ht->table_size);
    free(ht);
}


static bool_t policy(hashtable_t * ht)
{
    return ((int) (ht->setSize / ht->table_size) > 4 ? true : false);
}

static void resize(hashtable_t * ht)
{
    list_t *l;
    node_t *pred, *curr;
    unsigned int i, myBucket, table_size;

    table_size = ht->table_size;
    for (i = 0; i < table_size; i++)
	lock(ht->bucket[i].mtx);

    if (table_size != ht->table_size) {
	for (i = 0; i < table_size; i++)
	    unlock(ht->bucket[i].mtx);
	return;
    }

    ht->old_table_size = ht->table_size;
    ht->old_bucket = ht->bucket;

    if (init_bucket(ht, ht->table_size, ht->table_size * 2) == false)
	return;

    ht->table_size *= 2;

    for (i = 0; i < ht->old_table_size; i++) {
	l = &ht->old_bucket[i];

	pred = l->head;
	curr = pred->next;
	while (curr != NULL) {
	  pred->next = curr->next;	/*      next = curr->next;      pred->next = next; */
	  
	  myBucket = hashCode(curr->key, ht);
	  add_node_op(&ht->bucket[myBucket], curr);
	  
	  curr = pred->next;
	}
    }

    for (i = 0; i < ht->old_table_size; i++)
	unlock(ht->bucket[i].mtx);

    free_bucket(ht->old_bucket, ht->old_table_size);

}

void show_list(const list_t * l)
{
    node_t *pred, *curr;

    pred = l->head;
    curr = pred->next;

    while (curr != NULL) {
	printf("[%lu(%ld)]", (unsigned long int) curr->key,
	       (long int) curr->value);
	pred = curr;
	curr = curr->next;
    }
    printf("\n");
}

void show_hashtable(hashtable_t * ht)
{
    unsigned int i;

    printf("hash_table:\n\t|\n");

    for (i = 0; i < ht->table_size; i++) {
	printf("\t+[[%3d]]->", i);
	show_list(&ht->bucket[i]);
    }
    printf("\n");
}


unsigned int hashCode(lkey_t key, const hashtable_t * ht)
{
    return (key % ht->table_size);
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
