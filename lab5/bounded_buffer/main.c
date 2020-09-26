#include "lib.h"
#include "types.h"

// #define RAND 128
#define RAND (int)rng()


static unsigned int seed = 1;
static uint32_t rng() {
	seed *= 654321;
	seed += 123456;
	uint32_t ret = (seed & 0xff) + ((seed >> 8) & 0xff);
	return ret;
}


void Remove(sem_t* mutex, sem_t* fullBuffers, sem_t* emptyBuffers, int i, int times){
	while(times != 0) {
		times--;
		sem_wait(fullBuffers);
		sleep(RAND);
		sem_wait(mutex);
		sleep(RAND);
		printf("Consumer : consume\n");
		sleep(RAND);
		sem_post(mutex);
		sleep(RAND);
		sem_post(emptyBuffers);
		sleep(RAND);
	}
}


void Deposit(sem_t* mutex, sem_t* fullBuffers, sem_t* emptyBuffers, int i, int times){
	int ret = 0;
	while(times != 0) {
		times--;
		ret = sem_wait(emptyBuffers);
		if(ret == -1)
			return;
		sleep(RAND);
		ret = sem_wait(mutex);
		if(ret == -1)
			return;
		sleep(RAND);
		printf("Producer %d: produce\n", i);
		sleep(RAND);
		if(ret == -1)
			return;
		ret = sem_post(mutex);
		if(ret == -1)
			return;
		sleep(RAND);
		ret = sem_post(fullBuffers);
		if(ret == -1)
			return;
		sleep(RAND);
	}
}


int main(void) {
	// TODO in lab4
	printf("bounded_buffer\n");
	int ret = 0;
	int id = -1;
	sem_t mutex;
	sem_t fullBuffers;
	sem_t emptyBuffers;
	// printf("Father Process: Semaphore Initializing.\n");
	ret = sem_init(&mutex, 1);
	if (ret == -1) {
		printf("Father Process: Semaphore Initializing Failed.\n");
		exit();
	}
	ret = sem_init(&fullBuffers, 0);
	if (ret == -1) {
		printf("Father Process: Semaphore Initializing Failed.\n");
		sem_destroy(&mutex);
		exit();
	}
	ret = sem_init(&emptyBuffers, 3);
	if (ret == -1) {
		printf("Father Process: Semaphore Initializing Failed.\n");
		sem_destroy(&mutex);
		sem_destroy(&fullBuffers);
		exit();
	}


	for(int i = 1; i <= 5; i++) {
		ret = fork();
		if(ret == 0) {
			if(i == 1){
				// consumer
				id = getpid();
				Remove(&mutex, &fullBuffers, &emptyBuffers, id-2, 10);
				sem_destroy(&mutex);
				sem_destroy(&fullBuffers);
				sem_destroy(&emptyBuffers);
				exit();
			}
			else {
				// producer	
				id = getpid();
				Deposit(&mutex, &fullBuffers, &emptyBuffers, id-3, 3);
				exit();
			}
		}
		else if(ret != -1) {
			continue;
		}
		else {
			printf("Father Process: Fork failed.\n");
			exit();
		}
	}

	exit();
	return 0;
}
