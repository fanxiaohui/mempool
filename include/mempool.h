#ifndef _MEMPOOL_H
#define _MEMPOOL_H

/*can anyone tell me whats wrong with:
 *#define __location__	__func__ ":" __mem_ST3__
 *whats the prototype of __func__? It is not a string?!
 * */
#ifndef __location__
#define __mem_ST1__(s)	#s
#define __mem_ST2__(s)	__mem_ST1__(s)
#define __mem_ST3__	__mem_ST2__(__LINE__)
#define __location__	__FILE__ ":" __mem_ST3__
#endif

#ifdef RELEASE
#define mem_print
#else
#define mem_print printf
#endif

#define ALIGN_SIZE 	64	//should be the power of 2!
#define size_align(p) (((uintptr_t)(p) + ((uintptr_t)ALIGN_SIZE - 1))&~((uintptr_t)ALIGN_SIZE - 1))

#ifdef __cplusplus
extern "C"{
#endif

#include "lock.h"
#include "link_mempool.h"

typedef struct mempool_class	*mempool;

//contain the basic funcs decelarations for mempool
struct mempool_func {
	mempool_create_fn	pool_create;
	
	mempool_alloc_fn	chunk_alloc;
	
	mempool_chunk_free_fn	chunk_free;
	
	mempool_pool_free_fn	pool_free;
};

struct mempool_class {
	void 	*m_pool;
	
	struct mempool_func m_func;
};

mempool link_mempool_init(size_t , bool , mempool_create_fn , mempool_alloc_fn ,
			mempool_chunk_free_fn , mempool_pool_free_fn );

#ifdef __cplusplus
}
#endif

#endif
