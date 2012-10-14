/*
 *  cjalgorithm.m
 *  Typing
 *
 *  Created by Chris Johnson and Michael Dickens.
 *
 */

#include <limits.h>
#include "keyboard.h"

#define FILE_READ_NOT_HAPPEN -100

struct ThreadArg {
	Keyboard bestk;
	int numRounds;
	time_t start;
	
	/* Indicates that the subroutine should keep creating new threads until 
	 * this reaches 0.
	 */
	int threadCount;
	
	/* Indicates whether the current thread is done running. */
	int isFinished;
};

int runCJAlgorithm(const char *const filename);

void runThreadedAlgorithm();
void * runThreadsRec(void *arg);
void * runSingleThread(void *arg);

int64_t greatToBest(Keyboard *k);
int tryPermutations(int length, int *p, int index, int *used, int *locs, 
	Keyboard *bestk, Keyboard *origk);

int64_t anneal(Keyboard *k, int lockins[][2], size_t lockin_length);
int64_t improveLayout(int64_t evaluationToBeat, Keyboard *k, 
	int lockins[][2], size_t lockin_length);
int smartMutate(int swapIndices[][2], Keyboard *k, int numberOfSwaps);
