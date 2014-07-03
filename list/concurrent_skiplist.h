/* ---------------------------------------------------------------------------
 * Skiplist utils
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Nov.16
 * Copyright (C) 2009  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */

#ifndef _SKIPLIST_WORKSPACE_H_
#define _SKIPLIST_WORKSPACE_H_


/* 
 * static workspace_t *init_workspace(const int maxLevel)
 *
 * (スレッド毎に)skiplist *slの補助メモリ領域:predsとsuccsを確保する。
 *
 * 成功すると確保したメモリへのポインタを返す。失敗するとNULLを返す。
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
 * wsを開放する。
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
 * (スレッド毎に)初めて呼ばれた時はメモリ領域へ確保し、pthread_keyに対応させる。
 * 必ずスレッドに対応したメモリ領域へのポインタを返す。
 *
 */
static workspace_t *get_workspace(skiplist_t * sl)
{
  /* スレッド毎のメモリ領域へのポインタを得る */
  workspace_t *workspace = pthread_getspecific(sl->workspace_key);

  /* まだメモリ領域が確保されていない場合: */
  if (workspace == NULL) {
    /* メモリ領域の確保 */
    if ((workspace = init_workspace(sl->maxLevel)) != NULL) { 
      /* メモリ領域をpthread_keyに対応させる */
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
