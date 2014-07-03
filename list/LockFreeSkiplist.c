/* ---------------------------------------------------------------------------
 * Lock-Free Skiplist
 *
 * "A Lock-Free concurrent skiplist with wait-free search" by Maurice Herlihy & Nir Shavit
 *  http://www.cs.brown.edu/courses/csci1760/ch14.ppt
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.01
 * Copyright (C) 2009-2014  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>

#include "LockFreeSkiplist.h"
#include "concurrent_skiplist.h"

static inline bool_t 
#ifdef _X86_64_
cas(volatile tower_ref * addr, const tower_ref oldp,
		  const tower_ref newp)
{
    char result;
    __asm__ __volatile__("lock; cmpxchg16b %0; setz %1":"=m"(*(volatile tower_ref *)addr),
			 "=q"(result)
			 :"m"(*(volatile tower_ref *)addr), "a"(oldp.mark),
			 "d"(oldp.next_node_ptr), "b"(newp.mark),
			 "c"(newp.next_node_ptr)
			 :"memory");

    return (((int) result != 0) ? true : false);
}
#else
cas(volatile tower_ref * addr, const tower_ref oldp,
		  const tower_ref newp)
{
    char result;
    __asm__ __volatile__("lock; cmpxchg8b %0; setz %1":"=m"(*(volatile tower_ref *)addr),
			 "=q"(result)
			 :"m"(*(volatile tower_ref *)addr), "a"(oldp.mark),
			 "d"(oldp.next_node_ptr), "b"(newp.mark),
			 "c"(newp.next_node_ptr)
			 :"memory");

    return (((int) result != 0) ? true : false);
}
#endif

static tower_ref make_ref(const skiplist_node_t * next_node_ptr,
			  const node_stat mark)
{
    tower_ref ref;
    ref.mark = mark;
    ref.next_node_ptr = (skiplist_node_t *) next_node_ptr;
    return ref;
}


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
 *   "preds[2]=2" means that "the node whose key is 2 pointed to the node whose key is 4 at level 2".
 *   "preds[1]=2" means that "the node whose key is 2 pointed to the node whose key is 4 at level 1".
 *   "preds[0]=3" means that "the node whose key is 3 pointed to the node whose key is 4 at level 0".
 *
 *   "succs[2]=7" means that "the node whose key is 4 pointed to the node whose key is 7 at level 2".
 *   "succs[1]=5" means that "the node whose key is 4 pointed to the node whose key is 5 at level 1".
 *   "succs[0]=5" means that "the node whose key is 4 pointed to the node whose key is 5 at level 0".
 */
static bool_t search(skiplist_t * sl, const lkey_t key, skiplist_node_t ** preds,
		 skiplist_node_t ** succs)
{
    int level;
    skiplist_node_t *pred, *curr, *succ;
    bool_t snip;
    node_stat marked;

  retry:
    while (1) {
	pred = sl->head;

	for (level = sl->maxLevel - 1; level >= 0; level--) {
	    curr = pred->tower[level].next_node_ptr;

	    while (1) {
		succ = curr->tower[level].next_node_ptr;
		marked = curr->tower[level].mark;

		while (marked == MARKED) {
		    snip =
			cas(&(*pred).tower[level],
			      make_ref(curr, UNMARKED), make_ref(succ,
								 UNMARKED));

		    if (snip != true)
			goto retry;

		    curr = pred->tower[level].next_node_ptr;
		    succ = curr->tower[level].next_node_ptr;
		    marked = curr->tower[level].mark;
		}

		if (key > curr->key) {
		    pred = curr;
		    curr = succ;
		} else
		    break;
	    }			// end while(1)

	    preds[level] = pred;
	    succs[level] = curr;
	}			// end for()

	return (curr->key == key ? true : false);
    }				// end while(1)

    return true;		// dummy
}


/*
 * skiplist_node_t *create_node(const int topLevel, const lkey_t key, const val_t val)
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
    int level;

    if ((node = (skiplist_node_t *) calloc(1, node_size(topLevel))) == NULL) {
      elog("calloc error");
      return NULL;
    }

    node->key = key;
    node->val = val;
    node->topLevel = topLevel;

    if ((node->tower =
	 (tower_ref *) calloc(1, sizeof(tower_ref) * (topLevel + 1))) ==
	NULL) {
      elog("calloc error");
	free(node);
	return NULL;
    }
    for (level = 0; level < topLevel; level++) {
	node->tower[level].next_node_ptr = NULL;
	node->tower[level].mark = UNMARKED;
    }

    return node;
}

static void free_node(skiplist_node_t * node)
{
#ifdef _FREE_
      free(node->tower);
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

    if ((tail = create_node(maxLevel, max, max)) == NULL) {
      elog("create_node() error");
      goto end;
    }

    for (i = 0; i < maxLevel; i++) {
	head->tower[i].next_node_ptr = tail;
	tail->tower[i].next_node_ptr = NULL;
    }

    sl->head = head;
    sl->tail = tail;

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


void free_skiplist(skiplist_t * sl)
{
    free_node(sl->head);
    free_node(sl->tail);
    free(sl);
}


static bool_t _add(skiplist_t * sl, skiplist_node_t ** preds,
	 skiplist_node_t ** succs, const lkey_t key, const val_t val)
{
    int level, topLevel;
    int bottomLevel = 0;
    skiplist_node_t *pred, *succ, *newNode;
    int r = rand();

    topLevel = (r % sl->maxLevel);
    assert(0 <= topLevel && topLevel < sl->maxLevel);

    while (1) {
	if (search(sl, key, preds, succs) == true)
	    return false;

	newNode = create_node(topLevel, key, val);

	for (level = bottomLevel; level <= topLevel; level++) {
	    newNode->tower[level].next_node_ptr = succs[level];
	    newNode->tower[level].mark = UNMARKED;
	}

	pred = preds[bottomLevel];
	succ = succs[bottomLevel];
	newNode->tower[bottomLevel].next_node_ptr = succ;
	newNode->tower[bottomLevel].mark = UNMARKED;

	if (cas
	    (&(*pred).tower[bottomLevel], make_ref(succ, UNMARKED),
	     make_ref(newNode, UNMARKED))
	    == false)
	    continue;

	for (level = bottomLevel + 1; level <= topLevel; level++) {
	    while (1) {
		pred = preds[level];
		succ = succs[level];
		if (cas
		    (&(*pred).tower[level], make_ref(succ, UNMARKED),
		     make_ref(newNode, UNMARKED))
		    == true)
		    break;
		search(sl, key, preds, succs);
	    }
	}
	return true;
    }
    return true;		// dummy
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
    workspace_t *ws = get_workspace(sl);
    assert(ws != NULL);
    return _add(sl, ws->preds, ws->succs, key, val);
}



static bool_t _delete(skiplist_t * sl, skiplist_node_t ** preds,
	    skiplist_node_t ** succs, const lkey_t key, val_t * val)
{
    int level;
    int bottomLevel = 0;
    skiplist_node_t *succ, *victim;
    node_stat marked;

    while (1) {
	if (search(sl, key, preds, succs) == false)
	    return false;

	victim = succs[bottomLevel];

	for (level = victim->topLevel; level >= bottomLevel + 1; level--) {
	    succ = victim->tower[level].next_node_ptr;
	    marked = victim->tower[level].mark;

	    while (marked == UNMARKED) {
		victim->tower[level].next_node_ptr = succ;
		victim->tower[level].mark = MARKED;

		succ = victim->tower[level].next_node_ptr;
		marked = victim->tower[level].mark;
	    }
	}			// end for()

	succ = victim->tower[bottomLevel].next_node_ptr;
	marked = victim->tower[bottomLevel].mark;

	while (1) {
	    bool_t iMarkedIt;
	    iMarkedIt =
		cas(&(*victim).tower[bottomLevel],
		      make_ref(succ, UNMARKED), make_ref(succ, MARKED));

	    succ = succs[bottomLevel]->tower[bottomLevel].next_node_ptr;
	    marked = succs[bottomLevel]->tower[bottomLevel].mark;

	    if (iMarkedIt == true) {
		search(sl, key, preds, succs);
		*val = victim->val;
		free_node(victim);
		return true;
	    } else if (marked == MARKED)
		return false;
	}

    }				// end while()
    return true;		// dummy
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


static bool_t _find(skiplist_t * sl, skiplist_node_t ** preds,
	      skiplist_node_t ** succs, const lkey_t key)
{
    int level;
    int bottomLevel = 0;
    skiplist_node_t *pred, *curr, *succ;
    node_stat marked = false;


    pred = sl->head;
    curr = NULL;
    succ = NULL;

    for (level = sl->maxLevel; level >= bottomLevel; level--) {
	curr = pred->tower[level].next_node_ptr;

	while (1) {
	    succ = curr->tower[level].next_node_ptr;
	    marked = curr->tower[level].mark;

	    while (marked == MARKED) {
		curr = pred->tower[level].next_node_ptr;
		succ = curr->tower[level].next_node_ptr;
		marked = curr->tower[level].mark;
	    }

	    if (curr->key < key) {
		pred = curr;
		curr = succ;
	    } else
		break;
	}			// end while(1)
    }

    return (curr->key == key ? true : false);
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


void free_list(skiplist_t * sl)
{
    free_node(sl->tail);
    free_node(sl->head);
    free(sl);
}


void show_list(skiplist_t * sl)
{
    int i;
    skiplist_node_t *n;

    for (i = sl->maxLevel - 1; i >= 0; i--) {
      printf("level %2d: ", i);
      
      n = sl->head;
      
      n = sl->head->tower[0].next_node_ptr;
      while (n != NULL) {
	if (n == sl->tail) {
	  n = n->tower[0].next_node_ptr;
	  continue;
	}
	
	if (i <= n->topLevel && n->tower[i].mark == UNMARKED) {
	  printf(" [%5d]", (int) n->key);
	}
	else {
	  printf("        ");
	}
	
	n = n->tower[0].next_node_ptr;
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


    nums = malloc(sizeof *nums * t);
    sl = init_list(4, INT_MIN, INT_MAX);

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
    free_skiplist(sl);

    return 0;
}

#endif
