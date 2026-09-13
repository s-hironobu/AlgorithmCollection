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

/*
 * Portable memory barriers (x86_64 and arm64/Apple Silicon).
 * __sync_synchronize() is a GCC/Clang builtin that lowers to the
 * appropriate fence instruction on every supported architecture
 * (MFENCE on x86_64, DMB ISH on arm64), unlike the previous
 * x86-only inline assembly.
 */
#define MB()  __sync_synchronize()
#define WMB() __sync_synchronize()
#define RMB() __sync_synchronize()

#endif
