/* ---------------------------------------------------------------------------
 * UnBounded LockFree Queue
 *
 * "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms" by M. Michael and M. Scott
 * http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.03
 * Copyright (C) 2009-2024  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>

#include "CASLockFreeQueue.h"

static node_t *create_node(const val_t);
static void free_node(node_t *);


static inline bool_t
#ifdef _X86_64_
cas(volatile pointer_t * addr, pointer_t oldp, const pointer_t newp)
{
    char result;
  __asm__ __volatile__("lock; cmpxchg16b %0; setz %1":"=m"(*addr),
		       "=q"(result)
		       :"m"(*addr), "a"(oldp.count), "d"(oldp.ptr),
		       "b"(newp.count), "c"(newp.ptr)
		       :"memory");
  return (((int)result == 0) ? false:true);
}
#else
cas(volatile pointer_t * addr, const pointer_t oldp, const pointer_t newp)
{
    char result;
  __asm__ __volatile__("lock; cmpxchg8b %0; setz %1":"=m"(*addr),
		       "=q"(result)
		       :"m"(*addr), "a"(oldp.count), "d"(oldp.ptr),
			"b"(newp.count), "c"(newp.ptr)
		       :"memory");
  return (((int)result == 0) ? false:true);
}
#endif


static node_t *create_node(const val_t val)
{
    node_t *node;

    if ((node = (node_t *) calloc(1, sizeof(node_t))) == NULL) {
      elog("calloc error");
	return NULL;
    }

    node->val = val;
    node->next.ptr = NULL;
    node->next.count = 0;

    return node;
}

static void free_node(node_t * node)
{
#ifdef _FREE_
    free(node);
#endif
}


queue_t *init_queue(void)
{
    queue_t *q;
    node_t *node;

    if ((q = (queue_t *) calloc(1, sizeof(queue_t))) == NULL) {
      elog("calloc error");
	return NULL;
    }

    if ((node = create_node((val_t)NULL)) == NULL) {
      elog("create_node() error");
      abort();
    }

    q->head.ptr = node;
    q->tail.ptr = node;

    return q;
}

void free_queue(queue_t * q)
{
  free(q);
}


bool_t enq(queue_t * q, const val_t val)
{
    node_t *newNode;
    pointer_t tail, next, tmp;

    if ((newNode = create_node(val)) == NULL)
	return false;

    while (1) {
	tail = q->tail;
	next = tail.ptr->next;

	if (tail.count == q->tail.count && tail.ptr == q->tail.ptr) {
	  if (next.ptr == NULL) {
	    tmp.ptr = newNode;
	    tmp.count = next.count + 1;
	    if (cas(&tail.ptr->next, next, tmp) == true) {
	      break;
	    }
	  }
	  else {
	    tmp.ptr = next.ptr;
	    tmp.count = tail.count + 1;
	    cas(&q->tail, tail, tmp);
	  }
	}
    }
    tmp.ptr = newNode;    tmp.count = tail.count + 1;
    cas(&q->tail, tail, tmp);

    return true;
}


bool_t deq(queue_t * q, val_t * val)
{
  pointer_t head, tail, next, tmp;
 
    while (1) {
	head = q->head;
	tail = q->tail;
	next = head.ptr->next;

	if (head.count == q->head.count && head.ptr == q->head.ptr) {
	  if (head.ptr == tail.ptr) {
	    if (next.ptr == NULL) {
	      return false;
	    }
	    tmp.ptr = next.ptr;
	    tmp.count = head.count + 1;
	    cas(&q->tail, tail, tmp);
	  }
	  else {
	    *val = next.ptr->val;
	    tmp.ptr = next.ptr;
	    tmp.count = head.count + 1;
	    if (cas(&q->head, head, tmp) == true) {
	      break;
	    }
	  }
	}
    }

    free_node (head.ptr);
    return true;
}


void show_queue(queue_t * q)
{
    node_t *curr;

    curr = q->head.ptr;
    while ((curr = curr->next.ptr) != NULL) {
	printf("[%d]", (int) curr->val);
    }
    printf("\n");

}


#ifdef _SINGLE_THREAD_

queue_t *q;

int main(int argc, char **argv)
{
    int i;
    val_t val;

    int max = 10;

    q = init_queue();

    for (i = 0; i < max; i++) {
      enq(q, i);
      show_queue(q);
    }

    for (i = 0; i < max; i++) {
      deq(q, &val);
      show_queue(q);
    }

    free_queue(q);
    return 0;
}

#endif
