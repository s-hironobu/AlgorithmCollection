/* ---------------------------------------------------------------------------
 * Fine-Grained Synchronization Singly-linked List: pthread_mutex version
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Oct.25
 * Copyright (C) 2009-2025  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <assert.h>

#include "FineGrainedSynchroList.h"

static node_t *create_node(const lkey_t, const val_t);

#define lock(_mtx_) pthread_mutex_lock(&_mtx_)
#define unlock(_mtx_) pthread_mutex_unlock(&_mtx_)
#ifdef _FREE_
#define free_node(node) do {pthread_mutex_destroy(&node->mtx);\
    free(node);  }while(0);
#else
#define free_node(node) ;
#endif

/*
 * node_t *create_node(const lkey_t key, const val_t val)
 *
 * Create node '(key, val)'.
 *
 * success : return pointer to this node
 * failure : return NULL
 */
static node_t *create_node(const lkey_t key, const val_t val)
{
    node_t *node;

    if ((node = calloc(1, sizeof(node_t))) == NULL) {
      elog("calloc error");
      return NULL;
    }
    node->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

    node->key = key;
    node->val = val;

    return node;
}


/*
 * bool_t add(list_t * list, const lkey_t key, const val_t val)
 *
 * Add node'(key,val)' to list l. key should be unique.
 *
 * success : return true
 * failure(key already exists) : return false
 */
bool add(list_t * list, const lkey_t key, const val_t val)
{
    node_t *pred, *curr;
    node_t *newNode;
    bool ret = true;

    if ((newNode = create_node(key, val)) == NULL)
      return false;

    lock(list->head->mtx);  /* get lock of list->head. */

    pred = list->head;
    curr = pred->next;      /* curr is next node of list->head */

    lock(curr->mtx);        /* get lock of curr */

    if (curr == list->tail) {
      /* there is no node in the list */
	list->head->next = newNode;
	newNode->next = list->tail;

	unlock(list->head->mtx);
	unlock(curr->mtx);
    } else {
	while (curr != list->tail && curr->key < key) {
	  /* getting locks of pred and curr */
	  unlock(pred->mtx);
	  /* rename curr to pred. */
	  pred = curr;  curr = curr->next; 
	  lock(curr->mtx); 
	  /* getting locks of pred and curr. */
	}

	/* 
	 *  assert((pred->key) < (newNode->key) <= (curr->key))
	 */

	if (curr != list->tail && key == curr->key) {  
	  /* already there is a node has same key. */
	    free_node(newNode);
	    ret = false;
	} else {	
	    /* add node */
	    newNode->next = curr;
	    pred->next = newNode;
	}
	unlock(pred->mtx);
	unlock(curr->mtx);
    }

    return ret;
}

/*
 * bool_t delete(list_t * list, const lkey_t key, val_t *val)
 *
 * Delete node'(key, val)' from list l, and write the val to *getval.
 * 
 * success : return true
 * failure(not found) : return false
 */
bool_t delete(list_t * list, const lkey_t key, val_t *val)
{
    node_t *pred, *curr;
    bool_t ret = true;

    lock(list->head->mtx);  /* get lock of list->head */

    pred = list->head;
    curr = pred->next;      /* curr is the next node of list->head */

    lock(curr->mtx);        /* get lock of curr */

    if (curr == list->tail) {     
      /* there is no node in the list. */
	unlock(list->head->mtx);
	unlock(curr->mtx);
	ret = false;
    } else {
	while (curr != list->tail && curr->key < key) {
	    unlock(pred->mtx);
	    pred = curr;
	    curr = curr->next;
	    lock(curr->mtx);
	}

	/* assert((pred->key) < (newNode->key) <= (curr->key)) */
	if (curr != list->tail && key == curr->key) {
	  *val = curr->val;
	  pred->next = curr->next;

	  unlock(curr->mtx);	  free_node(curr); /* release lock of curr, and delete curr node. */
	  unlock(pred->mtx);	  /* after curr node deletes, release lock of pred. */
	} else {
	  unlock(pred->mtx);
	  unlock(curr->mtx);
	  
	  ret = false;
	}
    }
    return ret;
}

/*
 * bool_t find(list_t * list, const lkey_t key, val_t *val)
 *
 * Find node'(key, val)' by the key from list 'l', and write the val to *getval.
 *
 * success : return true
 * failure(not found) : return false
 */
bool_t find(list_t * list, const lkey_t key, val_t *val)
{
    node_t *pred, *curr;
    bool_t ret = true;

    lock(list->head->mtx);  /* get lock of list->head */

    pred = list->head;
    curr = pred->next;      /* curr is the next node of list->head */

    lock(curr->mtx);        /* get lock of curr */

    if (curr == list->tail) {
      /* there is no node in the list. */
      unlock(list->head->mtx);
      unlock(curr->mtx);
      ret = false;
    } else {
      while (curr != list->tail && curr->key < key) {
	unlock(pred->mtx);
	pred = curr;
	curr = curr->next;
	lock(curr->mtx);
      }
      
      /*
       * assert(pred->key < newNode->key);
       * assert(newNode->key <= curr->key);
       */
      if (curr != list->tail && key == curr->key)
	*val = curr->val;
      else
	ret = false;
      
      unlock(pred->mtx);
      unlock(curr->mtx);
    }
    return ret;
}


/*
 * list_t *init_list(void)
 *
 * create initial list.
 *
 * success : return pointer to the initial list
 * failure : return NULL
 */
list_t *init_list(void)
{
    list_t *list;

    if ((list = (list_t *) calloc(1, sizeof(list_t))) == NULL) {
      elog("calloc error");
      return NULL;
    }
    
    if ((list->head = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
      elog("calloc error");
      free (list);
      goto end;
    }
    list->head->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    
    if ((list->tail = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
      elog("calloc error");
      goto end;
    }
    list->tail->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    
    list->head->next = list->tail;
    list->tail->next = NULL;
  
  return list;

 end :
  free (list->head);
  free (list);
  return NULL;
}

void free_list(list_t * list)
{
  node_t *curr, *next;

  curr = list->head->next;
  while (curr != list->tail)
    {
      next = curr->next;
      free_node (curr);
      curr = next;
    }
  
  free(list->head);
  free(list->tail);
  free(list);
}


void show_list(const list_t * l)
{
    node_t *pred, *curr;

    pred = l->head;
    curr = pred->next;

    printf ("list:\n\t");
    while (curr != l->tail) {
      printf(" [%lu:%ld]", (unsigned long int) curr->key, (long int)curr->val);
      pred = curr;
      curr = curr->next;
    }
    printf("\n");
}

#ifdef _SINGLE_THREAD_


list_t *list;

int main (int argc, char **argv)
{
  lkey_t i;
  val_t g;

  list = init_list();

  show_list (list);

  for (i = 0; i < 10; i++) 
    {
      add (list, (lkey_t)i, (val_t) i * 10);
    }

  show_list (list);

  for (i = 0; i < 5; i++) {
    delete (list, (lkey_t)i, &g);
    printf ("ret = %ld\n", (long int)g);
  }

  show_list (list);

  free_list (list);

  return 0;
}

#endif
