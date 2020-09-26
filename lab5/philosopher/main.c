#include "lib.h"
#include "types.h"

#define N 5
// #define RAND 128
#define RAND (int)rng()


static unsigned int seed = 1;
static uint32_t rng() {
	seed *= 654321;
	seed += 123456;
	uint32_t ret = (seed & 0xff) + ((seed >> 8) & 0xff);
	return ret;
}

void philosopher(sem_t* fork, int i, int times) {
	int ret = 0;
	while(times != 0){
		times--;
		printf("Philosopher %d: think\n", i);
		sleep(RAND);
		if(i % 2 == 0){
			ret = sem_wait(&fork[i]);
			if(ret == -1)
				return;
			sleep(RAND);
			ret = sem_wait(&fork[(i+1) % N]);
			if(ret == -1)
				return;
			sleep(RAND);
		}
		else {
			ret = sem_wait(&fork[(i+1) % N]);
			if(ret == -1)
				return;
			sleep(RAND);
			ret = sem_wait(&fork[i]);
			if(ret == -1)
				return;
			sleep(RAND);
		}
		printf("Philosopher %d: eat\n", i);
		sleep(RAND);
		ret = sem_post(&fork[i]);
		if(ret == -1)
			return;
		sleep(RAND);
		ret = sem_post(&fork[(i+1) % N]);
		if(ret == -1)
			return;
		sleep(RAND);
	}
}

int main(void) {
	// TODO in lab4
	printf("philosopher\n");
	int ret = 0;
	int id = -1;
	sem_t sem[N];
	for(int i = 0; i < N; i++){
		ret = sem_init(&sem[i], 1);
		if (ret == -1) {
			printf("Father Process: Semaphore Initializing Failed.\n");
			for(int j = 0; j < i; j++){
				sem_destroy(&sem[j]);
			}
			exit();
		}
	}

	for(int i = 1; i <= N; i++){
		ret = fork();
		if(ret == 0) {
			// philosopher
			id = getpid();
			philosopher(sem, id-2, 2);
			if(i == N) {
				// destroy sem
				for(int i = 0; i < N; i++){
					sem_destroy(&sem[i]);
				}
			}
			exit();
		}
		else if(ret != -1){
			continue;
		}
		else{
			printf("Father Process: Fork failed.\n");
			exit();
		}
	}

	exit();
	return 0;
}
