/* ---------------------------------------------------------------------------
 * (Chain) Hash Table 
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.17
 * Copyright (C) 2009-2025  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>

#include "Hash.h"

static bool_t add_node_op(list_t *, node_t *);
static node_t *create_node(const lkey_t, const val_t);
static void free_node(node_t *);
static bool_t add_node(list_t *, const lkey_t, const val_t);
static bool_t delete_node(list_t *, const lkey_t, val_t *);
static bool_t find_node(list_t *, lkey_t);
static bool_t init_list(list_t *);
static bool_t init_bucket(hashtable_t *, const unsigned int);
static void free_bucket(list_t *, const unsigned int);
static bool_t policy(hashtable_t *);
static void resize(hashtable_t *);
static void show_list(const list_t *);
static unsigned int hashCode(lkey_t, const hashtable_t *);


#define lock(_mtx_)     pthread_mutex_lock(&(_mtx_))
#define unlock(_mtx_)   pthread_mutex_unlock(&(_mtx_))



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

    pred = l->head;
    curr = pred->next;

    if (curr == NULL) {
	l->head->next = newNode;
	newNode->next = NULL;
    } else {

      while ((curr != NULL) && (curr->key < newNode->key)) {
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

    if ((newNode = create_node(key, val)) == NULL)
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
  bool_t ret = true;
  unsigned int myBucket;

  lock(ht->mtx);
  myBucket = hashCode(key, ht);
  
  if (add_node(&ht->bucket[myBucket], key, val) == true)
    ht->setSize++;
  else 
    ret = false;
  
  if (policy(ht)) {
    resize(ht);
    fprintf (stdout, "Resized\n"); fflush(stdout);
  }

  unlock(ht->mtx);

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

    while ((curr != NULL) && (curr->key < key)) {
	pred = curr;
	curr = curr->next;
    }

    if ((key == curr->key) && (curr != NULL)) {
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
 * failure(not found): return false
 */
bool_t delete(hashtable_t * ht, const lkey_t key, val_t * getval)
{
  bool_t ret = true;
  unsigned int  myBucket;

  lock(ht->mtx);
  myBucket = hashCode(key, ht);

  if (delete_node(&ht->bucket[myBucket], key, getval) == true)
    ht->setSize--;
  else 
    ret = false;

  unlock(ht->mtx);
  
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
      while ((curr != NULL) && (curr->key < key)) {
	    pred = curr;
	    curr = curr->next;
	}

      if ((curr == NULL) || (key != curr->key))
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
  unsigned int myBucket;
    bool_t ret;

    lock(ht->mtx);
    myBucket = hashCode(key, ht);
    ret = find_node(&ht->bucket[myBucket], key);
    unlock(ht->mtx);

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
static bool_t init_list(list_t * l)
{
  if ((l->head = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    return false;
  }
  l->head->next = NULL;
  return true;
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
	 (list_t *) calloc(table_size, sizeof(list_t))) == NULL) {
      elog("calloc error");
      return false;
    }

    for (i = 0; i < table_size; i++)
      if (init_list(&ht->bucket[i]) == false)
	return false;

    return true;
}


static void free_bucket(list_t * bucket, const unsigned int table_size)
{
    int i;
    for (i = 0; i < table_size; i++)
	free(&(*bucket[i].head));

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
hashtable_t *init_hashtable(const unsigned int table_size)
{
    hashtable_t *ht;

    if ((ht = (hashtable_t *) calloc(1, sizeof(hashtable_t))) == NULL) {
      elog("calloc error");
      return NULL;
    }

    ht->table_size = table_size;
    ht->setSize = 0;

    ht->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

    if (init_bucket(ht, table_size) != true) {
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
    unsigned int i, myBucket;


    ht->old_table_size = ht->table_size;
    ht->old_bucket = ht->bucket;


    if (init_bucket(ht, ht->table_size * 2) == false)
	return;

    ht->table_size *= 2;

    for (i = 0; i < ht->old_table_size; i++) {
	l = &ht->old_bucket[i];

	pred = l->head;
	curr = pred->next;
	while (curr != NULL) {
	  pred->next = curr->next;	/*    <==>  next = curr->next; pred->next = next; */
	  
	  myBucket = hashCode(curr->key, ht);
	  add_node_op(&ht->bucket[myBucket], curr);
	  
	  curr = pred->next;
	}
    }

    free_bucket(ht->old_bucket, ht->old_table_size);
}

static void show_list(const list_t * l)
{
    node_t *pred, *curr;

    pred = l->head;
    curr = pred->next;

    while (curr != NULL) {
	printf("[%d(%d)]", (int) curr->key, (int) curr->value);
	curr = curr->next;
    }
    printf("\n");
}

void show_hashtable(hashtable_t * ht)
{
    unsigned int i;

    printf("hash_table:\n\t|\n");

    lock(ht->mtx);
    for (i = 0; i < ht->table_size; i++) {
	printf("\t+[[%3u]]->", i);
	show_list(&ht->bucket[i]);
    }
    printf("\n");
    unlock(ht->mtx);
}


static unsigned int hashCode(lkey_t key, const hashtable_t * ht)
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
      printf("add i = %d, setSize = %llu\n", i, ht->setSize);
      add(ht, i, i);
      show_hashtable(ht);
    }


    for (i = 0; i < 10; i++) {
      printf("del i = %d, setSize = %llu\n", i, ht->setSize);
      delete(ht, i, &getval);
      show_hashtable(ht);
    }

    free_hashtable(ht);

    return 0;
}

#endif
