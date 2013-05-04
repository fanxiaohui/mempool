#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"

#include "mempool.h"

//more memory pool algorithms coming....
mempool mempool_init(size_t size, bool pool_share, POOL_TYPE pool_type,
			mempool_alloc_fn chunk_alloc,
			mempool_chunk_free_fn chunk_free,
			mempool_pool_free_fn pool_destory)
{
	switch(pool_type){
	case LINK_POOL:
	case POOL_CUSTOMIZE:
	    return link_mempool_init(size, pool_share, chunk_alloc, chunk_free, pool_destory);
	    break;
	default:
	    printf("unkno");
	    exit(1);
	}
}
