/* ---------------------------------------------------------------------------
 * Skiplist utils
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.16
 * Copyright (C) 2009-2022  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */

#ifndef _SKIPLIST_WORKSPACE_H_
#define _SKIPLIST_WORKSPACE_H_


/* 
 * static workspace_t *init_workspace(const int maxLevel)
 *
 * Initialize both preds and succs of skiplist *sl in each thread.
 * Return the pointers of preds and succs if success, otherwise NULL.
 */
static workspace_t *init_workspace(const int maxLevel)
{
    workspace_t *ws;

    if ((ws = (workspace_t *) calloc(1, sizeof(workspace_t))) == NULL) {
	elog("calloc error");
	return NULL;
    }

    if ((ws->preds = (skiplist_node_t **) calloc(1, sizeof(skiplist_node_t *) * maxLevel)) == NULL) {
	elog("calloc error");
	goto end;
    }
    if ((ws->succs = (skiplist_node_t **) calloc(1, sizeof(skiplist_node_t *) * maxLevel)) == NULL) {
	elog("calloc error");
	goto end;
    }

    return ws;

  end:
    free(ws->preds);
    free(ws);
    return (workspace_t *) NULL;
}

/*
 * void free_workspace(workspace_t * ws)
 *
 * Free ws.
 */
static void free_workspace(workspace_t * ws)
{
    free(ws->preds);
    free(ws->succs);
    free(ws);
}

/*
 * workspace_t *get_workspace(skiplist_t * sl)
 *
 * Return the pointer of the workspace.
 *
 */
static workspace_t *get_workspace(skiplist_t * sl)
{
  /* Get workspace. */
  workspace_t *workspace = pthread_getspecific(sl->workspace_key);

  /* If the workspace has not been allocated yet. */
  if (workspace == NULL) {
    /* Initialize the workspace. */
    if ((workspace = init_workspace(sl->maxLevel)) != NULL) { 
      if (pthread_setspecific(sl->workspace_key, (void *) workspace) != 0) {
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

#endif
