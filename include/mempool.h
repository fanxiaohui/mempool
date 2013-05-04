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

#define LINK_MEMPOOL		0
#define CUSTOMIZE_MEMPOOL	1

#ifdef __cplusplus
extern "C"{
#endif

typedef struct mempool_class	*mempool;

typedef void * (*mempool_alloc_fn) (void *m_pool , size_t);	//alloc chunk from mempool func
typedef void (*mempool_chunk_free_fn) (void *m_pool, void *chunk);	//free chunk func
typedef void (*mempool_pool_free_fn) (void *m_pool);	//free mempool func

//contain the basic funcs decelarations for mempool
struct mempool_func {
	mempool_alloc_fn	chunk_alloc;
	
	mempool_chunk_free_fn	chunk_free;
	
	mempool_pool_free_fn	pool_free;
};

struct mempool_class {
	void 	*m_pool;
	
	struct mempool_func m_func;
};

typedef enum {
	POOL_TYPE_MIN = -1,
	LINK_POOL = LINK_MEMPOOL,
	POOL_CUSTOMIZE = CUSTOMIZE_MEMPOOL,
	POOL_TYPE_MAX,
}POOL_TYPE;

extern mempool mempool_init(size_t , bool , POOL_TYPE ,
			mempool_alloc_fn , mempool_chunk_free_fn , mempool_pool_free_fn);

extern mempool link_mempool_init(size_t , bool , mempool_alloc_fn ,
			mempool_chunk_free_fn , mempool_pool_free_fn );

#ifdef __cplusplus
}
#endif

#endif
