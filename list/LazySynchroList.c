/* ---------------------------------------------------------------------------
 * Lazy Synchronization Singly-linked List: pthread_mutex version
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Oct.25
 * Copyright (C) 2009-2025  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>

#include "LazySynchroList.h"

static node_t *create_node(const lkey_t, const val_t);

#define lock(mtx) pthread_mutex_lock(mtx)
#define unlock(mtx) pthread_mutex_unlock(mtx)

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
    elog("calloc error ");
    return NULL;
  }
    
  node->key = key;
  node->val = val;
  node->marked = false;
  node->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;

  return node;
}

#ifdef _FREE_
#define free_node(_node)   do {			                  \
                            pthread_mutex_destroy(&(_node->mtx));  \
			    free(_node);}while(0);
#else
#define free_node(_node)  ;
#endif


/*
 * bool_t add(list_t * list, const lkey_t key, const val_t val)
 *
 * Add node'(key,val)' to list l. key should be unique.
 *
 * success : return true
 * failure(key already exists) : return false
 */
#define validate(pred, curr) (!pred->marked && !curr->marked && pred->next == curr)

bool_t add(list_t * l, const lkey_t key, const val_t val)
{
    node_t *pred, *curr;
    node_t *newNode;
    bool_t ret = true;

    if ((newNode = create_node(key, val)) == NULL)
      return false;

    while (1) {
      pred = l->head;
      curr = pred->next;

      /* Traverse the list until before the node of "key" without acquiring any locks. */
      while (curr->key < key && curr != l->tail) {
	pred = curr;
	curr = pred->next; 
      }
      
      /* 
       * During this period, there is a possibility that 
       * another thread does something (changes node, adds node, delete curr or pred).
       */
      
      /* If lock acquires fail, start over from the beginning. */
      if (lock(&pred->mtx) != 0) {
	continue;
      } else if (lock(&curr->mtx) != 0) {
	unlock(&pred->mtx);
	continue;
      }
      
      assert ((pred->key < key) && (key <= curr->key));
      
      /* begin critical section */
      if (validate(pred, curr)) {
	if (key == curr->key) {
	  ret = false;
	  free_node(newNode);
	} else {
	  newNode->next = curr;
	  pred->next = newNode;
	}
	/* end critical section */
	unlock(&pred->mtx);	unlock(&curr->mtx);
	break;
      }
      unlock(&pred->mtx);      unlock(&curr->mtx);
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
bool_t delete(list_t * l, const lkey_t key, val_t *val)
{
  node_t *pred, *curr;
  bool_t ret = true;
  
  while(1) {
    pred = l->head;
    curr = pred->next;
    
    if (curr == l->tail) {
      ret = false;
      break;
    } else {
      /* Traverse the list until before the node of "key" without acquiring any locks. */
      while (curr->key < key && curr != l->tail) {
	pred = curr;
	curr = pred->next;
      }
      /* 
       * During this period, there is a possibility that 
       * another thread does something (changes node, adds node, delete curr or pred).
       */
      /* If lock acquires fail, start over from the beginning. */
      if (lock(&pred->mtx) != 0) {
	continue;
      } else if (lock(&curr->mtx) != 0) {
	unlock(&pred->mtx);
	continue;
      }
      if (!(pred->key < key) || !(key <= curr->key)) {
	unlock(&pred->mtx);	unlock(&curr->mtx);
	continue;
      }

      assert ((pred->key < key) && (key <= curr->key));
      
      /* begin critical section */
      if (validate(pred, curr)) { 
	if (key == curr->key) {
	  curr->marked = true;
	  *val = curr->val;
	  pred->next = curr->next;
	  free_node(curr);
	} else {
	  ret = false;
	}
	/* end critical section */
	unlock(&pred->mtx);	unlock(&curr->mtx);
	break;
      }
      unlock(&pred->mtx);      unlock(&curr->mtx);
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
bool_t find(const list_t * list, const lkey_t key)
{

    node_t *curr;

    curr = list->head;
    while (curr->key < key) {
	curr = curr->next;
    }
    
    return (curr->key == key && !curr->marked);
}

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
  list->head->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
  
  if ((list->tail = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    goto end;
  }
  list->tail->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
  
  list->head->next = list->tail;
  list->tail->next = NULL;

  list->head->key = INT_MIN;
  list->tail->key = INT_MAX;

    return list;

 end:
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
      free (curr);
      curr = next;
    }
  
  free(list->head);
  free(list->tail);
  free(list);
}

void show_list(const list_t * list)
{
    node_t *pred, *curr;

    pred = list->head;
    curr = pred->next;

    printf ("list:\n\t");
    while (curr != list->tail) {
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
