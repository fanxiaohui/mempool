#include <pthread.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#include "lock.h"

static void pool_lock_init(lock_entity);
static void pool_lock(lock_entity);
static void pool_unlock(lock_entity);
static void pool_lock_free(lock_entity);
static void chain_lock_init(lock_entity);
static void chain_lock(lock_entity);
static void chain_unlock(lock_entity);
static void chain_lock_free(lock_entity);

static void lock_entity_init(char *, void **, int );

static void pool_lock_init(lock_entity lock_t)
{
/* Set pthread_mutex_attr to process shared */
	pthread_mutexattr_t p_attr; 
	if(pthread_mutexattr_init(&p_attr)){
		perror("pthread_mutexattr_init");
		exit(1);
	}
	
	if(pthread_mutexattr_setpshared(&p_attr, PTHREAD_PROCESS_SHARED)){
		perror("pthread_mutexattr_setpshared");
		exit(1);
	}
	
	if(pthread_mutex_init((pthread_mutex_t *)lock_t->lock_addr, &p_attr)){
		perror("pthread_mutex_init");
		exit(1);
	}
}

static void pool_lock(lock_entity lock_t)
{
	if(pthread_mutex_lock((pthread_mutex_t *)lock_t->lock_addr)){
		perror("pthread_mutex_lock");
		pthread_exit(NULL);
	}
}

static void pool_unlock(lock_entity lock_t)
{
	if(pthread_mutex_unlock((pthread_mutex_t *)lock_t->lock_addr)){
		perror("pthread_mutex_unlock");
		pthread_exit(NULL);
	}
}

static void pool_lock_free(lock_entity lock_t)
{
	if(pthread_mutex_destroy((pthread_mutex_t *)lock_t->lock_addr)){
		perror("pthread_mutex_destroy");
		exit(1);
	}
	
	munmap(lock_t->lock_addr, sizeof(pthread_mutex_t));
	shm_unlink(lock_t->lock_name);

	free(lock_t);
	lock_t = NULL;
}

static void chain_lock_init(lock_entity lock_t)
{
	if(pthread_spin_init((pthread_spinlock_t *)lock_t->lock_addr, PTHREAD_PROCESS_SHARED)){
		perror("pthread_spin_init");
		exit(1);
	}
}

static void chain_lock(lock_entity lock_t)
{
	if(pthread_spin_lock((pthread_spinlock_t *)lock_t->lock_addr)){
		perror("pthread_spin_lock");
		pthread_exit(NULL);
	}
}

static void chain_unlock(lock_entity lock_t)
{
	if(pthread_spin_unlock((pthread_spinlock_t *)lock_t->lock_addr)){
		perror("pthread_spin_unlock");
		pthread_exit(NULL);
	}
}

static void chain_lock_free(lock_entity lock_t)
{
	if(pthread_spin_destroy((pthread_spinlock_t *)lock_t->lock_addr)){
		perror("pthread_spin_destroy");
		exit(1);
	}
	
	munmap(lock_t->lock_addr, sizeof(pthread_spinlock_t));
	shm_unlink(lock_t->lock_name);

	free(lock_t);
	lock_t = NULL;
}

static void lock_entity_init(char *lock_name, void **lock_t, int nnum)
{
#define POOL_NUM	0
#define HP_NUM		1
#define NP_NUM		2

	if(lock_name == NULL){
		printf("should init lock name\n");
		exit(1);
	}

	if(nnum < 0 || nnum >= MEM_LOCK_NUM){
		printf("invalid parameter\n");
		exit(1);
	}

	*lock_t = (lock_entity)malloc(sizeof(struct mem_lock_t));
	lock_entity p = *lock_t;

	p->lock_name = (char *)calloc(128, sizeof(char));
	sprintf(p->lock_name, "%s.%d", lock_name+4, nnum);
	
	int lock_fd;
	lock_fd = shm_open(p->lock_name, O_CREAT|O_RDWR|O_EXCL, 0644);
	if(lock_fd == -1){
		perror("shm_open");
		exit(1);
	}

	switch(nnum){
	case POOL_NUM:
		if(ftruncate(lock_fd, sizeof(pthread_mutex_t))){
			perror("ftruncate");
			exit(1);
		}
		p->lock_addr = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ|PROT_WRITE,
								MAP_SHARED, lock_fd, 0);
		if(p->lock_addr == NULL){
			perror("mmap");
			exit(1);
		}
		break;
	case HP_NUM:
	case NP_NUM:
		if(ftruncate(lock_fd, sizeof(pthread_spinlock_t))){
			perror("ftruncate");
			exit(1);
		}
		p->lock_addr = mmap(NULL, sizeof(pthread_spinlock_t), PROT_READ|PROT_WRITE,
								MAP_SHARED, lock_fd, 0);
		if(p->lock_addr == NULL){
			perror("mmap");
			exit(1);
		}
		break;
	default:
		break;
	}
	
#undef POOL_NUM	
#undef HP_NUM	
#undef NP_NUM	
}

struct lock_ops *lock_init(bool use_lock)
{
	struct lock_ops *ops_t;
	ops_t = (struct lock_ops *)calloc(1, sizeof(struct lock_ops));

	if(use_lock){	//zero means no lock using
	//init lock
		ops_t->e_lock_init = lock_entity_init;
		ops_t->p_lock_init = pool_lock_init;
		ops_t->c_lock_init = chain_lock_init;
	//lock
		ops_t->p_lock = pool_lock;
		ops_t->c_lock = chain_lock;
	//unlock
		ops_t->p_unlock = pool_unlock;
		ops_t->c_unlock = chain_unlock;
	//lock destory
		ops_t->p_destory = pool_lock_free;
		ops_t->c_destory = chain_lock_free;
	}
	
	return ops_t;
}

