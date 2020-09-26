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

void reader(sem_t* WriteMutex, sem_t* CountMutex, int i, int times) {
	int ret = 0;
	int Rcount;
	while(times != 0) {
		times--;
		ret = sem_wait(CountMutex);
		if(ret == -1)
			return;
		sleep(RAND);
		read(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		if(Rcount == 0) {
			ret = sem_wait(WriteMutex);
			if(ret == -1)
				return;
			sleep(RAND);
		}
		Rcount++;
		write(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		ret = sem_post(CountMutex);
		if(ret == -1)
			return;
		printf("Reader %d: read, total %d reader\n", i, Rcount);
		sleep(RAND);
		ret = sem_wait(CountMutex);
		if(ret == -1)
			return;
		sleep(RAND);
		read(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		Rcount--;
		write(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		if(Rcount == 0) {
			ret = sem_post(WriteMutex);
			if(ret == -1)
				return;
			sleep(RAND);
		}
		ret = sem_post(CountMutex);
		if(ret == -1)
			return;
		sleep(RAND);
	}
}


void writer(sem_t* WriteMutex, sem_t* CountMutex, int i, int times) {
	int ret = 0;
	while(times != 0) {
		times--;
		ret = sem_wait(WriteMutex);
		if(ret == -1)
			return;
		sleep(RAND);
		printf("Writer %d: write\n", i);
		sleep(RAND);
		ret = sem_post(WriteMutex);
		if(ret == -1)
			return;
		sleep(RAND);
	}
}


int main(void) {
	// TODO in lab4
	printf("reader_writer\n");
	int ret = 0;
	int id = -1;
	sem_t WriteMutex;
	sem_t CountMutex;
	int Rcount = 0;

	write(SH_MEM, (uint8_t *)&Rcount, 4, 0);
	ret = sem_init(&WriteMutex, 1);
	if (ret == -1) {
		printf("Father Process: Semaphore Initializing Failed.\n");
		exit();
	}
	ret = sem_init(&CountMutex, 1);
	if (ret == -1) {
		printf("Father Process: Semaphore Initializing Failed.\n");
		sem_destroy(&WriteMutex);
		exit();
	}

	for(int i = 1; i <= 6; i++){
		ret = fork();
		if(ret == 0) {
			if(i <= 3){
				// writer
				id = getpid();
				writer(&WriteMutex, &CountMutex, id-2, 2);
				exit();
			}
			else{
				// reader
				id = getpid();
				reader(&WriteMutex, &CountMutex, id-5, 2);
				if(i == 6) {
					sem_destroy(&WriteMutex);
					sem_destroy(&CountMutex);
				}
				exit();
			}
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
