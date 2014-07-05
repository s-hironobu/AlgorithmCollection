/* ---------------------------------------------------------------------------
 * UnBounded LockFree Queue
 *
 * "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms" by M. Michael and M. Scott
 * http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.03
 * Copyright (C) 2009-2014  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _CAS_LOCKFREE_QUEUE_H_
#define _CAS_LOCKFREE_QUEUE_H_

#include <inttypes.h>
#include "common.h"

typedef struct _pointer_t {
  intptr_t count;
  struct _node_t *ptr;
}__attribute__((packed)) pointer_t;

typedef struct _node_t {
  /* Do not change this order. */
  pointer_t next;
  val_t val;
}__attribute__((packed)) node_t;


typedef struct _queue_t
{
  pointer_t head;
  pointer_t tail;
} queue_t;

queue_t * init_queue (void);
void free_queue (queue_t *);
bool_t enq (queue_t *, const val_t);
bool_t deq (queue_t *, val_t *);

void show_queue(queue_t *);

#endif
