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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "LLSCLockFreeQueue.h"

static workspace_t *init_workspace();
static void free_workspace(workspace_t *);
static workspace_t *get_workspace(queue_t *);
static node_t *LL(LLSCvar *, int *, node_t **);
static bool_t SC(LLSCvar *, node_t *, int, node_t *);
static void transfer(node_t *, int);
static void release(node_t *);
static void unlink(LLSCvar *, int, node_t *);
static void setNLPred(node_t *);
static void setToBeFreed(node_t *);
static node_t *create_node (val_t);

#ifdef _X86_64_
static inline bool_t cas(void *ptr, uint64_t oldv, uint64_t newv)
{
  uint64_t result;
  __asm__ __volatile__("lock; cmpxchgq %1,%2"
                       : "=a" (result)
                       : "q" (newv), "m" (*(uint64_t *)ptr),"0" (oldv)
                       : "memory");
  return ((result == oldv) ? true : false);
}
#define CAST(value)   (*((uint64_t *)&(value)))

#else
static inline bool_t cas(void *ptr, uint32_t oldv, uint32_t newv)
{
  uint32_t result;
  __asm__ __volatile__("lock; cmpxchgl %1,%2"
                       : "=a" (result)
                       : "q" (newv), "m" (*(uint32_t *)ptr),"0" (oldv)
                       : "memory");
  return ((result == oldv) ? true : false);
}

#define CAST(value)   (*((uint32_t *)&(value)))
#endif


static workspace_t *init_workspace(void)
{
  workspace_t *ws;
  
  if ((ws = (workspace_t *) calloc(1, sizeof(workspace_t))) == NULL) {
    elog("calloc error");
    return NULL;
  }
  
  return ws;
}

static void free_workspace(workspace_t * ws)
{
  free(ws);
}

static workspace_t *get_workspace(queue_t * q)
{
  workspace_t *workspace = pthread_getspecific(q->workspace_key);

  if (workspace == NULL) {
    if ((workspace = init_workspace()) != NULL) { 
      if (pthread_setspecific(q->workspace_key, (void *) workspace) != 0) {
	elog("pthread_setspecific() error");
	abort();
      }
    } else {
      elog("init_workspace() error");
      abort();
    }
  }
  assert(workspace != NULL);
  return workspace;
}


#define CURRENT(loc, ver)  (ver % 2 == 0 ? loc->ptr0 : loc->ptr1)
#define NONCURADDR(loc, ver)  (ver % 2 == 0 ? (void *)&(loc->ptr1) : (void *)&(loc->ptr0))
#define CLEAN(exit)     ((exit.count == 0) && (exit.transfersLeft == 0))
#define FREEABLE(exit)  (CLEAN(exit) &&  exit.nlP && exit.toBeFreed)


static node_t 
*LL(LLSCvar *loc, int *myver, node_t **mynode)
{
  EntryTag e, new;
  do {
    e = loc->entry;
    *myver = e.ver;
    *mynode = CURRENT(loc, e.ver);
    {
      new.ver = e.ver;
      new.count = e.count + 1;
    }
  } while (!cas(&loc->entry, CAST(e), CAST(new)));

  return *mynode;
}

static bool_t
SC(LLSCvar *loc, node_t *nd, int myver, node_t *mynode)
{
  EntryTag e, new;
  node_t *pred_nd = mynode->pred;
  bool_t success = cas(NONCURADDR(loc, myver), CAST(pred_nd), CAST(nd));

  /*
  if (!success)
    free(new_nd);
  */

  e = loc->entry;
  while (e.ver == myver) {
    {
      new.ver = e.ver + 1;
      new.count = 0;
    }
    if (cas(&loc->entry, CAST(e), CAST(new)))
      transfer(mynode, e.count);
    e = loc->entry;
  }
  release(mynode);
  return success;
}

static void 
transfer(node_t *nd, int count)
{
  ExitTag pre, post;
  do {
    pre = nd->exit;
    {
      post.count = pre.count + count;
      post.transfersLeft = pre.transfersLeft - 1;
      post.nlP = pre.nlP;
      post.toBeFreed = pre.toBeFreed;
    }
  } while (!cas(&nd->exit, CAST(pre), CAST(post)));
}


static void 
release(node_t *nd)
{
  ExitTag pre, post;
  node_t *pred_nd = nd->pred;

  do {
    pre = nd->exit;
    {
      post.count = pre.count - 1;
      post.transfersLeft = pre.transfersLeft;
      post.nlP = pre.nlP;
      post.toBeFreed = pre.toBeFreed;
    }
  } while (!cas(&nd->exit, CAST(pre), CAST(post)));
  
  if (CLEAN(post))
    setNLPred(pred_nd);

  if (FREEABLE(post))
    free(nd);
}


static void 
unlink(LLSCvar *loc, int myver, node_t *mynode)
{
  EntryTag e, new;
  do {
    e = loc->entry;
  } while (e.ver == myver);

  {
    new.ver = e.ver;
    new.count = e.count - 1;
  }
  if (!cas(&loc->entry, CAST(e), CAST(new)))
    release(mynode);
}

static void 
setNLPred(node_t *pred_nd)
{
  ExitTag pre, post;
    do {
    pre = pred_nd->exit;
    {
      post.count = pre.count;
      post.transfersLeft = pre.transfersLeft;
      post.nlP = true;
      post.toBeFreed = pre.toBeFreed;
    }
    } while (!cas(&pred_nd->exit, CAST(pre), CAST(post)));
  if (FREEABLE(post))
    free(pred_nd);
}

static void
setToBeFreed(node_t *pred_nd) 
{
  ExitTag pre, post;

  do {
    pre = pred_nd->exit;
    {
      post.count = pre.count;
      post.transfersLeft = pre.transfersLeft;
      post.nlP = pre.nlP;
      post.toBeFreed = true;
    }
  } while (!cas(&pred_nd->exit, CAST(pre), CAST(post)));
  
  if (FREEABLE(post)) 
    free(pred_nd);
}

static node_t *create_node (val_t val)
{
  node_t *node;
  if ((node = (node_t*)calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    return NULL;
  }
  node->val = val;
  node->next = NULL;
  node->exit.count = 0;
  node->exit.transfersLeft = 2;
  node->exit.nlP = false;
  node->exit.toBeFreed = false;

  return node;
}

#ifdef _FREE_
#define free_node(node)  free(node)
#else
#define free_node(node)  ;
#endif


bool_t enq(queue_t *q, val_t val)
{
  bool_t ret = true;
  node_t *nd, *tail;
  workspace_t *ws = get_workspace(q);
  assert(ws != NULL);

  if ((nd = create_node(val)) == NULL) {
    return false;
  }

  while (1) {
    tail = LL(&q->tail, &ws->myver, &ws->mynode);

    nd->pred = tail;
    if (cas(&tail->next, (uintptr_t)NULL, CAST(nd))) {
      SC(&q->tail, nd, ws->myver, ws->mynode);
      break;
    } 
    else {
      SC(&q->tail, tail->next, ws->myver, ws->mynode);
    }
  }
  return ret;
}

bool_t deq(queue_t *q, val_t *val) 
{
  bool_t ret = true;
  node_t *head, *next;
  workspace_t *ws = get_workspace(q);
  assert(ws != NULL);
  
  while (1) {
    head = LL(&q->head, &ws->myver, &ws->mynode);
    next = head->next;
    if (next == NULL) {
      unlink(&q->head, ws->myver, ws->mynode);
      *val = (val_t)NULL;
      ret = false;
      break;
    }
    
    if (SC(&q->head, next, ws->myver, ws->mynode)) {
      *val = next->val;
      setToBeFreed(next);
      free_node(next);
      break;
    }
  }
  return ret;
}


queue_t *init_queue (void)
{
  queue_t *q;

  if ((q = (queue_t *) calloc(1, sizeof(queue_t))) == NULL) {
    elog("calloc error");
    return NULL;
  }

  q->tail.entry.ver = 0;
  q->tail.entry.count = 0;
  
  if ((q->tail.ptr0 = (node_t *)calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    goto end;
  }
  if ((q->tail.ptr1 = (node_t *)calloc(1, sizeof(node_t))) == NULL) {
    elog("calloc error");
    goto end;
  }
  
  q->tail.ptr0->pred = q->tail.ptr1;
  
  q->tail.ptr0->exit.count = 0;
  q->tail.ptr0->exit.transfersLeft = 2;
  q->tail.ptr0->exit.nlP = 0;
  q->tail.ptr0->exit.toBeFreed = 0;

  q->tail.ptr0->next = NULL;
  
  q->tail.ptr1->exit.count = 0;
  q->tail.ptr1->exit.transfersLeft = 0;
  q->tail.ptr1->exit.nlP = 0;
  q->tail.ptr1->exit.toBeFreed = 0;
  
  q->head = q->tail;

  if (pthread_key_create(&q->workspace_key, (void *) free_workspace) != 0) {
    elog("pthread_key_create() error");
    abort();
  }

  return q;

 end:
  free(q->tail.ptr0);
  free(q);
  return NULL;
}

void
free_queue(queue_t *q)
{
  free(q);
}


void show_queue(queue_t * q)
{
  LLSCvar e = q->head;
  node_t *curr = (e.entry.ver % 2 == 0 ? e.ptr0 : e.ptr1);
  
  while ((curr = curr->next) != NULL)
    printf("[%ld]", (long int) curr->val);
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
