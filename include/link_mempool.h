#ifndef __LINK_MEMPOOL_H
#define __LINK_MEMPOOL_H

#define T_HP 	0
#define T_NP 	1

#define GO_LEFT 	0
#define GO_RIGHT 	1

#define HP_CSIZE_THRESHOLD	128
#define NP_CSIZE_THRESHOLD	1024

#define MAX_HP_MEMBER	50

#define exec_lock_fn(fn,...)	if(fn != NULL) fn(__VA_ARGS__)

#ifdef __cplusplus
extern "C"{
#endif

typedef struct mem_chunk *link_mem_chunk;	//data structure for memory chunk
typedef struct intra_mempool *link_intra_mempool;	//data structure to manage memory chunk
typedef struct mempool_t *link_mempool;	//data structure for mempool

typedef struct lock_ops		*mempool_lock_ops;
typedef u_char *link_mem_addr;	

typedef void * (*mempool_priv_alloc_fn) (size_t);	//memory alloc func used in mempool
typedef struct mempool_t * (*mempool_create_fn) (size_t);	//mempool creation func
typedef struct mem_chunk * (*mempool_alloc_fn) (struct mempool_t *, size_t);	//alloc chunk from mempool func
typedef void (*mempool_chunk_free_fn) (struct mempool_t *, struct mem_chunk *);	//free chunk func
typedef void (*mempool_pool_free_fn) (struct mempool_t *);	//free mempool func

struct mem_chunk {
	size_t chunk_size;
	
	unsigned long match;
	
	link_mem_addr	data_start;
	link_mem_addr	data_end;
	
	link_mem_chunk	next;
	link_mem_chunk	prev;
};

struct intra_mempool {
	size_t pool_member;
	
	void *T_CHAIN;
	
	link_mem_chunk	link;
	link_mem_chunk	head;
	link_mem_chunk	tail;
};

struct mempool_t {
	size_t mem_size;
	
	size_t mem_left;
	
	size_t real_mem_left;
	
	int mem_fd;
	
	void *T_POOL;

	char *mem_name;
	
	link_mem_addr real_mem;
	link_mem_addr real_mem_h;
	
	link_intra_mempool high_cache_pool;
	
	link_intra_mempool normal_pool;
	
	mempool_lock_ops m_ops;
};

#ifdef __cplusplus
}
#endif

#endif
