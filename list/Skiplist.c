/* ---------------------------------------------------------------------------
 * Skiplist
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.01
 * Copyright (C) 2009-2024  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>

#include "Skiplist.h"

static int search(skiplist_t *, const lkey_t, skiplist_node_t **, skiplist_node_t **);
static skiplist_node_t *create_node(const int, const lkey_t, const val_t);
static void free_node(skiplist_node_t *);


#define lock(_mtx_)    pthread_mutex_lock(&(_mtx_))
#define unlock(_mtx_)    pthread_mutex_unlock(&(_mtx_))

/*
 *static int search(skiplist_t * sl, const lkey_t key, skiplist_node_t ** preds,
 *                                                     skiplist_node_t ** succs)
 *
 * Find node whose key is 'key'.
 *
 * Not found: return -1
 * Found node: return the level of the node. And create preds and succs (see below for details).
 *
 *  Example: Find key=4.
 *
 *level: head                                 tail
 *  3   [-inf]------>[2]--------------------->[inf]
 *  2   [-inf]------>[2]------>[4]------>[7]->[inf]
 *  1   [-inf]->[1]->[2]------>[4]->[5]->[7]->[inf]
 *  0   [-inf]->[1]->[2]->[3]->[4]->[5]->[7]->[inf]
 *   
 * 　Return the level of the node, in this case, return 2. 
 *   preds and succs are below:
 *    level: preds succs
 *     [2]     2     7
 *     [1]     2     5
 *     [0]     3     5
 *
 *   "preds[2]=2" means that "the node whose key is 2 refers to the node whose key is 4 at level 2".
 *   "preds[1]=2" means that "the node whose key is 2 refers to the node whose key is 4 at level 1".
 *   "preds[0]=3" means that "the node whose key is 3 refers to the node whose key is 4 at level 0".
 *
 *   "succs[2]=7" means that "the node whose key is 4 refers to the node whose key is 7 at level 2".
 *   "succs[1]=5" means that "the node whose key is 4 refers to the node whose key is 5 at level 1".
 *   "succs[0]=5" means that "the node whose key is 4 refers to the node whose key is 5 at level 0".
 */
static int search(skiplist_t * sl, const lkey_t key, skiplist_node_t ** preds,
		  skiplist_node_t ** succs)
{
    int lFound, level;
    skiplist_node_t *pred, *curr;

    pred = sl->head;
    lFound = -1;

    for (level = sl->maxLevel - 1; level >= 0; level--) {
      curr = pred->next[level];

      while (key > curr->key) {
	pred = curr;
	curr = pred->next[level];
      }
      
      if (lFound == -1 && key == curr->key) 
	    lFound = level;
      
      preds[level] = pred;
      succs[level] = curr;
    }

    return lFound;
}


/*
 * static skiplist_node_t *create_node(const int topLevel, const lkey_t key, const val_t val)
 *
 * Create a node '(key, val)' whose level is 'topLevel'.
 *
 * success : return pointer to this node
 * failure : return NULL
 */
#define node_size(level)	  (sizeof(skiplist_node_t) + (level * sizeof(skiplist_node_t *)))

static skiplist_node_t *create_node(const int topLevel, const lkey_t key, const val_t val)
{
    skiplist_node_t *node;

    if ((node = (skiplist_node_t *) calloc(1, node_size(topLevel))) == NULL) {
	elog("calloc error");
	return NULL;
    }

    node->key = key;
    node->val = val;
    node->topLevel = topLevel;

    return node;
}

static void free_node(skiplist_node_t * node)
{
#ifdef _FREE_
    free(node);
#endif
}


/*
 * skiplist_t *init_list(const int maxLevel, const lkey_t min, const lkey_t max)
 *
 * Create skiplist. 
 * maxLevel is the max hight of skiplist.
 * min is the minimum key's value (write to skiplist->head->key)
 * max is the maximum key's value (write to skiplist->tail->key)
 *
 * success : return pointer to this skiplist
 * failure : return NULL
 */
skiplist_t *init_list(const int maxLevel, const lkey_t min, const lkey_t max)
{
    skiplist_t *sl;
    skiplist_node_t *head;
    skiplist_node_t *tail = NULL;
    int i;

    if ((sl = (skiplist_t *) calloc(1, sizeof(skiplist_t))) == NULL) {
	elog("calloc error");
	return NULL;
    }

    sl->mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    sl->maxLevel = maxLevel;

    if ((head = create_node(maxLevel, min, (val_t)NULL)) == NULL) {
      elog("create_node() error");
      goto end;
    }
    if ((tail = create_node(maxLevel, max, (val_t)NULL)) == NULL) {
      elog("create_node() error");
      goto end;
    }

    for (i = 0; i < maxLevel; i++) {
	head->next[i] = tail;
	tail->next[i] = NULL;
    }

    sl->head = head;
    sl->tail = tail;

    if ((sl->preds = (skiplist_node_t **) calloc(maxLevel, sizeof(skiplist_node_t *))) == NULL) {
	elog("calloc error");
	goto end;
    }
    if ((sl->succs = (skiplist_node_t **) calloc(maxLevel, sizeof(skiplist_node_t *))) == NULL) {
	elog("calloc error");
	goto end;
    }
    return sl;

 end:
    free(head);
    free(tail);
    free(sl->preds);
    free(sl);
    return NULL;
}


void free_list(skiplist_t * sl)
{
  pthread_mutex_destroy(&sl->mtx);
  free_node(sl->tail);
  free_node(sl->head);
  free_node((void *)sl->preds);
  free_node((void *)sl->succs);
  free(sl);
}


/*
 * bool_t add(skiplist_t * sl, const lkey_t key, const val_t val)
 *
 * Add node'(key,val)' to skiplist sl. key should be unique.
 *
 * success : return true
 * failure(key already exists) : return false
 */
bool_t add(skiplist_t * sl, const lkey_t key, const val_t val)
{
    int level, topLevel, lFound;
    skiplist_node_t *newNode;
    int r = rand();
    bool_t ret = true;

    lock(sl->mtx);

    if ((lFound = search(sl, key, sl->preds, sl->succs)) != -1) {
      ret = false;
    } else {      
      topLevel = (r % sl->maxLevel);
      assert(0 <= topLevel && topLevel < sl->maxLevel);

      newNode = create_node(topLevel, key, val);
      
      for (level = 0; level <= topLevel; level++) {
	newNode->next[level] = sl->succs[level];
	sl->preds[level]->next[level] = newNode;
      }
    }

    unlock(sl->mtx);

    return ret;
}


/*
 * bool_t delete(skiplist_t * sl, const lkey_t key, val_t * val)
 *
 * Delete node'(key, val)' from skiplist sl, and write the val to *getval.
 * 
 * success : return true
 * failure(not found) : return false
 */
bool_t delete(skiplist_t * sl, const lkey_t key, val_t * val)
{
    int lFound, level;
    skiplist_node_t *victim = NULL;
    bool_t ret = true;

    lock(sl->mtx);

    if ((lFound = search(sl, key, sl->preds, sl->succs)) == -1) {
      ret = false;
    } else {

      victim = sl->succs[lFound];
      for (level = victim->topLevel; level >= 0; level--)
	sl->preds[level]->next[level] = victim->next[level];
      
      *val = victim->val;
      free_node(victim);
    }

    unlock(sl->mtx);

    return ret;
}


/*
 * val_t find(skiplist_t * sl, const lkey_t key)
 *
 * Find node'(key, val)' by the key from skiplist 'sl', and write the val to *getval.
 *
 * success : return true
 * failure(not found) : return false
 */
val_t find(skiplist_t * sl, const lkey_t key)
{
    int lFound;
    val_t ret;

    lock(sl->mtx);

    if ((lFound = search(sl, key, sl->preds, sl->succs)) == -1)
      ret = (val_t)NULL;
    else
      ret = sl->preds[0]->next[0]->val;
    
    unlock(sl->mtx);

    return ret;
}


void show_list(skiplist_t * sl)
{
    int i;
    skiplist_node_t *n;

    for (i = sl->maxLevel - 1; i >= 0; i--) {
      printf("\tlevel %2d: ", i);
      
      n = sl->head;
      
      n = sl->head->next[0];
      while (n != NULL) {
	if (n == sl->tail) {
	  n = n->next[0];
	  continue;
	}

	if (i <= n->topLevel) {
	  printf(" [%5d]", (int) n->key);
	}
	else {
	  printf("        ");
	}
	n = n->next[0];
      }
      printf("\n");
    }
}


#ifdef _SINGLE_THREAD_

int main(int argc, char **argv)
{
    int i;
    skiplist_t *sl;

    int *nums;
    int t = 10;
    val_t gval;

    nums = calloc(1, sizeof *nums * t);
    sl = init_list(4, LONG_MIN, LONG_MAX);

    for (i = 1; i < t; i++) {
	add(sl, (nums[i] = i), i);
	printf("add %d\n", nums[i]);
	show_list(sl);
    }

    show_list(sl);

    for (i = t - 1; 1 <= i; i--) {
	if (!delete(sl, nums[i], &gval))
	    printf("failed to remove %d.\n", nums[i]);
	else
	    printf("delete %d\n", nums[i]);
	show_list(sl);
    }

    free(nums);
    free_list(sl);
    exit(0);

    return 0;
}

#endif
