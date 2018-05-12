#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
int main(int argc, char *argv[])
{
//    uint64_t t1[] = {1,2,3,4,5};
// 	for (int i = 0; i < 5; ++i)
// 	{
// 		uint64_t t = __atomic_load_n(&t1[i], __ATOMIC_SEQ_CST);
// 		printf("read %lu\n", t);
// 	}
	int shmid;	
	void *mem;
	int flag = 0666;
	
	if (argc == 1)
	{
		shmid = shmget(0x100200, 1024, flag);
		mem = shmat(shmid, NULL, 0);
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
	int atomic = atoi(argv[3]);
	if (create)
	{
		flag |= IPC_CREAT|IPC_EXCL;
	}
	shmid = shmget(0x100200, 1024, flag);
	mem = shmat(shmid, NULL, 0);
	if (mem == (void *)-1) {
		printf("%s %d: shmat fail[%d]", __FUNCTION__, __LINE__, errno);
		return (0);
	}
	uint64_t *p = mem;

	if (atomic)
	{
		for (int i = 0; i < num; ++i)
		{
			__atomic_add_fetch(p, 10, __ATOMIC_SEQ_CST);
		}
	}
	else
	{
		for (int i = 0; i < num; ++i)
		{
			*p = *p + 10;
		}
	}
	printf("finished, num = %d, create = %d, pid = %d %d\n", num, create, getpid(), getppid());
    return 0;
}

