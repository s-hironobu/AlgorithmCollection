/* ---------------------------------------------------------------------------
 * 
 * Lock-Free List : "A Pragmatic Implementation of Non-Blocking Linked-Lists" Timothy L. Harris 
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.02
 * Copyright (C) 2009-2014  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>

#include "NonBlockingList.h"

static node_t *create_node(const lkey_t, const val_t);
static node_t *search(list_t *, const lkey_t, node_t **);
static next_ref make_ref(const node_t *, const node_stat);


static inline bool_t
#ifdef _X86_64_
cas(next_ref * addr, const next_ref oldp, const next_ref newp)
{
  char result;
  __asm__ __volatile__("lock; cmpxchg16b %0; setz %1":"=m"(*addr),
		       "=q"(result)
		       :"m"(*addr), "a"(oldp.mark), "d"(oldp.node_ptr),
			"b"(newp.mark), "c"(newp.node_ptr)
		       :"memory");
  return (((int)result == 0) ? false:true);
}
#else
cas(next_ref * addr, const next_ref oldp, const next_ref newp)
{
  char result;
  __asm__ __volatile__("lock; cmpxchg8b %0; setz %1":"=m"(*addr),
		       "=q"(result)
		       :"m"(*addr), "a"(oldp.mark), "d"(oldp.node_ptr),
			"b"(newp.mark), "c"(newp.node_ptr)
		       :"memory");
  return (((int)result == 0) ? false:true);
}
#endif


#ifdef _FREE_
#define free_node(_node_) free(_node_)
#else
#define free_node(_node_) ;
#endif


#define is_marked_ref(ref)   (ref.mark == MARKED ? true:false)
#define is_unmarked_ref(ref) (ref.mark == UNMARKED ? true:false)

#define get_ptr(ref)          (ref.node_ptr)

static next_ref make_ref(const node_t * node_ptr, const node_stat mark)
{
    next_ref ref;
    ref.mark = mark;
    ref.node_ptr = (node_t *) node_ptr;
    return ref;
}


#define get_unmarked_ref(ptr)  make_ref(ptr, UNMARKED)
#define get_marked_ref(ptr)    make_ref(ptr, MARKED)

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

  if ((node = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    return NULL;
  }

  node->key = key;
  node->val = val;

  return node;
}

/*
 * node_t *search(list_t * list, const lkey_t key, node_t **pred)
 *
 *
 */
static node_t *search(list_t * list, const lkey_t key, node_t **pred)
{
    node_t *pred_next = NULL;
    node_t *curr;

 search_again:
    do {
      node_t *t = list->head;
      node_t *t_next = (node_t *)get_ptr(list->head->next);

      /* step 1: find pred and curr */
      do {
	if (!is_marked_ref(t_next->next)) {
	  (*pred) = t;
	  pred_next = t_next;
	}

	t = t_next;	    t->next.mark = (intptr_t)UNMARKED;
	
	if (t == list->tail)
	  break;
	t_next = (node_t *)get_ptr(t->next);
      } while (t->key < key || is_marked_ref(t_next->next));
      assert(key <= t->key || is_unmarked_ref(t_next->next));

      curr = t;

      /* step 2: check nodes are adjacent */
      if (pred_next == curr) {
	if ((curr != list->tail) && (is_marked_ref(curr->next))) {
	  goto search_again;
	} else {	    
	  assert (pred_next == curr);
	  return curr;
	}
      }

      /* step 3: remove one or more marked nodes */
      if (cas(&(*pred)->next, make_ref(pred_next, MARKED), make_ref(curr, UNMARKED)) == true) {
	if (curr != list->tail && is_marked_ref(curr->next)) {
	  goto search_again;
	} else {
	  assert (get_ptr((*pred)->next) == curr);
	  return curr;
	  goto end; // dummy
	}
      }
    }
    while (1);
    
 end:
    assert (get_ptr((*pred)->next) == curr);

    return curr;
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
    node_t *pred = NULL;
    node_t *curr;
    node_t *newNode;

    if ((newNode = create_node(key, val)) == NULL)
      return false;

    do {
      curr = search(list, key, &pred);
      assert(pred->key < key && key <= curr->key);

      if ((curr != list->tail) && (curr->key == key)) {
	free_node(newNode);
	return false;
      }

      newNode->next = make_ref(curr, UNMARKED);
      if (cas(&pred->next, make_ref(curr, UNMARKED), make_ref(newNode, UNMARKED)) == true) {
      	break;
      }
    }
    while (1);

    return true;
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
    node_t *pred = NULL;
    node_t *curr, *curr_next;
    bool_t ret = true;

    do {
      curr = search(list, key, &pred);

      assert(pred->key < key && key <= curr->key);

      if ((curr == list->tail) || (curr->key != key)) {
#ifdef _DEBUG_
	if (curr->key != key)
	  printf ("delete ERROR: key = %ld curr->key %ld\n", (long int)key, (long int)curr->key);
#endif
	return false;
      }

      curr_next = (node_t *)get_ptr(curr->next);

      if (is_unmarked_ref(curr_next->next)) {
	node_t *node_ptr = (node_t *)get_ptr(curr->next);
	if (cas(&(curr->next), make_ref(node_ptr, curr->next.mark), get_marked_ref(node_ptr)) == true)
	  break;
      }      
    }
    while (1);
    if (cas(&(pred->next), get_unmarked_ref(curr), get_unmarked_ref(curr_next)) != true) {
      curr = search(list, curr->key, &pred);
    }

    *val = curr->val;
    free_node(curr);
    
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
bool find(list_t * l, const lkey_t key)
{
    node_t *curr;
    node_t *pred = NULL;

    curr = search(l, key, &pred);
    if ((curr == l->tail) || (curr->key != key))
	return false;

    return true;
}


void show_list(const list_t * list)
{
    node_t *pred, *curr;

    pred = list->head;
    curr = (node_t *)get_ptr(pred->next);

    printf ("list:\n\t");
    while (curr != list->tail) {
#ifdef _DEBUG_
      printf(" [%d]", (intptr_t) curr->key);
#else
      printf(" [%lu:%lu]", (unsigned long) curr->key, (long)curr->val);
#endif
      pred = curr;
      curr = (node_t *)get_ptr(curr->next);
    }
    printf("\n");
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
    goto end;
  }
  
  if ((list->tail = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    goto end;
  }

  list->head->key = INT_MIN;
  list->head->next = make_ref(list->tail, UNMARKED);

  list->tail->key = INT_MAX;
  list->tail->next = make_ref(NULL, UNMARKED);

  return list;

 end:
  free (list->head);
  free (list);
  return NULL;
}

void free_list(list_t * list)
{
  node_t *curr, *next;

  curr = (node_t *)get_ptr(list->head->next);

  while (curr != list->tail) {
    next = (node_t *)get_ptr(curr->next);

    free_node (curr);
    curr = next;
  }

  free_node(list->head);
  free_node(list->tail);
  free(list);
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
    printf ("ret = %lu\n", (unsigned long)g);
  }

  show_list (list);

  free_list (list);

  return 0;
}

#endif
