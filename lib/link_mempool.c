#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mempool.h"

static inline void *link_priv_alloc(size_t );
static link_mempool pool_create(size_t );
static link_intra_mempool init_intraPool(void );
static inline link_mem_chunk make_mem_chunk(size_t );
static void remove_node(link_intra_mempool, link_mem_chunk );
static link_mem_chunk search_high_cache_pool(link_intra_mempool , size_t );
static link_mem_chunk get_NP_chunk(link_intra_mempool , size_t , int , bool );
static link_mem_chunk chunk_split(link_mem_chunk , size_t );
static link_mem_chunk search_normal_pool(link_intra_mempool , size_t , bool );
static link_mem_chunk make_empty_chunk(void);
static link_mem_chunk mempool_real_alloc(size_t );
static link_mem_chunk mempool_alloc(link_mempool , size_t );
static void mem_chunk_free(link_mempool , link_mem_chunk );
static void free2NP(link_mempool , link_mem_chunk );
static void node_insert(link_intra_mempool , link_mem_chunk , link_mem_chunk );
static void insert2chain(link_intra_mempool , link_mem_chunk , int , int );
static void add_chunk2HP(link_intra_mempool , link_mem_chunk );
static bool free2HP(link_mempool , link_mem_chunk );
static void init_chain(link_intra_mempool , link_mem_chunk );
static link_mem_chunk find_buddy_chunk(link_intra_mempool , link_mem_chunk );
static void add_chunk2NP(link_intra_mempool , link_mem_chunk );
static void add_chunk2NP_fast(link_intra_mempool , link_mem_chunk );
static void freeChunk(link_mem_chunk );
static void freeChain(link_intra_mempool , mempool_lock_ops );
static void destory_pool(link_mempool );
static mempool link_mempool_allocator(size_t , bool , mempool_create_fn ,mempool_alloc_fn ,
				mempool_chunk_free_fn , mempool_pool_free_fn , mempool_priv_alloc_fn );
/*
 *	1. do we need a way to manage all mempool created?
 *	2. need a better way to generate index for each mempool
*/

static inline void *link_priv_alloc(size_t size)
{
	void *p;
	if(posix_memalign(&p, ALIGN_SIZE, size)){
		mem_print("posix_memalign failed(%s)\n", __location__);
		p = NULL;
	}
	
	return p;
}

static link_mempool pool_create(size_t size)
{		
//make sure the size is aligned first!
	size = size_align(size);

	link_mempool p;
	p = (link_mempool)link_priv_alloc(sizeof(struct mempool_t));

	if(!p){
		mem_print("malloc error(%s)\n", __location__);
		return NULL;
	}

	p->mem_size = size;
	p->mem_left = size;
	p->real_mem_left = size;
	
	char tmp_memfile[128] = {0};
	struct timeval tp;
	gettimeofday(&tp, NULL);
	sprintf(tmp_memfile, "/tmp/%d_%d.tmp", tp.tv_sec, tp.tv_usec);	//may have problem....FIXME
	
	p->mem_fd = open(tmp_memfile, O_CREAT|O_RDWR, 0644);
	if(p->mem_fd == -1){
		mem_print("open error(%s)\n", __location__);
		free(p);
		return NULL;
	}

	p->mem_name = strndup(tmp_memfile, strlen(tmp_memfile));
	
	if(ftruncate(p->mem_fd, size)){
		mem_print("ftruncate error(%s)\n", __location__);
		free(p);
		return NULL;
	}

	p->real_mem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, p->mem_fd, 0);
	if(!p->real_mem){
		mem_print("mmap error(%s)\n", __location__);
		free(p);
		return NULL;
	}
	p->real_mem_h = p->real_mem;
	if(posix_madvise(p->real_mem, size, MADV_WILLNEED)){
		mem_print("madvise does not work?!\n");
	}

	p->high_cache_pool = init_intraPool();
	if(!(p->high_cache_pool))
		return NULL;
	
	p->normal_pool = init_intraPool();
	if(!(p->normal_pool))
		return NULL;
			
	return p;
}

static link_intra_mempool init_intraPool(void)
{
	link_intra_mempool p;
	p = (link_intra_mempool )link_priv_alloc(sizeof(struct intra_mempool));
	
	if(!p){
		mem_print("init intra pool fail(%s)\n", __location__);
		return NULL;
	}

	p->pool_member = 0;
	
	p->link = NULL;
	p->head = NULL;
	p->tail = NULL;
	
	return p;
}

static inline link_mem_chunk make_mem_chunk(size_t size)
{
	link_mem_chunk p;
	p = (link_mem_chunk)link_priv_alloc(sizeof(struct mem_chunk));
	if(!p){
		mem_print("chunk init alloc failed(%s)\n", __location__);
		return NULL;
	}
	
	p->chunk_size = size;
	p->match = 0;
	
	p->data_start = NULL;
	p->data_end = NULL;

	p->next = NULL;
	p->prev = NULL;

	return p;
}

static void remove_node(link_intra_mempool chain, link_mem_chunk cur)
{
	static int random = 0;
	if(random == 10000)	//i do not want it overflows as time runs....
		random = 0;
	
	bool flag = false;
	
	if(cur == chain->link){
		flag = true;
		chain->link = cur->next?cur->next:cur->prev;
	}
	
	if(cur == chain->head){
		chain->head = cur->next;
		if(cur->next)
			cur->next->prev = NULL;
		else	//only one node in link
			chain->tail = cur->next;
		
		cur->next = NULL;
		cur->prev = NULL;
	}
	else if(cur == chain->tail){
		chain->tail = cur->prev;
		if(cur->prev)
			cur->prev->next = NULL;
		else
			chain->head = cur->prev;
		
		cur->next = NULL;
		cur->prev = NULL;
	}
	else{
		if(!flag)
			chain->link = random%2?cur->next:cur->prev;	//try to keep the pointer around the recent using chunk
		
		cur->prev->next = cur->next;
		cur->next->prev = cur->prev;
		
		cur->next = NULL;
		cur->prev = NULL;
	}
	
	random++;
}

static link_mem_chunk search_high_cache_pool(link_intra_mempool chain, size_t size)
{
	link_mem_chunk cur;
	int tmp;
	
	cur = chain->head;

	while(cur){
		tmp = cur->chunk_size - size;

		if(tmp >=0 && tmp <= HP_CSIZE_THRESHOLD){	//find the proper chunk in HP
			remove_node(chain, cur);
			
			cur->match++;	//add one for the chunk match rate
			chain->pool_member--;
			return cur;
		}
		
		cur = cur->next;
	}
	
	return cur;
}

//do not lock in this function to avoid race condition!
static link_mem_chunk get_NP_chunk(link_intra_mempool chain, size_t size, int type, bool alloc)
{
	link_mem_chunk p, backup;
	p = chain->link;
	
	bool goal = false;
	int tmp;
	
	if(type == GO_RIGHT){	//next...need a smaller chunk
		backup = p;

		while(p){
			tmp = p->chunk_size - size;
			if(tmp < 0)
				break;
				
			if(tmp <= NP_CSIZE_THRESHOLD){
				remove_node(chain, p);
				
				chain->pool_member--;
				p->match++;
				return p;
			}
			
			if(backup->match < p->match)
				backup = p;
				
			p = p->next;
		}
		//if we could alloc in real memory, return p anyway (NOTE: we garantee there could have a smaller chunk for allocation!)
		if(alloc){
			return NULL;
		}
		else{
			goto DO_CHUNK_SPLIT;
		}
	}
	else{	//prev...need a bigger chunk
		backup = p;

		while(p){
			tmp = p->chunk_size - size;
			if(tmp >= 0){
				if(tmp <= NP_CSIZE_THRESHOLD){
					remove_node(chain, p);
				
					chain->pool_member--;
					p->match++;
					return p;
				}
				else{
					if(backup->match < p->match){
						goal = true;
						backup = p;
					}
				}
			}
				
			p = p->next;
		}
		//i do not know if we need to do the merge. now i just return NULL even if we have enough but uncontinuous space to allocate
		if(alloc){
			return NULL;
		}
		else{
			if(!goal){
				return NULL;
			}
			else{
				goto DO_CHUNK_SPLIT;
			}
		}
	}
DO_CHUNK_SPLIT:
	remove_node(chain, backup);
	chain->pool_member--;
		
	p = chunk_split(backup, size);
	//reinsert the node to chain
	add_chunk2NP_fast(chain, backup);
		
	return p;
}

static link_mem_chunk chunk_split(link_mem_chunk p, size_t size)
{
	link_mem_chunk res = NULL;
	res = make_mem_chunk(size);
	
	if(!res){
		mem_print("WTF? make new chunk failed(%s)\n", __location__);
		return NULL;
	}
	
	res->data_start = p->data_start + size;
	res->data_end = p->data_end;
	res->match++;
//	res->index = res->data_start;
	
	p->data_end = p->data_start + size - 1;
	p->chunk_size -= size;
	
	return res;
}

static link_mem_chunk search_normal_pool(link_intra_mempool chain, 
				   size_t size, 
				   bool alloc/*false means we may need to split a chunk for allocation*/)
{
	if(chain->head == NULL){
		mem_print("NP do not have member yet\n");
		return NULL;
	}

	link_mem_chunk p;
	int type;
	
	if(size >= chain->link->chunk_size)
		type = GO_LEFT;
	else
		type = GO_RIGHT;
		
	p = get_NP_chunk(chain, size, type, alloc);
	
	return p;
	
}

static link_mem_chunk make_empty_chunk(void)
{
	link_mem_chunk p;
	p = make_mem_chunk(0);
	if(!p)
		return NULL;
	
	p->data_start = NULL;
	p->data_end = NULL;

	return p;
}

static link_mem_chunk mempool_real_alloc(size_t size)
{
	link_mem_chunk p;
	p = make_mem_chunk(size);
	if(!p)
		return NULL;
	
	p->data_start = malloc(size);
	
	return p;
}

static link_mem_chunk mempool_alloc(link_mempool pool, size_t size)
{
//must put locks here and make sure there are in pair-wise
	if(size == 0)
		return	make_empty_chunk();

	size = size_align(size);
	
	exec_lock_fn(pool->m_ops->p_lock, (lock_entry)pool->T_POOL);
	if(size > pool->mem_left){
		exec_lock_fn(pool->m_ops->p_unlock, (lock_entry)pool->T_POOL);
		mem_print("%d out pool size(%s)\n", size, __location__);
		goto FAIL_ALLOC;
	}
	
	link_mem_chunk p;

//check if we could alloc in real memory!	
	bool real_alloc;
	int tmp_res;
	tmp_res = pool->real_mem_left - size;
	real_alloc = (tmp_res >= 0)?true:false;

//search high cach pool first!
	exec_lock_fn(pool->m_ops->c_lock, (lock_entry)pool->high_cache_pool->T_CHAIN);
	p = search_high_cache_pool(pool->high_cache_pool, size);
	exec_lock_fn(pool->m_ops->c_unlock, (lock_entry)pool->high_cache_pool->T_CHAIN);

	if(p){
		mem_print("alloc from HP\n");
		goto DONE_ALLOC;
	}
	
//nothing found in HP... we need to tell if we could do allocation in real memory space in advance	
	exec_lock_fn(pool->m_ops->c_lock, (lock_entry)pool->normal_pool->T_CHAIN);
	p = search_normal_pool(pool->normal_pool, size, real_alloc);
	exec_lock_fn(pool->m_ops->c_unlock, (lock_entry)pool->normal_pool->T_CHAIN);
	
	if(p){
		mem_print("alloc from NP!\n");
		goto DONE_ALLOC;
	}
	else{
		if(!real_alloc){
			mem_print("pool_size left: %d size:%d failed in alloc(%s)\n", 
				pool->real_mem_left, size, __location__);
			exec_lock_fn(pool->m_ops->p_unlock, (lock_entry)pool->T_POOL);
			goto FAIL_ALLOC;
		}
	}
	
//nothing found in normal pool, but luck we have space in real memory space
	p = make_mem_chunk(size);
	if(!p){
		exec_lock_fn(pool->m_ops->p_unlock, (lock_entry)pool->T_POOL);
		return NULL;
	}

	mem_print("alloc from real memory!\n");
	
	p->data_start = pool->real_mem;
	p->data_end = pool->real_mem + size - 1;
//	p->index = p->data_start;
	
	pool->real_mem += size;
	pool->real_mem_left -= size;
	
DONE_ALLOC:
//mem_print("mem left: %d\twill alloc size: %d\n", pool->mem_left, size);
	pool->mem_left -= p->chunk_size;
	exec_lock_fn(pool->m_ops->p_unlock, (lock_entry)pool->T_POOL);
	
	return p;

FAIL_ALLOC:
	return mempool_real_alloc(size);
}

static void mem_chunk_free(link_mempool pool, link_mem_chunk chunk)
{
//0 size chunk
	if(chunk->chunk_size == 0){
		free(chunk);
		chunk = NULL;
		return;
	}
//chunk alloc by real alloc methold	
	if(chunk->data_end == NULL){
		free(chunk->data_start);
		chunk->data_start = NULL;
		free(chunk);
		chunk = NULL;
		return;
	}
//chunk alloc by our mempool
	bool free_flag;
	size_t size = chunk->chunk_size;

//check if we need to put the chunk back into high cache pool first
	free_flag = free2HP(pool, chunk);
	if(free_flag){
//		mem_print("done free to HP\n");
		goto DONE_FREE;
	}

//the chunk is not fittable to high cache pool, then put it back to normal pool
	free2NP(pool, chunk);
		
DONE_FREE:
	exec_lock_fn(pool->m_ops->p_lock, (lock_entry)pool->T_POOL);
	pool->mem_left += size;
	exec_lock_fn(pool->m_ops->p_unlock, (lock_entry)pool->T_POOL);
}

static void free2NP(link_mempool pool, link_mem_chunk node)
{
	exec_lock_fn(pool->m_ops->c_lock, (lock_entry)pool->normal_pool->T_CHAIN);
	add_chunk2NP(pool->normal_pool, node);
	exec_lock_fn(pool->m_ops->c_unlock, (lock_entry)pool->normal_pool->T_CHAIN);
}

static void node_insert(link_intra_mempool chain, link_mem_chunk point, link_mem_chunk node)
{
	if(point->next == NULL){	//it is the tail
		point->next = node;
		node->prev = point;
		node->next = NULL;
		
		chain->tail = node;
	}
	else{
		node->next = point->next;
		node->prev = point;
		point->next->prev = node;
		point->next = node;
		
		//move the current point of the chain(not to head!)
		if(point->prev != NULL){
			chain->link = node;
		}
	}
}

static void insert2chain(link_intra_mempool chain, link_mem_chunk node, int direct, int base)
{
//mem_print("========>%s(direct: %d)\n", __FUNCTION__, direct);

	link_mem_chunk p;
	p = chain->link;
	
	int cp1, cp2;

//base: 0 - T_HP, 1 - T_NP 
	if(direct == GO_RIGHT){	//find smaller value
		while(p){
			cp1 = base?p->chunk_size:p->match;
			cp2 = base?node->chunk_size:node->match;
			
			if(cp1 - cp2 == 0){	//just insert
				node_insert(chain, p, node);	//just insert the node after the point p
				return;
			}
			else if(cp1 - cp2 < 0){
				node_insert(chain, p->prev, node);
				return;
			}
			else{
				p = p->next;
			}
		}
		if(!p){	//nothing found, then add at the tail
			node_insert(chain, chain->tail, node);
		}
	}
	else{	//find bigger value
		while(p){
			cp1 = base?p->chunk_size:p->match;
			cp2 = base?node->chunk_size:node->match;
			
			if(cp1 - cp2 >= 0){
				node_insert(chain, p, node);
				return;
			}
			else{
				p = p->prev;
			}
		}
		if(!p){
			//we need to insert the node in front of the head!
			node->next = chain->head;
			node->prev = NULL;
			chain->head->prev = node;
			chain->head = node;
		//	node_insert(chain, chain->head, node);
		}
	}
}

//must lock before excute this func
static void add_chunk2HP(link_intra_mempool chain, link_mem_chunk node)
{
//if the link is not init.
	if(!chain->head){
		init_chain(chain, node);
		
		return;
	}
	
	int direction;
	
	if(node->match - chain->link->match >= 0)
		direction = GO_LEFT;
	else
		direction = GO_RIGHT;
		
	insert2chain(chain, node, direction, T_HP);
	
	chain->pool_member++;
}

static bool free2HP(link_mempool pool, link_mem_chunk p)
{
//mem_print("=======>%s\n", __FUNCTION__);
	exec_lock_fn(pool->m_ops->c_lock, (lock_entry)pool->high_cache_pool->T_CHAIN);
	
	if(pool->high_cache_pool->head == NULL){
	//hp has no member, so just insert the node to hp
		add_chunk2HP(pool->high_cache_pool, p);
		exec_lock_fn(pool->m_ops->c_unlock, (lock_entry)pool->high_cache_pool->T_CHAIN);

		return true;
	}

	if(pool->high_cache_pool->pool_member < MAX_HP_MEMBER){	//have space in hp to add a new chunk
//TODO: Is link structrue the best for hp?
		add_chunk2HP(pool->high_cache_pool, p);
		exec_lock_fn(pool->m_ops->c_unlock, (lock_entry)pool->high_cache_pool->T_CHAIN);
		
		return true;
	}
	else{	//HP pool is full...
		if(p->match <= pool->high_cache_pool->tail->match){	//the chunk is not fittble for hp
			exec_lock_fn(pool->m_ops->c_unlock, (lock_entry)pool->high_cache_pool->T_CHAIN);
			return false;
		}
	//do not have space in hp, so do node exchange
		mem_print("free to HP by exchange!\n");

		link_mem_chunk tmp_HP;
		tmp_HP = pool->high_cache_pool->tail;
		
		remove_node(pool->high_cache_pool, pool->high_cache_pool->tail);	//take away the tail first
		pool->high_cache_pool->pool_member--;
		
		add_chunk2HP(pool->high_cache_pool, p);	//then add the new chunk to HP
		exec_lock_fn(pool->m_ops->c_unlock, (lock_entry)pool->high_cache_pool->T_CHAIN);
		
		//last add the removed hp chunk to np
		exec_lock_fn(pool->m_ops->c_lock, (lock_entry)pool->normal_pool->T_CHAIN);
		add_chunk2NP(pool->normal_pool, tmp_HP);
		exec_lock_fn(pool->m_ops->c_unlock, (lock_entry)pool->normal_pool->T_CHAIN);
		
		return true;
	}
}

static void init_chain(link_intra_mempool chain, link_mem_chunk node)
{
	node->next = NULL;
	node->prev = NULL;
		
	chain->head = node;
	chain->tail = node;
	chain->link = node;
		
	chain->pool_member++;
}

static link_mem_chunk find_buddy_chunk(link_intra_mempool chain, link_mem_chunk p)
{
	link_mem_chunk cur, head_t;
	cur = chain->head;
	head_t = chain->head;
	
	while(cur){
		if((cur->data_start - 1 == p->data_end) ||
			(cur->data_end + 1 == p->data_start))
			break;	//find my buddy :)
			
		cur = cur->next;
	}
	
	chain->head = head_t;
	return cur;
}

static void add_chunk2NP(link_intra_mempool chain, link_mem_chunk p)
{
	if(chain->head == NULL){	//if it is not init.
		init_chain(chain, p);
		return;
	}
	
//find buddy chunk---try best to make memory space continuous
	link_mem_chunk buddy = NULL;
	buddy = find_buddy_chunk(chain, p);
	if(buddy){
	//remove buddy chunk first
		remove_node(chain, buddy);
		chain->pool_member--;
	//do merge
		if(buddy->data_start - 1 == p->data_end){
			buddy->data_start = p->data_start;
//			buddy->index = buddy->data_start;
		}
		else
			buddy->data_end = p->data_end;
		
		buddy->chunk_size += p->chunk_size;		
		
		buddy->match = (p->match + buddy->match)/2;	//if we need to split, split the chunk with large size. yeah, u know what i am doing here....
		
	//free the merged chunk	
		free(p);
		p = NULL;
	//reinsert buddy chunk to chain
		add_chunk2NP(chain, buddy);
		
		if(!p)
			return;
	}
//do not find buddy chunk, then just insert the node based on size
	add_chunk2NP_fast(chain, p);
	return;
}

static void add_chunk2NP_fast(link_intra_mempool chain, link_mem_chunk node)
{
    	if(chain->head == NULL){
		init_chain(chain, node);
		return;
	}

	int direction;
	
	if(node->chunk_size >= chain->link->chunk_size)
		direction = GO_LEFT;
	else
		direction = GO_RIGHT;
		
	insert2chain(chain, node, direction, T_NP);
	
	chain->pool_member++;
}

static void freeChunk(link_mem_chunk p)
{
	p->data_start = NULL;
	p->data_end = NULL;
	p->next = NULL;
	p->prev = NULL;
	
	free(p);
	return;
}

static void freeChain(link_intra_mempool chain, mempool_lock_ops lock_ops)
{
	link_mem_chunk p, tmp;
	p = chain->head;
	
	while(p){
		tmp = p->next;
		freeChunk(p);
		p = tmp;
	}
	
	exec_lock_fn(lock_ops->c_destory, (lock_entry)chain->T_CHAIN);

	free(chain);
	chain = NULL;
}

static void destory_pool(link_mempool pool)
{
//free chain first
	freeChain(pool->high_cache_pool, pool->m_ops);
	freeChain(pool->normal_pool, pool->m_ops);
	
//then free real memory area
	munmap(pool->real_mem_h, pool->mem_size);
	pool->real_mem = NULL;
	pool->real_mem_h = NULL;
	
//free the last part
	close(pool->mem_fd);
	unlink(pool->mem_name);
	
	free(pool->mem_name);
	pool->mem_name = NULL;
	
	exec_lock_fn(pool->m_ops->p_destory, (lock_entry)pool->T_POOL);

	free(pool);
	pool = NULL;
}

static mempool link_mempool_allocator(size_t pool_size, bool pool_lock,
			mempool_create_fn pool_create_fn,
			mempool_alloc_fn chunk_alloc_fn,
			mempool_chunk_free_fn chunk_free_fn,
			mempool_pool_free_fn pool_free_fn,
			mempool_priv_alloc_fn priv_alloc)
{	
	mempool p = NULL;
	
	if(!p){
		p = (mempool)priv_alloc(sizeof(struct mempool_class));
		if(!p)
			return NULL;
	}
	
	p->m_pool = (link_mempool)pool_create_fn(pool_size);
	if(p->m_pool == NULL)
		return NULL;
		
	p->m_func.pool_create = pool_create_fn;
	p->m_func.chunk_alloc = chunk_alloc_fn;
	p->m_func.chunk_free = chunk_free_fn;
	p->m_func.pool_free = pool_free_fn;
	
	link_mempool tmp = (link_mempool)p->m_pool;
	tmp->m_ops = lock_init(pool_lock);
	
	//init lock entry first
	exec_lock_fn(tmp->m_ops->e_lock_init, tmp->mem_name, &(tmp->T_POOL), 0);
	exec_lock_fn(tmp->m_ops->e_lock_init, tmp->mem_name, &(tmp->high_cache_pool->T_CHAIN), 1);
	exec_lock_fn(tmp->m_ops->e_lock_init, tmp->mem_name, &(tmp->normal_pool->T_CHAIN), 2);
	
	//init lock
	exec_lock_fn(tmp->m_ops->p_lock_init, (lock_entry)tmp->T_POOL);
	exec_lock_fn(tmp->m_ops->c_lock_init, (lock_entry)tmp->high_cache_pool->T_CHAIN);
	exec_lock_fn(tmp->m_ops->c_lock_init, (lock_entry)tmp->normal_pool->T_CHAIN);
	
	return p;
}

mempool link_mempool_init(size_t pool_size, bool pool_lock,
			mempool_create_fn pool_create_fn,
			mempool_alloc_fn chunk_alloc_fn,
			mempool_chunk_free_fn chunk_free_fn,
			mempool_pool_free_fn pool_free_fn)
{
	if(!pool_create_fn) pool_create_fn = pool_create;
	if(!chunk_alloc_fn) chunk_alloc_fn = mempool_alloc;
	if(!chunk_free_fn)	chunk_free_fn = mem_chunk_free;
	if(!pool_free_fn)	pool_free_fn = destory_pool;
	
	return link_mempool_allocator(pool_size, pool_lock, pool_create_fn, chunk_alloc_fn, 
								  chunk_free_fn, pool_free_fn, link_priv_alloc);
}

