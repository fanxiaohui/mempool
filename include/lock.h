#ifndef __MEMPOOL_LOCK_H
#define __MEMPOOL_LOCK_H

#ifndef _POSIX_THREAD_PROCESS_SHARED
#error "Not support process shared mutex"
#endif

#define MEM_LOCK_NUM	3

#ifdef __cplusplus
extern "C"{
#endif

typedef struct mem_lock_t *lock_entity;

struct mem_lock_t {
	void *lock_addr;
	char *lock_name;
};

//locking func prototype
typedef void (*lock_entity_init_fn) (char *, void **, int );
typedef void (*pool_lock_init_fn) (lock_entity);
typedef void (*chain_lock_init_fn) (lock_entity);
typedef void (*pool_lock_fn) (lock_entity);
typedef void (*chain_lock_fn) (lock_entity);
typedef void (*pool_unlock_fn) (lock_entity);
typedef void (*chain_unlock_fn) (lock_entity);
typedef void (*pool_lock_free_fn) (lock_entity);
typedef void (*chain_lock_free_fn) (lock_entity);

struct lock_ops {
	lock_entity_init_fn 	e_lock_init;
	pool_lock_init_fn	p_lock_init;
	chain_lock_init_fn	c_lock_init;
	pool_lock_fn		p_lock;
	pool_unlock_fn 	p_unlock;
	chain_lock_fn		c_lock;
	chain_unlock_fn 	c_unlock;
	pool_lock_free_fn	p_destory;
	chain_lock_free_fn	c_destory;
};

extern struct lock_ops *lock_init(bool type);

#ifdef __cplusplus
}
#endif

#endif
