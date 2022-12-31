/* ---------------------------------------------------------------------------
 * Concurrent Cuckoo Hash Table
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.17
 * Copyright (C) 2009-2023  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <assert.h>

#include "ConcurrentCuckooHash.h"

static void acquire (hashtable_t *, const lkey_t);
static void release (hashtable_t *, const lkey_t);
static void aLock (hashtable_t *);
static void aUnLock (hashtable_t *);

static bool_t add_node_atTail(list_t *, node_t *);
static node_t *create_node(const lkey_t, const val_t);
static void free_node(node_t *);
static node_t *get_head_node(list_t *);
static node_t *delete_node(list_t *, lkey_t);
static list_t *init_list(void);
static bool_t search_list(list_t *, const lkey_t);
static list_t *get_list(hashtable_t *, const int, const lkey_t);
static bool_t init_tables(hashtable_t *, const unsigned int, const int,
			const int);
static void free_tables(list_t ***, const unsigned int);
static unsigned long int hashCode0(lkey_t, const hashtable_t *);
static unsigned long int hashCode1(lkey_t, const hashtable_t *);
static void resize(hashtable_t *);
static bool_t contains_op(hashtable_t *, const lkey_t);
static bool_t relocate(hashtable_t *, int, int);
static void show_list(list_t *, const unsigned int);


/*
 * Utitles
 */
static void
acquire (hashtable_t *ht, const lkey_t key)
{
  unsigned int i, j;

  i = (unsigned int)(hashCode0(key, ht) % ht->mtx_size);
  j = (unsigned int)(hashCode1(key, ht) % ht->mtx_size);

  pthread_mutex_lock(&ht->mtx[0][i]);
  pthread_mutex_lock(&ht->mtx[1][j]);
}

static void
release (hashtable_t *ht, const lkey_t key)
{
  unsigned int i, j;

  i = (unsigned int)(hashCode0(key, ht) % ht->mtx_size);
  j = (unsigned int)(hashCode1(key, ht) % ht->mtx_size);

  pthread_mutex_unlock(&ht->mtx[0][i]);
  pthread_mutex_unlock(&ht->mtx[1][j]);
}

static void
aLock (hashtable_t *ht)
{
  int i;
  for (i = 0; i < ht->mtx_size; i++) {
    pthread_mutex_lock(&ht->mtx[0][i]);
    pthread_mutex_lock(&ht->mtx[1][i]);
  }

}

static void
aUnLock (hashtable_t *ht)
{
  int i;
  for (i = 0; i < ht->mtx_size; i++) {
    pthread_mutex_unlock(&ht->mtx[1][i]);
    pthread_mutex_unlock(&ht->mtx[0][i]);
  }
}


/*
 * Node
 */

/**
 */
static bool_t add_node_atTail(list_t * l, node_t * newNode)
{
    node_t *curr = l->head;

    while (curr->next != NULL)
       curr = curr->next;

    curr->next = newNode;
    newNode->next = NULL;

    l->size++;

    return true;
}

/**
 */
static node_t *create_node(const lkey_t key, const val_t val)
{
    node_t *node;

    if ((node = (node_t *)calloc(1, sizeof(node_t))) == NULL) {
      elog("calloc error");
      return NULL;
    }

    node->key = key;
    node->value = val;
    node->next = NULL;

    return node;
}


/**
 */
static void free_node(node_t * node)
{
    free(node);
}

static node_t *get_head_node(list_t * l)
{
  return l->head->next;
}

/**
 */
static node_t *delete_node(list_t * l, lkey_t key)
{
    node_t *pred, *curr;
    node_t *ret = NULL;

    pred = l->head;
    curr = pred->next;

    while (curr != NULL) {
	if (curr->key == key) {
	    ret = curr;
	    break;
	}
	pred = curr;
	curr = curr->next;
    }

    pred->next = curr->next;

    if (ret != NULL)
      l->size--;

    return ret;
}


/*
 * List
 */

/**
 */
static list_t *init_list(void)
{
    list_t *l;

    if ((l = (list_t *) calloc(1, sizeof(list_t))) == NULL) {
      elog("calloc error");
      	return NULL;
    }
    if ((l->head = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
      elog("calloc error");
      free (l);
      return NULL;
    }

    (l->head)->next = NULL;

    l->size = 0;

    return l;
}

/**
 */
void free_list(list_t * l)
{
    node_t *curr, *next;
    curr = l->head->next;


    while (curr != NULL) {
	next = curr->next;
	free_node(curr);
	curr = next;
    }

    free(l->head);
    free(l);
}

static bool_t search_list(list_t * list, const lkey_t key)
{
    bool_t ret = false;
    node_t *node;

    if (0 < list->size) {
	node = list->head->next;
	while (node != NULL) {
	    if (node->key == key) {
		ret = true;
		break;
	    }
	    node = node->next;
	}
    }
    return ret;
}

static list_t *get_list(hashtable_t * ht, const int no, const lkey_t key)
{
    unsigned int myBucket;
    list_t *list;
    if (no == 0) {
      myBucket = (unsigned int)(hashCode0(key, ht) % ht->table_size);
      list = ht->table[0][myBucket];
    } else {
      myBucket = (unsigned int)(hashCode1(key, ht) % ht->table_size);
      list = ht->table[1][myBucket];
    }
    return list;
}


/*
 * Table
 */

static bool_t init_tables(hashtable_t * ht, const unsigned int table_size,
			const int probe_size, const int threshold)
{
    unsigned int i;

    if ((ht->table[0] =
	 (list_t **) calloc(table_size, sizeof(list_t))) == NULL)
      return false;
    if ((ht->table[1] =
	 (list_t **) calloc(table_size, sizeof(list_t))) == NULL) {
      free (ht->table[0]);
      return false;
    }

    for (i = 0; i < table_size; i++) {
	ht->table[0][i] = init_list();
	ht->table[1][i] = init_list();
    }

    return true;
}


static void free_tables(list_t **table[2], const unsigned int table_size)
{
    int i;

    for (i = 0; i < table_size; i++) {
	free_list(table[0][i]);
	free_list(table[1][i]);
    }

    free(table[0]);
    free(table[1]);
}

/*
 * Hashtable
 */

hashtable_t *init_hashtable(const unsigned int size, const int probe_size,
			    const int threshold)
{
  int i;
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

    ht->setSize = 0;
    ht->table_size = table_size;
    ht->probe_size = probe_size;
    ht->threshold = threshold;

    ht->mtx_size = table_size;
    if ((ht->mtx[0] = (pthread_mutex_t *) calloc(table_size, sizeof(pthread_mutex_t))) == NULL) {
      elog("calloc eror");
      goto end;
    }

    if ((ht->mtx[1] = (pthread_mutex_t *) calloc(table_size, sizeof(pthread_mutex_t))) == NULL) {
      elog("calloc error");
      goto end;
    }

    for (i = 0; i <= table_size; i++) {
      ht->mtx[0][i] = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
      ht->mtx[1][i] = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    }

    if (init_tables(ht, table_size, probe_size, threshold) != true)
      goto end;

    return ht;

 end:
    free(ht->mtx[0]);
    free (ht);
    
    return NULL;
}

void free_hashtable(hashtable_t * ht, const unsigned int table_size)
{
  free_tables(ht->table, table_size);
  pthread_mutex_destroy(ht->mtx[0]);
  pthread_mutex_destroy(ht->mtx[1]);
  free(ht);
  
}


static unsigned long int hashCode0(lkey_t key, const hashtable_t * ht)
{
  return (key * 65699);
}

static  unsigned long int hashCode1(lkey_t key, const hashtable_t * ht)
{
    return (key * 65701);
}


static void resize(hashtable_t * ht)
{
    list_t *old_list;
    unsigned int i, j;

    aLock(ht);

    ht->old_table_size = ht->table_size;
    ht->old_table[0] = ht->table[0];
    ht->old_table[1] = ht->table[1];

    if (init_tables(ht, (ht->table_size * 2), ht->probe_size, ht->threshold) ==
	false)
      {
	aUnLock(ht);
	return;
      }

    ht->table_size *= 2;

    for (i = 0; i <= 1; i++) {
	for (j = 0; j < ht->old_table_size; j++) {
	    old_list = ht->old_table[i][j];

	    if (0 < old_list->size) {
		node_t *node = old_list->head->next;
		node_t *next;
		while (node != NULL) {
		    next = node->next;
		    unsigned int h0 = (unsigned int)(hashCode0(node->key, ht) % ht->table_size);
		    unsigned int h1 = (unsigned int)(hashCode1(node->key, ht) % ht->table_size);

		    list_t *set0 = ht->table[0][h0];
		    list_t *set1 = ht->table[1][h1];

		    if (set0->size < ht->threshold) {
		      if (add_node_atTail(set0, node) == true) ht->setSize++;
		    } else if (set1->size < ht->threshold) {
		      if (add_node_atTail(set1, node) == true) ht->setSize++;
		    } else if (set0->size < ht->probe_size) {
		      if (add_node_atTail(set0, node) == true) ht->setSize++;
		    } else if (set1->size < ht->probe_size) {
		      if (add_node_atTail(set1, node) == true) ht->setSize++;
		    } else {
			printf("ERRRRR!!!! a5e\n");
			exit(-1);
			free_node(node);
		    }
		    node = next;
		}
	    }
	}
    }

    //    free(ht->old_table[0]->head);
    //    free(ht->old_table[1]->head);
    free(ht->old_table[0]);
    free(ht->old_table[1]);
    //    free_tables(ht->old_table, ht->old_table_size);

    aUnLock(ht);    
}


static bool_t contains_op(hashtable_t * ht, const lkey_t key)
{
    list_t *list;
    bool_t ret = false;
    int i;

    for (i = 0; i <= 1; i++) {
	list = get_list(ht, i, key);
	if ((ret = search_list(list, key)) == true)
	    break;
    }
    return ret;
}


static bool_t relocate(hashtable_t * ht, int i, int hi)
{
    int hj = 0;
    int j = 1 - i;
    int LIMIT = 8;
    int round;
    list_t *iSet, *jSet;
    node_t *y;
    lkey_t lock_key;

    for (round = 0; round < LIMIT; round++) {
	iSet = ht->table[i][hi];
	y = get_head_node(iSet);
	lock_key = y->key;

	switch (i) {
	case 0:	    hj = (unsigned int)(hashCode1(y->key, ht) % ht->table_size);	    break;
	case 1:	    hj = (unsigned int)(hashCode0(y->key, ht) % ht->table_size);	    break;
	}

	acquire(ht, lock_key);
	jSet = ht->table[j][hj];

	if ((y = delete_node(iSet, lock_key)) != NULL) {
	    if (jSet->size < ht->threshold) {
	      if (add_node_atTail(jSet, y) == true) ht->setSize++;
	      release(ht, lock_key);
	      return true;
	    } else if (jSet->size < ht->probe_size) {
	      if (add_node_atTail(jSet, y) == true) ht->setSize++;
	      i = 1 - i;
	      hi = hj;
	      j = 1 - j;
	    } else {
	      if (add_node_atTail(iSet, y) == true) ht->setSize++;
	      release(ht, lock_key);
	      return false;
	    }
	} else if (iSet->size >= ht->threshold)
	  continue;
	else {
	  release(ht, lock_key);
	  return true;
	}
	release(ht, lock_key);
    }

    return false;
}


bool_t add(hashtable_t * ht, const lkey_t key, const val_t val)
{
    node_t *newNode;
    unsigned int h0, h1;
    int i = -1;
    unsigned int h;
    bool_t mustResize = false;
    list_t *set0, *set1;


    acquire(ht, key);

    if (contains_op(ht, key) == true) {
      release(ht, key);
	return false;
    }

    if ((newNode = create_node(key, val)) == NULL) {
      release(ht, key);
      return false;
    }

    h0 = (unsigned int)(hashCode0(key, ht) % ht->table_size);
    h1 = (unsigned int)(hashCode1(key, ht) % ht->table_size);
    set0 = ht->table[0][h0];
    set1 = ht->table[1][h1];

    if (set0->size < ht->threshold) {
      if (add_node_atTail(set0, newNode) == true) ht->setSize++;
      release(ht, key);
      return true;
    } else if (set1->size < ht->threshold) {
      if (add_node_atTail(set1, newNode) == true) ht->setSize++;
      release(ht, key);
      return true;
    } else if (set0->size < ht->probe_size) {
      if (add_node_atTail(set0, newNode) == true) ht->setSize++;
      i = 0;	h = h0;
    } else if (set1->size < ht->probe_size) {
      if (add_node_atTail(set1, newNode) == true) ht->setSize++;
      i = 1;	h = h1;
    } else {
      free_node(newNode);
      mustResize = true;
    }
    release(ht, key);
    
    if (mustResize == true) {
      resize(ht);
      fprintf (stderr, "Resized\n");
      add(ht, key, val);
    } else if (relocate(ht, i, h) == false) {
      //	show_hashtable(ht);
      resize(ht);
    }
    return true;
}


bool_t delete(hashtable_t * ht, const lkey_t key, val_t * getval)
{
    bool_t ret = false;
    node_t *node;
    list_t *set0, *set1;

    acquire(ht, key);

    set0 = get_list(ht, 0, key);

    if (search_list(set0, key) == true) {
	if ((node = delete_node(set0, key)) != NULL) {
	    *getval = node->value;
	    ht->setSize--;
	    free_node(node);
	    ret = true;
	}
    } else {
	set1 = get_list(ht, 1, key);
	if (search_list(set1, key) == true) {
	    if ((node = delete_node(set1, key)) != NULL) {
		*getval = node->value;
		ht->setSize--;
		free_node(node);
		ret = true;
	    }
	}
    }

    release(ht, key);

    return ret;
}


bool_t contains(hashtable_t * ht, const lkey_t key)
{
    bool_t ret;

    acquire(ht, key);
    ret = contains_op(ht, key);
    release(ht, key);

    return ret;
}

static void show_list(list_t * list, const unsigned int j)
{
    node_t *node;

    if (0 == list->size)
	printf("[%d()]", j);
    else {
	printf("[%d(", j);

	node = list->head->next;

	if (node != NULL) {
	  printf("%lu", (unsigned long int)node->key);
	  node = node->next;
	}
	while (node != NULL) {
	  printf(",%lu", (unsigned long int)node->key);
	    node = node->next;
	}

	printf(")]");
    }
}

void show_hashtable(hashtable_t * ht)
{

    unsigned int i, j;
    for (i = 0; i <= 1; i++) {
	printf("table[%d]\t", i);
	for (j = 0; j < ht->table_size; j++) {
	    show_list(ht->table[i][j], j);
	}
	printf("\n");
    }

}



#ifdef _SINGLE_THREAD_

hashtable_t *ht;

int main(int argc, char **argv)
{
    val_t getval;
    int i;

    ht = init_hashtable(4, 4, 2);

    for (i = 0; i < 10; i++) {
      printf("del i = %d, setSize = %lu\n", i, ht->setSize);
      add(ht, i, i);
      show_hashtable(ht);
    }


    for (i = 0; i < 10; i++) {
      printf("del i = %d, setSize = %lu\n", i, ht->setSize);
      delete(ht, i, &getval);
      show_hashtable(ht);
    }

    free_hashtable(ht, ht->table_size);

    return 0;
}

#endif

