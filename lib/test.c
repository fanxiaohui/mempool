#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mempool.h"
#include "link_mempool.h"

#define nthread 100

static void cal_run_time(struct timeval start, struct timeval end)
{
	double time_slot;
	time_slot = end.tv_sec - start.tv_sec + ((double)end.tv_usec/1000000 - (double)start.tv_usec/1000000);

	printf("time coast: %lf\n", time_slot);
	return;
}

static void *run_thread(void *arg)
{
	mempool pool = (mempool)arg;
	link_mem_chunk t_data;
	char test_str[] = "test";

	int i;
	for(i=0; i<1000; i++){
		t_data = pool->m_func.chunk_alloc((link_mempool)pool->m_pool, 10);
		if(!t_data){
			printf("failed on thread test!\n");
			exit(1);
		}
		//sanity test for thread
		memcpy(t_data->data_start, test_str, strlen(test_str));
		if(memcmp(t_data->data_start, test_str, strlen(test_str))){
			printf("failed on thread test 2!\n");
			exit(1);
		}

		pool->m_func.chunk_free((link_mempool)pool->m_pool, t_data);
	}
	
	printf("thread(%d) passed the test<=========\n\n", pthread_self());
	return;
}

static void thread_test(mempool pool)
{
	printf("============>%s\n", __FUNCTION__);

	pthread_t tid[nthread];

	int i;
	for(i=0; i<nthread; i++){
		if(pthread_create(&tid[i], 0, run_thread, (void *)pool) < 0){
			perror("pthread_create");
			exit(1);
		}
	}
	for(i=0; i<nthread; i++)
		pthread_join(tid[i], NULL);

	printf("%s<============\n\n", __FUNCTION__);
}

static void stress_test(mempool pool, int round, size_t size)
{
	printf("=======>%s\n", __FUNCTION__);

	struct timeval time_s, time_e;
	int i;
	link_mem_chunk t_data;
	char *t_data1;
	
	gettimeofday(&time_s, NULL);
	
	size_t alloc_size;

	for(i=0;i<round;i++){
		alloc_size = size*i;

		t_data = pool->m_func.chunk_alloc((link_mempool)pool->m_pool, alloc_size);
		if(!t_data){
			printf("test failed!\n");
			break;
		}
		pool->m_func.chunk_free((link_mempool)pool->m_pool, t_data);
	}
	
	gettimeofday(&time_e, NULL);
	cal_run_time(time_s, time_e);

	printf("\n###########################\n\n");

	gettimeofday(&time_s, NULL);
	
	for(i=0;i<round;i++){
		alloc_size = size*i;
		t_data1 = (char *)malloc(alloc_size*sizeof(char));
		if(!t_data1){
			perror("malloc");
			break;
		}
		free(t_data1);
	}
	
	gettimeofday(&time_e, NULL);
	cal_run_time(time_s, time_e);

	printf("pass %s<===========\n\n", __FUNCTION__);
}

static void sanity_test(mempool pool, const char *test_str)
{	
	printf("==========>%s\n", __FUNCTION__);

	link_mempool tmp = (link_mempool)pool->m_pool;
	size_t org_size = tmp->mem_left;

	link_mem_chunk data = NULL;
	data = pool->m_func.chunk_alloc(tmp, strlen(test_str)+5);
	if(data == NULL){
		printf("sanity test failed!\n");	
		exit(1);
	}
	printf("done data alloc!\n");

	memcpy(data->data_start, test_str, strlen(test_str));
	if(memcmp(data->data_start, test_str, strlen(test_str))){
		printf("sanity test failed!\n");
		exit(1);
	}
	printf("%s\n", data->data_start);

//	dump_mem_chunk(data, -1);

//	dump_pool(pool, true, true);

	pool->m_func.chunk_free(tmp, data);
	printf("done data free!\n");

	if(org_size != tmp->mem_left){
		printf("sanity test failed!\n");
		exit(1);
	}

	printf("pass %s<===============\n\n", __FUNCTION__);
}

static void sim_test(mempool pool, int round, size_t base)
{
	size_t alloc_size;
	int i;
	link_mem_chunk p;

	round = round;

	for(i=0; i<round; i++){
		alloc_size = i*20 + base;
		
		p = pool->m_func.chunk_alloc((link_mempool)pool->m_pool, alloc_size);

		if(!p){
			printf("alloc mem failed in sim test(size: %d)!\n", size_align(alloc_size));
//			mempool_map(pool);
			exit(1);
		}
//		printf("%d alloc done\n", i);

		if(i%2 == 0){
//			printf("%d will be free\n", i);
			pool->m_func.chunk_free((link_mempool)pool->m_pool, p);
	
		//	mempool_map(pool);
		}
	}

//	mempool_map(pool);
	printf("pass sime_test\n");
}

int main(int argc, char *argv[])
{
	extern char *optarg;
	int opt;
	bool test_p = true;

	int roundnum = -1;
	size_t size_base = 0;
	size_t pool_size = 0;

	while(argc > 1 && (*argv[1] != '-')){
		argv++;
		argc--;
	}
	while(EOF != (opt = getopt(argc, argv, "n:s:b:r:")))
		switch(opt){
		case 's':
			pool_size = (size_t)atoi(optarg);
			break;

		case 'b':
			size_base = (size_t)atoi(optarg);
			break;

		case 'r':
			roundnum = atoi(optarg);
			break;

		default:
			printf("wrong argv!\n");
			exit(1);
		}

	if(roundnum == -1){
		printf("need round num for stress test(-r)!\n");
		test_p = false;
	}
	if(pool_size == 0){
		printf("need pool size(-s)!\n");
		test_p = false;
	}
	if(size_base == 0){
		printf("need base size for stress test(-b)!\n");
		test_p = false;
	}
	if(!test_p)
		exit(1);
	
	printf("test pram:\n#pool size: %d\n#round num:%d\t#base size:%d\n\n", 
		pool_size, roundnum, size_base);

//	init_log();

	mempool test_pool = NULL;
	test_pool = mempool_init(pool_size, 2/*thread share type*/, NULL, NULL, NULL);
	if(!test_pool){
		printf("failed on pool create!\n");
		exit(1);	
	}
	printf("done pool create!\n");

	//sanity test
	char test_str[] = "hello world!";
	sanity_test(test_pool, test_str);
//	mempool_map(test_pool);

	//stress test
	stress_test(test_pool, roundnum, size_base);

	//thread safe test
	thread_test(test_pool);

	//simulation test
	sim_test(test_pool, roundnum, size_base);

	test_pool->m_func.pool_free(test_pool->m_pool);
	printf("done pool distroy!\n");
	
	return 0;
}

