/* ---------------------------------------------------------------------------
 * Lazy Skiplist
 * "A Simple Optimistic skip-list Algorithm" Maurice Herlihy, Yossi Lev, Victor Luchangco, Nir Shavit
 * http://www.cs.brown.edu/~levyossi/Pubs/LazySkipList.pdf
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.16
 * Copyright (C) 2009-2022  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <limits.h>
#include <assert.h>

#include "LazySkiplist.h"
#include "concurrent_skiplist.h"

static skiplist_node_t *create_node(const int, const lkey_t, const val_t);
static void free_node(skiplist_node_t *);
static bool_t _add(skiplist_t *, skiplist_node_t **, skiplist_node_t **, const lkey_t, const val_t);
static bool_t _delete(skiplist_t *, skiplist_node_t **, skiplist_node_t **, const lkey_t, val_t *);
static bool_t _find(skiplist_t *, skiplist_node_t **, skiplist_node_t **, const lkey_t);



#define lock(_mtx_)      pthread_mutex_lock(&(_mtx_))
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
static int search(const skiplist_t * sl, const lkey_t key, skiplist_node_t ** preds, 
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
#define node_size(level)	  (sizeof(skiplist_node_t) + ((level + 1) * sizeof(skiplist_node_t *)))

static skiplist_node_t *create_node(const int topLevel, const lkey_t key, const val_t val)
{
    skiplist_node_t *node;
#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
    pthread_mutexattr_t mtx_attr;
#endif

    if ((node = (skiplist_node_t *) calloc(1, node_size(topLevel))) == NULL) {
	elog("calloc error");
	return NULL;
    }

    node->key = key;
    node->val = val;
    node->topLevel = topLevel;

#ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
    node->mtx = (pthread_mutex_t) PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#else
    pthread_mutexattr_init(&mtx_attr);
#ifdef PTHREAD_MUTEX_RECURSIVE_NP
    pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_RECURSIVE_NP);
#else
    pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_RECURSIVE); /* for OSX */
#endif
    pthread_mutex_init(&node->mtx, &mtx_attr);
#endif

    node->marked = false;
    node->fullyLinked = false;

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
    int i;
    skiplist_t *sl;
    skiplist_node_t *head, *tail;

    if ((sl = (skiplist_t *) calloc(1, sizeof(skiplist_t))) == NULL) {
	elog("calloc error");
	return NULL;
    }

    sl->maxLevel = maxLevel;

    if ((head = create_node(maxLevel, min, min)) == NULL) {
	elog("create_node() error");
	goto end;
    }
    head->fullyLinked = true;

    if ((tail = create_node(maxLevel, max, max)) == NULL) {
	elog("create_node() error");
	goto end;
    }

    tail->fullyLinked = true;

    for (i = 0; i < maxLevel; i++) {
	head->next[i] = tail;
	tail->next[i] = NULL;
    }

    sl->head = head;
    sl->tail = tail;

    /* 
     * Initialize pthread_key_t workspace_key.
     */
    if (pthread_key_create(&sl->workspace_key, (void *) free_workspace) != 0) {
	elog("pthread_key_create() error");
	abort();
    }

    return sl;

  end:
    free(sl->head);
    free(sl);
    return NULL;
}

void free_list(skiplist_t * sl)
{
    free(sl->head);
    free(sl->tail);
    free(sl);
}


/*
 * bool_t _add(skiplist_t * sl, skiplist_node_t ** preds, skiplist_node_t ** succs,
 *                                                const lkey_t key, const val_t val)
 *
 * Add node'(key,val)' to skiplist sl. key should be unique.
 *
 * success : return true
 * failure(key already exists) : return false
 */
static bool_t
_add(skiplist_t * sl, skiplist_node_t ** preds, skiplist_node_t ** succs,
     const lkey_t key, const val_t val)
{
    int topLevel, highestLocked, lFound, level;
    skiplist_node_t *newNode, *pred, *succ, *nodeFound;
    bool_t valid;
    int r = rand();

    topLevel = (r % sl->maxLevel);
    assert(0 <= topLevel && topLevel < sl->maxLevel);

    /*
     * step 1: Search node
     */
    while (1) {
#ifdef _TEST_
      int _lFound, _level;
      skiplist_node_t *_pred, *_curr; 
      { /* search */
	_pred = sl->head;					     
	_lFound = -1;
	for (_level = sl->maxLevel - 1; _level >= 0; _level--) {
	  _curr = _pred->next[_level];
	  while (key > _curr->key) { 
	    _pred = _curr; 
	    _curr = _pred->next[_level]; 
	  }
	  if (_lFound == -1 && key == _curr->key)  
	    _lFound = _level;	
	  preds[_level] = _pred; succs[_level] = _curr;
	}
      }
	
      lFound = _lFound;
      if (lFound != -1) {
#else
      if ((lFound = search(sl, key, preds, succs)) != -1) {
#endif
	  nodeFound = succs[lFound];
	if (nodeFound->marked != true) {
	  while (nodeFound->fullyLinked != true) {};
	  return false;
	}
	continue;
      }
      
    /*
     * step 2: preds[]に含まれるノードのロック
     */
      highestLocked = -1;
      
      valid = true;
      for (level = 0; (valid == true) && (level <= topLevel); level++) {
	pred = preds[level];
	succ = succs[level];
	
	lock(pred->mtx);  /* levelによって複数回lockされる可能性がある -> RECURSIVE_MUTEX */
	
	highestLocked = level;
	valid = (pred->marked != true) && (succ->marked != true)
	  && (pred->next[level] == succ);
      }
      
      if (valid != true) {
	for (level = 0; level <= highestLocked; level++)
	  unlock(preds[level]->mtx);
	continue;
      }

      assert(valid == true);
      for (level = 0; (level <= topLevel); level++) {
	pred = preds[level];	succ = succs[level];
	assert(pred->marked != true);
	assert(pred->next[level] == succ);
      }

      /*
       * step 3: Add new node to skiplist
       */      
      newNode = create_node(topLevel, key, val);
      
      for (level = 0; level <= topLevel; level++) {
	newNode->next[level] = succs[level];
	preds[level]->next[level] = newNode;
      }
      
      newNode->fullyLinked = true;
      break;      
    }

    /*
     * step 4: Release lock
     */    
    assert(highestLocked == topLevel);
    for (level = 0; level <= topLevel; level++)
      unlock(preds[level]->mtx);
    
    return true;
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
  workspace_t *ws = get_workspace(sl); /* Return pointer to the workspace. (Each thread has one workspace.) */
  assert(ws != NULL);
  return _add(sl, ws->preds, ws->succs, key, val);
}

/*
 *bool_t _delete(skiplist_t * sl, skiplist_node_t ** preds, skiplist_node_t ** succs,
 *                                                       const lkey_t key, val_t * val)
 *
 * Delete node'(key, val)' from skiplist sl, and write the val to *getval.
 * 
 * success : return true
 * failure(not found) : return false
 */
static bool_t
_delete(skiplist_t * sl, skiplist_node_t ** preds, skiplist_node_t ** succs,
	const lkey_t key, val_t * val)
{
    int topLevel = -1;
    int i, lFound, level, highestLocked;
    skiplist_node_t *pred, *victim;
    bool_t isMarked = false;
    bool_t valid, flag;
    
    while (1) {
      /*
       * step 1: Search victim node.
       */
      victim = NULL;
#ifdef _TEST_
      int _lFound, _level;
      skiplist_node_t *_pred, *_curr; 
      { /* search */
	_pred = sl->head;					     
	_lFound = -1;
	for (_level = sl->maxLevel - 1; _level >= 0; _level--) {
	  _curr = _pred->next[_level];
	  while (key > _curr->key) { 
	    _pred = _curr; 
	    _curr = _pred->next[_level]; 
	  } 
	  if (_lFound == -1 && key == _curr->key)  
	    _lFound = _level;	
	  preds[_level] = _pred; succs[_level] = _curr;
	}
      }
      lFound = _lFound;
      if (lFound == -1)
#else
      if ((lFound = search(sl, key, preds, succs)) == -1) 
	/* There is no victim node. */
#endif
	return false;
      
      victim = succs[lFound];
      flag = ((victim->fullyLinked == true) && (victim->topLevel == lFound) && (victim->marked != true));

      /* Run only if the following conditions is satisfied or first time.
       * => victim->marked = true; isMarked = true; topLevel = victim->topLevel; 
       */
      if (isMarked == true || ((lFound != -1) && flag)) {
	if (isMarked != true) {
	  topLevel = victim->topLevel;
	  
	  lock(victim->mtx);
	  if (victim->marked == true) {
	    unlock(victim->mtx);
	    return false;
	  }
	  victim->marked = true;
	  isMarked = true;
	}
	
	/*
	 * step 2: Acquire locks of preds[]
	 */
	highestLocked = -1;
	valid = true;
	for (level = 0; (valid == true) && (level <= topLevel); level++) {
	  pred = preds[level];
	  lock(pred->mtx);
	  highestLocked = level;
	  valid = (pred->marked != true) && (pred->next[level] == victim);
	}
	if (valid != true) {
	  /* If lock of preds[] acquires fail, start over from the beginning. */
	  for (i = 0; i <= highestLocked; i++)	      unlock(preds[i]->mtx);
	  continue;
	}
	
	/*
	 * step 3: Delete nodes
	 */
	for (level = victim->topLevel; level >= 0; level--)
	  preds[level]->next[level] = victim->next[level];
	
	unlock(victim->mtx);
	for (i = 0; i <= lFound; i++) /* Release locks of preds[] */
	  unlock(preds[i]->mtx);
	break;
      }
      else
	return false;
    }
    
    *val = victim->val;
    free_node(victim);

    return true;
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
    workspace_t *ws = get_workspace(sl);
    assert(ws != NULL);
    return _delete(sl, ws->preds, ws->succs, key, val);
}

/*
 * bool_t _find(skiplist_t * sl, skiplist_node_t ** preds, skiplist_node_t ** succs, const lkey_t key)
 *
 * Find node'(key, val)' by the key from skiplist 'sl', and write the val to *getval.
 *
 * success : return true
 * failure(not found) : return false
 */
static bool_t _find(skiplist_t * sl, skiplist_node_t ** preds,
		    skiplist_node_t ** succs, const lkey_t key)
{
  int lFound;

#ifdef _TEST_
      int _lFound, _level;
      skiplist_node_t *_pred, *_curr; 
      { /* search */
	_pred = sl->head;					     
	_lFound = -1;
	  for (_level = sl->maxLevel - 1; _level >= 0; _level--) {
	    _curr = _pred->next[_level];
	    while (key > _curr->key) { 
	      _pred = _curr; 
	      _curr = _pred->next[_level]; 
	    } 
	    if (_lFound == -1 && key == _curr->key)  
	      _lFound = _level;	
	    preds[_level] = _pred; succs[_level] = _curr;
	  }
      }
      lFound = _lFound;
#else
    lFound = search(sl, key, preds, succs);
#endif
    if (lFound == -1)
	return (val_t) NULL;
    if (succs[lFound]->fullyLinked != true || succs[lFound]->marked == true)
	return (val_t) NULL;

    return preds[0]->next[0]->val;
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
    workspace_t *ws = get_workspace(sl);
    assert(ws != NULL);
    return _find(sl, ws->preds, ws->succs, key);
}


void show_list(skiplist_t * sl)
{
    int i;
    skiplist_node_t *n;

    for (i = sl->maxLevel - 1; i >= 0; i--) {
	printf("level %2d: ", i);

	n = sl->head;
	printf("%d ", (int) n->key);

	n = sl->head->next[i];
	while (n != NULL) {
	    printf(" -> %d", (int) n->key);
	    n = n->next[i];
	}
	printf(" -|\n");
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


    nums = malloc(sizeof *nums * t);
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
    //    free_skiplist(sl);

    return 0;
}

#endif
