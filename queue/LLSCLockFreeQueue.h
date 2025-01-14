/* ---------------------------------------------------------------------------
 * LL/SC based LockFree Queue
 * 
 * "Bringing Practical LockFree Synchronization to 64Bit Applications" 
 *  by Simon Doherty, Maurice Herlihy, Victor Luchangco, Mark Moir
 *
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.03
 * Copyright (C) 2009-2025  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef _LLSC_LOCKFREE_QUEUE_H_
#define _LLSC_LOCKFREE_QUEUE_H_

#include "common.h"

typedef struct _ExitTag {
  int count;
  int transfersLeft;
  bool_t nlP;
  bool_t toBeFreed;
} ExitTag;

typedef struct _node_t {
  val_t val;

  struct _node_t *next;
  struct _node_t *pred;
  ExitTag exit;
} node_t;

typedef struct _EntryTag {
  int ver;
  int count;
} EntryTag;


typedef struct _LLSCvar {
  node_t *ptr0;
  node_t *ptr1;
  EntryTag entry;
} LLSCvar;



typedef struct _queue_t {
  LLSCvar head;
  LLSCvar tail;
  pthread_key_t workspace_key;
} queue_t;


typedef struct _workspace_t {
  int myver;
  node_t *mynode __attribute__((aligned(16)));
} workspace_t __attribute__((aligned(16)));


bool_t enq(queue_t *, val_t);
bool_t deq(queue_t *, val_t*);
queue_t *init_queue (void);
void free_queue(queue_t *);
void show_queue(queue_t *);

#endif
