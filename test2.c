#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

int cur_index = 10000;

#define LOG_DEBUG printf
#define LOG_ERR printf

typedef struct PROTO_HEAD
{
	uint32_t size;
	char data[0];
} proto_head;

typedef struct SHM_IPC_OBJ
{
	uint32_t size;
	uint32_t read;
	uint32_t write;
//	void *mem;
//	void *end;
//	int shmid;
//	void *read_ptr;
//	void *write_ptr;
	char data[0];
} shm_ipc_obj;
#define SHM_DATA_OFFSET sizeof(struct SHM_IPC_OBJ)
#define READ_DATA(obj) (proto_head *)(obj->data + obj->read)
#define WRITE_DATA(obj) (proto_head *)(obj->data + obj->write)

shm_ipc_obj *init_shm_ipc_obj(int key, int size, bool create)
{
	int flag = 0666;
	if (create)
	{
		flag |= IPC_CREAT|IPC_EXCL;
	}
	int shmid = shmget(key, size, flag);
	shm_ipc_obj *ret;
	ret = shmat(shmid, NULL, 0);
	if (ret == (void *)-1) {
		LOG_ERR("%s %d: shmat fail[%d]\n", __FUNCTION__, __LINE__, errno);
		return NULL;
	}

	if (create)
	{
		ret->size = size;
		ret->read = ret->write = 0;
//		ret->mem = (void *)(ret->data);
//		ret->read_ptr = ret->write_ptr = ret->mem;
//		ret->end = ret->mem + size;
	}
	
	return (ret);
}

void rm_shm_ipc_obj(int shmid)
{
	shmctl(shmid, IPC_RMID, NULL);	
}

proto_head *read_from_shm_ipc(shm_ipc_obj *obj)
{
	if (obj->read == obj->write)
		return NULL;
	
	proto_head *head = READ_DATA(obj);
	obj->read += head->size;
	return head;
}

void try_read_reset(shm_ipc_obj *obj)
{
	uint32_t t = obj->read;
	if (__atomic_compare_exchange_n(&obj->write, &t, 0, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
	{
		obj->read = 0;
		LOG_DEBUG("reset read and write, curindex = %d\n", cur_index);
	}
}

void shm_ipc_move(shm_ipc_obj *obj, proto_head *head)
{
	int *p = (int *)&head->data[0];
	LOG_DEBUG("%s: write = %u, read = %u, size = %u, data = %d\n", __FUNCTION__, obj->write, obj->read, head->size, *p);
	
	assert(obj->read == 0);
	memmove(obj->data, head, head->size);
	__atomic_store_n(&obj->write, head->size, __ATOMIC_SEQ_CST);
}

int shm_ipc_obj_avaliable_size(shm_ipc_obj *obj)
{
	return obj->size - obj->write - SHM_DATA_OFFSET;
}

int write_to_shm_ipc(shm_ipc_obj *obj, proto_head *head)
{
	assert(shm_ipc_obj_avaliable_size(obj) >= head->size);

		//如果被重置了，那么要做memmove
		//如果没有重置，随时可能被重置
//	void *addr1 = (void *)head + head->size;  //未重置地址
//	void *addr2 = obj->mem + head->size;   //重置地址
	uint32_t addr1 = obj->write + head->size;
	uint32_t addr2 = head->size;
	for (;;)
	{
//		void *t1 = head;
//		void *t2 = obj->mem;
		uint32_t t1 = (void *)head - (void *)obj - SHM_DATA_OFFSET;
		uint32_t t2 = 0;
		if (__atomic_compare_exchange_n(&obj->write, &t1, addr1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		{
			return (0);
		}
//		else if (__atomic_compare_exchange_n(&obj->write, &t2, addr2, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		else
		{
			assert(obj->write == 0);
//			assert(obj->read == 0);
			shm_ipc_move(obj, head);
			return (0);
		}
//		else
//		{
//			LOG_ERR("%s: %d  it should be a bug\n", __FUNCTION__, __LINE__);
//		}
			
	}
	return (0);
}


int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		int shmid = shmget(0x100200, 1024, 0666);
		void *mem = shmat(shmid, NULL, 0);
		if (mem == (void *)-1) {
			printf("%s %d: shmat fail[%d]\n", __FUNCTION__, __LINE__, errno);
			return (0);
		}
		uint64_t *p = mem;
		printf("finished, result = %lu\n", *p);
		shmctl(shmid, IPC_RMID, NULL);
		return (0);
	}

	int num = atoi(argv[1]);
	int create = atoi(argv[2]);
//	int atomic = atoi(argv[3]);
	int write = atoi(argv[4]);
	
	shm_ipc_obj *G = init_shm_ipc_obj(0x100200, 0x1000, create);
	if (!G)
	{
		return (0);
	}
	proto_head *head;

	for (;;)
	{
		for (int i = 0; i < num; ++i)
		{
			if (write)
			{
				head = WRITE_DATA(G);
				if (shm_ipc_obj_avaliable_size(G) < 0x100)
				{
					usleep(1);
//					LOG_DEBUG("full, cannot write\n");
					continue;
				}
//				LOG_DEBUG("write %d, w[%d]r[%d] head[%p]\n", cur_index, G->write, G->read, head);
				
				head->size = 0x100;
				int *p = (int *)&head->data[0];
				*p = cur_index++;
				write_to_shm_ipc(G, head);
			}
			else
			{
				head = read_from_shm_ipc(G);
				if (!head)
				{
					usleep(1);
//					LOG_DEBUG("empty, cannot read\n");					
					continue;
				}
//				LOG_DEBUG("read %d, w[%d]r[%d] head[%p]\n", cur_index, G->write, G->read, head);
				
				int *p = (int *)&head->data[0];
				if (*p != cur_index)
				{
					LOG_ERR("BUG, *p[%d] cur_index[%d], read %u write %u\n", *p, cur_index, G->read, G->write);
					return (-1);
				}
				++cur_index;
				try_read_reset(G);
			}
		}
		int t = rand() % 100;
		usleep(t);
	}

/*
	for (int i = 0; i < 3; ++i)
	{
		head = WRITE_DATA(G);
		head->size = 0x100;
		write_to_shm_ipc(G, head);		
	}

	for (int i = 0; i < 5; ++i)
	{
		head = read_from_shm_ipc(G);
	}

	for (int i = 0; i < 3; ++i)
	{
		head = WRITE_DATA(G);
		head->size = 0x100;
		write_to_shm_ipc(G, head);		
	}
*/

	return (0);
}
