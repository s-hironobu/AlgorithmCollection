/* ---------------------------------------------------------------------------
 * Lock-Free list
 *
 * "Lock-Free Linked Lists and Skip Lists" Mikhail Fomitchev, Eric Ruppert
 * http://www.cse.yorku.ca/~ruppert/papers/lfll.pdf
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.16
 * Copyright (C) 2009-2023  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>

#include "LockFreeList.h"

static node_t *create_node(const lkey_t, const val_t);
static void helpFlagged (node_t *, node_t *);


static inline bool_t
#ifdef _X86_64_
cas(next_ref *addr, const next_ref oldp, const next_ref newp)
{
  char result;
  __asm__ __volatile__("lock; cmpxchg16b %0; setz %1;":"=m"(*(volatile next_ref *)addr),
		       "=q"(result)
		       :"m"(*(volatile next_ref *)addr), "a"(oldp.mark), "d"(oldp.node_ptr),
			"b"(newp.mark), "c"(newp.node_ptr)
		       :"memory");
  return (((int)result == 0) ? false:true);
}
#else
cas(next_ref *addr, const next_ref oldp, const next_ref newp)
{
  char result;
  __asm__ __volatile__("lock; cmpxchg8b %0; setz %1;":"=m"(*addr),
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

    node->succ.mark = (intptr_t)(UNMARKED | UNFLAGGED);
    node->succ.node_ptr = NULL;

    node->backlink = NULL;
    node->key = (lkey_t)key;
    node->val = (val_t)val;

    return node;
}

#define is_marked_ref(ref)      ((intptr_t)(ref.mark & MARKED_MASK) == (intptr_t)MARKED ? true:false)
#define is_unmarked_ref(ref)    ((intptr_t)(ref.mark & MARKED_MASK) == (intptr_t)UNMARKED ? true:false)
#define is_flagged_ref(ref)     ((intptr_t)(ref.mark & FLAGGED_MASK) == (intptr_t)FLAGGED ? true:false)
#define is_unflagged_ref(ref)   ((intptr_t)(ref.mark & FLAGGED_MASK) == (intptr_t)UNFLAGGED ? true:false)


static next_ref make_ref(const node_t * node_ptr, const intptr_t marked, const intptr_t flagged)
{
    next_ref ref;
    assert(!(marked == MARKED && flagged == FLAGGED));
    ref.mark = (marked | flagged);
    ref.node_ptr = (node_t *) node_ptr;
    return ref;
}

#define get_unmarked_ref(node_ptr)  make_ref (node_ptr, UNMARKED, UNFLAGGED)
#define get_marked_ref(node_ptr)  make_ref (node_ptr, MARKED, UNFLAGGED)

static void
helpMarked (node_t *prev_node, node_t *del_node)
{
  node_t *next_node;
  assert (del_node->succ.node_ptr != NULL);
  next_node = del_node->succ.node_ptr;
  
  cas(&prev_node->succ, 
      make_ref(del_node, UNMARKED, FLAGGED), 
      make_ref(next_node, UNMARKED, UNFLAGGED));
}

static bool_t searchFrom2 (const lkey_t key, node_t *curr_node, node_t **curr, node_t **next)
{
  node_t *next_node = curr_node->succ.node_ptr;
  
  while (next_node->key < key) {
    while (is_marked_ref(next_node->succ)
	   && (is_unmarked_ref(curr_node->succ) || (curr_node->succ.node_ptr != next_node))
	   ) {

      if (curr_node->succ.node_ptr == next_node)
	helpMarked(curr_node, next_node);
      next_node = curr_node->succ.node_ptr;
    }

    if (next_node->key < key) {
      curr_node = next_node;
      next_node = curr_node->succ.node_ptr;
    }    
  }

  *curr = curr_node;
  *next = next_node;

  return true;
}

static node_t *search(list_t *list, const lkey_t key)
{
  node_t *curr, *next;
  if (searchFrom2(key, list->head, &curr, &next) != true)
    return NULL;
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

bool_t add(list_t * list, const lkey_t key, const val_t val)
{
    node_t *newNode;
    
    node_t *prev_node, *next_node;
    next_ref prev_succ, result;
    
    if (searchFrom2(key, list->head, &prev_node, &next_node) != true)
      return false;
    
    if (prev_node->key == key)
      return false;
    
    if ((newNode = create_node(key, val)) == NULL)
      return false;
    
    while (1) {
      prev_succ = prev_node->succ;
      
      if ((prev_succ.mark & FLAGGED_MASK) == FLAGGED) {
	helpFlagged(prev_node, prev_succ.node_ptr);
      }
      else {
	newNode->succ = make_ref(next_node, UNMARKED, UNFLAGGED);
	if (cas(&prev_node->succ, make_ref(next_node, UNMARKED, UNFLAGGED), 
		make_ref(newNode, UNMARKED, UNFLAGGED)) == true) {
	  return true;
	}
	else {
	  result = prev_node->succ;

	  if (is_unmarked_ref(result) && is_flagged_ref(result)) {
	    helpFlagged(prev_node, result.node_ptr);
	  }
	  while (is_marked_ref(prev_node->succ)) {
	    prev_node = prev_node->backlink;
	  }
	}
      }
      
      searchFrom2(key, list->head, &prev_node, &next_node);
      
      if (prev_node->key == key) {
	free (newNode);
	return false;
      }
    }
}


static bool_t
tryFlag (node_t *prev_node, node_t *target_node, node_t **result_node)
{
  bool_t ret = true;
  node_t *del_node;
  next_ref result;

  *result_node = NULL;

  while (1) {

    if (prev_node->succ.node_ptr == target_node
	&& (is_unmarked_ref(prev_node->succ) && is_flagged_ref(prev_node->succ))) {
      *result_node = prev_node;
      return false;
    }

    if (cas(&prev_node->succ, 
	      make_ref(target_node, UNMARKED, UNFLAGGED),
	      make_ref(target_node, UNMARKED, FLAGGED)) == true) {
      *result_node = prev_node;
      return true;
    }
    result = prev_node->succ;
      
    if ((result.node_ptr == target_node) && (is_unmarked_ref(result)) && (is_flagged_ref(result))) {
      *result_node = prev_node;
      return false;
    }

    while (is_marked_ref(prev_node->succ)) {
      prev_node = prev_node->backlink;
    }
  }

  searchFrom2(target_node->key, prev_node, &prev_node, &del_node);

  if (del_node != target_node) {
    ret = false;
    *result_node = NULL;
  }
    
  return ret;
}


static void
tryMark(node_t *del_node) 
{
  node_t *next_node;
  next_ref result;
  do {
    next_node = del_node->succ.node_ptr;
    
    cas(&del_node->succ, make_ref(next_node, UNMARKED, UNFLAGGED),
	  make_ref(next_node, MARKED, UNFLAGGED));

    result = del_node->succ;
    if (is_unmarked_ref(result) && is_flagged_ref(result)) {
      helpFlagged(del_node, result.node_ptr);
    }
  } while (is_unmarked_ref(del_node->succ));
}

static void
helpFlagged (node_t *prev_node, node_t *del_node)
{
  del_node->backlink = prev_node;

  if (is_unmarked_ref(del_node->succ)) {
    tryMark(del_node);
  }
  helpMarked(prev_node, del_node);
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
  node_t *prev_node, *del_node;
  node_t *result_node;
  bool_t result;

  searchFrom2(key, list->head, &prev_node, &del_node);

  if (del_node->key != key)
    return false;
  
  result = tryFlag(prev_node, del_node, &result_node);

  if (result_node != NULL) {
    helpFlagged(result_node, del_node);
  }

  if (result == false)
    return false;

  *val = del_node->val;
  free_node(del_node);
    
  return true;
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

    curr = search(l, key);
    if ((curr == l->tail) || (curr->key != key))
	return false;

    return true;
}


void show_list(const list_t * list)
{
    node_t *pred, *curr;

    pred = list->head;
    curr = (node_t *)pred->succ.node_ptr;

    printf ("list:\n\t");
    while (curr != list->tail) {
#ifdef _DEBUG_
      printf(" [%d]", (int) curr->key);
#else
      printf(" [%lu:%ld]", (unsigned long int) curr->key, (long int)curr->val);
#endif
      pred = curr;
      curr = (node_t *)curr->succ.node_ptr;
    }
    printf("\n");
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

  list->head->key = INT_MIN;
  list->head->succ.mark = UNMARKED;
  list->head->succ.node_ptr = (void *)list->tail;

  list->tail->key = INT_MAX;
  list->tail->succ.mark = UNMARKED;
  list->tail->succ.node_ptr = NULL;

  return list;

 end:
  free (list->head);
  free (list);
  return NULL;
}

void free_list(list_t * list)
{
  node_t *curr, *next;

  curr = (node_t *)list->head->succ.node_ptr;

  while (curr != list->tail) {
    next = (node_t *)curr->succ.node_ptr;
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
      add (list, (lkey_t)i, (val_t) i);
      show_list (list);
    }

  for (i = 0; i < 10; i++) {
    delete (list, (lkey_t)i, &g);
    printf ("ret = %ld\n", (long int)g);
    show_list (list);
  }


  free_list (list);

  return 0;
}

#endif
