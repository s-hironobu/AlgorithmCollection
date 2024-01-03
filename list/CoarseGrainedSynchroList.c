/* ---------------------------------------------------------------------------
 * Coarse-Grained Synchronization Singly-linked List
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Oct.25
 * Copyright (C) 2009-2024  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "common.h"
#include "CoarseGrainedSynchroList.h"

#define lock(_mtx_) pthread_mutex_lock(&(_mtx_))
#define unlock(_mtx_) pthread_mutex_unlock(&(_mtx_))
#ifdef _FREE_
#define free_node(_node_) free(_node_)
#else
#define free_node(_node_) ;
#endif

static node_t *create_node(const lkey_t, const val_t);

/*
 * node_t *create_node(const lkey_t key, const val_t val)
 *
 * Create node '(key, val)'.
 *
 * success : return pointer of this node
 * failure : return NULL
 */
static node_t *create_node(const lkey_t key, const val_t val)
{
  node_t *node;
  
  if ((node = calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    return NULL;
  }
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
bool_t add(list_t * list, const lkey_t key, const val_t val)
{
  node_t *pred, *curr;
  node_t *newNode;
  bool_t ret = true;
  
  if ((newNode = create_node(key, val)) == NULL)
    return false;
  
  lock(list->mtx);
  
  pred = list->head;
  curr = pred->next;
  
  if (curr == list->tail) {
    list->head->next = newNode;
    newNode->next = list->tail;
  } else {
    while (curr != list->tail && curr->key < key) {
      pred = curr;
      curr = curr->next;
    }
    
    if (curr != list->tail && key == curr->key) {
      free_node(newNode);
      ret = false;
    } else {
      newNode->next = curr;
      pred->next = newNode;
    }
  }
  
  unlock(list->mtx);

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
  
  lock(list->mtx);
  
  pred = list->head;
  curr = pred->next;
  
  if (curr == list->tail) {
    ret = false;
  } else {
    while (curr->key < key && curr != list->tail) {
      pred = curr;
      curr = curr->next;
    }
    
    if (key == curr->key) {
      *val = curr->val;
      
      pred->next = curr->next;
      free_node(curr);
    } else
      ret = false;
  }
  
  unlock(list->mtx);
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
  
  lock(list->mtx);
  
  pred = list->head;
  curr = pred->next;
  
  if (curr == list->tail) {
    ret = false;
  } else {
    while (curr != list->tail && curr->key < key) {
      pred = curr;
      curr = curr->next;
    }
    if (curr != list->tail && key == curr->key) {
      *val = curr->val;
      ret = true;
    }
    else
      ret = false;
  }
  
  unlock(list->mtx);
  return ret;
}

/*
 * list_t *init_list(void)
 *
 * Create initial list.
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
    goto end;
  }
  
  if ((list->tail = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    goto end;
  }
  
  list->head->next = list->tail;
  list->tail->next = NULL;
  list->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
  
  return list;
  
 end:
  free(list->head);
  free(list);
  return NULL;
}

/*
 * void free_list(list_t * list)
 */
void free_list(list_t * list)
{
  node_t *curr, *next;

  curr = list->head->next;

  while (curr != list->tail) {
    next = curr->next;
    free_node (curr);
    curr = next;
  }

  free_node(list->head);
  free_node(list->tail);
  pthread_mutex_destroy(&list->mtx);
  free(list);
}

/*
 * void show_list(const list_t * l)
 */
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

#include <string.h>

list_t *list;

int main (int argc, char **argv)
{
  lkey_t i;
  val_t g;

  list = init_list();

  show_list (list);

  for (i = 0; i < 10; i++) {
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
