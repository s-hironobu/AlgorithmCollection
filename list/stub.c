/* ---------------------------------------------------------------------------
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.01
 * Copyright (C) 2009-2025  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>

#include "common.h"

#ifdef   _CoarseGrainedSynchroList_
#include "CoarseGrainedSynchroList.h"
#elif    _FineGrainedSynchroList_
#include "FineGrainedSynchroList.h"
#elif    _LazySynchroList_
#include "LazySynchroList.h"
#elif    _LockFreeList_
#include "LockFreeList.h"
#elif    _NonBlockingList_
#include "NonBlockingList.h"
#elif    _Skiplist_
#include "Skiplist.h"
#elif    _LazySkiplist_
#include "LazySkiplist.h"
#elif    _LockFreeSkiplist_
#include "LockFreeSkiplist.h"
#endif


#define PIPE_MAXLINE 32
#define MAX_THREADS 200
#define MAX_ITEMS 30000
#define MAX_LEVEL 16

#define DEFAULT_THREADS 10
#define DEFAULT_ITEMS 1000
#define DEFAULT_LEVEL 4

#if defined(_Skiplist_) || (_LazySkiplist_) || (_LockFreeSkiplist_)
static skiplist_t *list;
#else
static list_t *list;
#endif


static long long int sum[MAX_THREADS];
static long long int check[MAX_THREADS * MAX_ITEMS];

static pthread_mutex_t begin_mtx;
static pthread_cond_t begin_cond;
static unsigned int begin_thread_num;
static pthread_mutex_t end_mtx;
static pthread_cond_t end_cond;
static unsigned int end_thread_num;

typedef struct {
    int thread_num;
    int item_num;
    int verbose;
    int max_level;
} system_variables_t;

struct stat_time {
    struct timeval begin;
    struct timeval end;
};
typedef struct stat_time stat_data_t;

/*
 * declartion
 */
static double get_interval(struct timeval, struct timeval);
static void master_thread(void);
static void worker_thread(void *);
static int workbench(void);
static void usage(char **);
static void init_system_variables(void);

/*
 * global variables
 */
static system_variables_t system_variables;
static pthread_t *work_thread_tptr;
static pthread_t tid;
static stat_data_t *stat_data;

static struct timeval stat_data_begin;
static struct timeval stat_data_end;

/*
 * local functions
 */

static double get_interval(struct timeval bt, struct timeval et)
{
    double b, e;

    b = bt.tv_sec + (double) bt.tv_usec * 1e-6;
    e = et.tv_sec + (double) et.tv_usec * 1e-6;
    return e - b;
}


/*
 * master_thread
 */
static void master_thread(void)
{
    unsigned int i;

    /* wait for all threads end */
    pthread_mutex_lock(&end_mtx);
    while (system_variables.thread_num != end_thread_num)
	pthread_cond_wait(&end_cond, &end_mtx);
    pthread_mutex_unlock(&end_mtx);

    //    show_list(list);
    free_list(list);

    /* display result */
    double tmp_itvl;
    double min_itvl = 0x7fffffff;
    double ave_itvl = 0.0;
    double max_itvl = 0.0;
    long double itvl = 0.0;

    unsigned long long int total = 0;
    long long int count1, count2;

    gettimeofday(&stat_data_end, NULL);

    total = 0;
    for (i = 0; i < system_variables.thread_num; i++) {
      total += sum[i];

      tmp_itvl = get_interval(stat_data[i].begin, stat_data[i].end);
      
      itvl += tmp_itvl;
      if (max_itvl < tmp_itvl)
	max_itvl = tmp_itvl;
      if (tmp_itvl < min_itvl)
	min_itvl = tmp_itvl;
      if (0 < system_variables.verbose)
	fprintf(stderr, "thread(%d) end %f[sec]\n", i, tmp_itvl);
    }
    /*
    if (total != (((system_variables.item_num * system_variables.thread_num) *
		 ((system_variables.item_num * system_variables.thread_num) + 1)) / 2))
    */
    if (((system_variables.item_num * system_variables.thread_num) % 2) == 0) {
      count1 = (system_variables.item_num * system_variables.thread_num) / 2;
      count2 = (system_variables.item_num * system_variables.thread_num + 1);
    }
    else {
      count1 = (system_variables.item_num * system_variables.thread_num + 1) / 2;
      count2 = (system_variables.item_num * system_variables.thread_num);
    }
    if (total != (count1 * count2))
      fprintf (stderr, "RESULT: test FAILED!\n");
    else
      fprintf (stderr, "RESULT: test OK\n");

    fprintf (stderr, "condition =>\n");
    printf ("\t%d threads run\n", system_variables.thread_num);
    printf ("\t%d items inserted and deleted / thread, total %d items\n",
	    system_variables.item_num,
	    system_variables.item_num * system_variables.thread_num);

    assert(0 < system_variables.thread_num);
    ave_itvl = (double) (itvl / system_variables.thread_num);

    tmp_itvl = get_interval(stat_data_begin, stat_data_end);

    fprintf(stderr, "performance =>\n\tinterval =  %f [sec]\n", tmp_itvl);

    fprintf
      (stderr, "\tthread info:\n\t  ave. = %f[sec], min = %f[sec], max = %f[sec]\n",
       ave_itvl, min_itvl, max_itvl);
}



static void worker_thread(void *arg)
{
    uintptr_t no = (uintptr_t) arg;
    unsigned int i;
    lkey_t key;
    val_t getval;

    /*
     * increment begin_thread_num, and wait for broadcast signal from last created thread
     */
    if (system_variables.thread_num != 1) {
      pthread_mutex_lock(&begin_mtx);
      begin_thread_num++;
      if (begin_thread_num == system_variables.thread_num)
	pthread_cond_broadcast(&begin_cond);
      else {
	while (begin_thread_num < system_variables.thread_num)
	  pthread_cond_wait(&begin_cond, &begin_mtx);	
      }
      pthread_mutex_unlock(&begin_mtx);
    }

    gettimeofday(&stat_data[no].begin, NULL);
    sum[no] = 0;

    /*  main loop */
    key = no * system_variables.item_num;
    for (i = 0; i < system_variables.item_num; i++) {
      ++key;
      if (0 < system_variables.verbose)
	fprintf(stderr, "thread[%lu] add: %lu\n", (uintptr_t)no,
		(uintptr_t) key);

      if (add(list, (lkey_t) key, (val_t)key) != true)
	fprintf (stderr, "ERROR[%lu]: add %lu\n", (uintptr_t)no, (uintptr_t)key);

      if (1 < system_variables.verbose)
	show_list(list);
      
      //      usleep(no);
      //      pthread_yield(NULL);
    }

    usleep(no * 10);

    key = no * system_variables.item_num;
    for (i = 0; i < system_variables.item_num; i++) {
      ++key;
      if (delete(list, (lkey_t) key, &getval) != true) {
	printf ("ERROR[%lu]: del %lu\n", (uintptr_t)no, (uintptr_t)key);
      }

      if (1 < system_variables.verbose)
	show_list(list);
      
      if (0 < system_variables.verbose)
	fprintf(stderr, "delete: val = %ld\n", (lkey_t) getval);
      
      sum[no] += getval;
      
      check[getval]++;
    }

    /* send signal */
    gettimeofday(&stat_data[no].end, NULL);
    pthread_mutex_lock(&end_mtx);
    end_thread_num++;
    pthread_cond_signal(&end_cond);
    pthread_mutex_unlock(&end_mtx);
}

static int workbench(void)
{
    void *ret = NULL;
    unsigned int i;

    fprintf(stderr, "<<simple algorithm test bench>>\n");

#if defined(_Skiplist_) || (_LazySkiplist_) || (_LockFreeSkiplist_)
    if ((list = init_list(system_variables.max_level, INT_MIN, INT_MAX)) == NULL) {
#else
    if ((list = init_list()) == NULL) {
#endif
      elog("init_list() error");
      abort();
    }

    for (i = 0; i < system_variables.thread_num * system_variables.item_num; i++)
      check[i] = 0;


    if ((stat_data =
	 calloc(system_variables.thread_num, sizeof(stat_data_t))) == NULL)
    {
      elog("calloc error");
      goto end;
    }
    if ((work_thread_tptr =
	 calloc(system_variables.thread_num, sizeof(pthread_t))) == NULL) {
      elog("calloc error");
      goto end;
    }
    if (pthread_create(&tid, (void *) NULL, (void *) master_thread,
		       (void *) NULL) != 0) {
      elog("pthread_create() error");
      goto end;
    }
    gettimeofday(&stat_data_begin, NULL);

    for (i = 0; i < system_variables.thread_num; i++)
      if (pthread_create(&work_thread_tptr[i], NULL, (void *) worker_thread,
			 (void *)(intptr_t) i) != 0) {
	elog("pthread_create() error");
	goto end;
      }

    for (i = 0; i < system_variables.thread_num; i++)
	if (pthread_join(work_thread_tptr[i], &ret)) {
	  elog("pthread_join() error");
	  goto end;
	}

    if (pthread_join(tid, &ret)) {
      elog("pthread_join() error");
      goto end;
    }

    return 0;

 end:
    free(stat_data);
    free(work_thread_tptr);
    return -1;
}


static void usage(char **argv)
{
    fprintf(stderr, "simple algorithm test bench\n");
    fprintf(stderr, "usage: %s [Options<default>]\n", argv[0]);
    fprintf(stderr, "\t\t-t number_of_threads<%d>\n", DEFAULT_THREADS);
    fprintf(stderr, "\t\t-n number_of_items<%d>\n", DEFAULT_ITEMS);
#if defined(_Skiplist_) || (_LazySkiplist_) || (_LockFreeSkiplist_)
    fprintf(stderr, "\t\t-l max_level_of_skiplist<%d>\n", DEFAULT_LEVEL);
#endif
    fprintf(stderr, "\t\t-v               :verbose\n");
    fprintf(stderr, "\t\t-V               :debug mode\n");
    fprintf(stderr, "\t\t-h               :help\n");
}


static void init_system_variables(void)
{
    system_variables.thread_num = DEFAULT_THREADS;
    system_variables.item_num = DEFAULT_ITEMS;
    system_variables.max_level = DEFAULT_LEVEL;
    system_variables.verbose = 0;
}


int main(int argc, char **argv)
{
    char c;

    /*
     * init 
     */
    begin_mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    begin_cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
    end_mtx = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    end_cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
    begin_thread_num = 0;
    end_thread_num = 0;
    init_system_variables();

    /* options  */
#if defined(_Skiplist_) || (_LazySkiplist_) || (_LockFreeSkiplist_)
    while ((c = getopt(argc, argv, "t:n:l:vVh")) != -1) {
#else
    while ((c = getopt(argc, argv, "t:n:vVh")) != -1) {
#endif
	switch (c) {
	case 't':		/* number of thread */
	    system_variables.thread_num = strtol(optarg, NULL, 10);
	    if (system_variables.thread_num <= 0) {
		fprintf(stderr, "Error: thread number %d is not valid\n",
			system_variables.thread_num);
		exit(-1);
	    } else if (MAX_THREADS <= system_variables.thread_num)
		system_variables.thread_num = MAX_THREADS;

	    break;
	case 'n':		/* number of item */
	    system_variables.item_num = strtol(optarg, NULL, 10);
	    if (system_variables.item_num <= 0) {
		fprintf(stderr, "Error: item number %d is not valid\n",
			system_variables.item_num);
		exit(-1);
	    } else if (MAX_ITEMS <= system_variables.item_num)
		system_variables.item_num = MAX_ITEMS;
	    break;
#if defined(_Skiplist_) || (_LazySkiplist_) || (_LockFreeSkiplist_)
	case 'l':		/* max level of skiplist */
	    system_variables.max_level = strtol(optarg, NULL, 10);
	    if (system_variables.max_level <= 0) {
		fprintf(stderr, "Error: max level %d is not valid\n",
			system_variables.max_level);
		exit(-1);
	    } else if (MAX_LEVEL <= system_variables.max_level)
		system_variables.max_level = MAX_LEVEL;
	    break;
#endif
	case 'v':               /* verbose 1 */
	    system_variables.verbose = 1;
	    break;
	case 'V':               /* verbose 2 */
	    system_variables.verbose = 2;
	    break;
	case 'h':	        /* help */
	    usage(argv);
	    exit(0);
	default:
	    fprintf(stderr, "ERROR: option error: -%c is not valid\n",
		    optopt);
	    exit(-1);
	}
    }

    /*
     * main work 
     */
    if (workbench() != 0)
      abort();

    free (stat_data);
    free (work_thread_tptr);

    return 0;
}

// EOF
