/* ---------------------------------------------------------------------------
 * 
 * author: suzuki hironobu (hironobu@interdb.jp) 2009.Dec.01
 * Copyright (C) 2009  suzuki hironobu
 *
 * ---------------------------------------------------------------------------
 */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <inttypes.h>

#ifndef C_H
#ifndef bool
typedef char bool;
#endif
#ifndef true
#define true    ((bool) 1)
#endif
#ifndef false
#define false   ((bool) 0)
#endif
typedef bool *BoolPtr;
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif
#ifndef NULL
#define NULL    ((void *) 0)
#endif
#endif

typedef bool bool_t;
typedef intptr_t lkey_t;
typedef intptr_t  val_t;


#define elog(_message_)  do {fprintf(stderr,			        \
				     "%s():%s:%u: %s\n",		\
				     __FUNCTION__, __FILE__, __LINE__,	\
				     _message_); fflush(stderr);}while(0);

#endif
