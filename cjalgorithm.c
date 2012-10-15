/*
 *  cjalgorithm.m
 *  Typing
 *
 *  Written by Chris Johnson and Michael Dickens.
 *
 */

#include "cjalgorithm.h"
 

int runCJAlgorithm(const char *const filename)
{
	time_t start = time(NULL), finish;
	int roundNum, isFileEmpty;
	
	int64_t curEval;
	int64_t bestEval = FITNESS_MAX;

	Keyboard k = nilKeyboard;
	Keyboard prevk;
	Keyboard bestk = nilKeyboard;
	
	FILE *file = fopen(filename, "r");
	CHECK_FILE_FOR_NULL(file, filename);
	
	// The simulated annealing algorithm is seeded with either a completely random 
	// layout or a mutated version of the last layout found so far. The probabilty 
	// of using a mutated last layout is chanceToUsePreviousLayout.
	double chanceToUsePreviousLayout = 0.2; /* 0.2 */
	double subChanceToUseBestLayout = 0.1; /* 0.1 */
	int numberOfSwaps = ksize / 15;
	
	int roundsBeforeChanceInc = 10;
	int roundsBeforeSwapInc = (600 / ksize > 1 ? 600 / ksize : 1);
	
	int roundOnChanceInc = roundsBeforeChanceInc, 
		roundOnSwapInc = roundsBeforeSwapInc;
	int greatToBestInterval = 64, roundsBeforeGTBIDec = 200;

	int usedPreviousLayout = FALSE;
	int intervalBetweenPrints = 60, intervalInc = 0;
	
	/* Run Chris Johnson's simulated annealing algorithm. */
	isFileEmpty = (INIT_FROM_FILE ? FALSE : TRUE);
	for (roundNum = 0; ; ++roundNum) {
		copyKeyboard(&prevk, &k);

		/* chanceToUsePreviousLayout and numberOfSwaps increase as the program 
		 * runs for longer and longer.
		 */
		if (roundNum == roundOnChanceInc) {
			chanceToUsePreviousLayout = pow(chanceToUsePreviousLayout, 0.7);
			roundsBeforeChanceInc = (int) (roundsBeforeChanceInc * 1.2) + 1;
			roundOnChanceInc += roundsBeforeChanceInc;
			if (detailedOutput)
                printf("Chance to use previous layout is now %f.\n",
                       chanceToUsePreviousLayout);
		}
		
		if (roundNum == roundOnSwapInc) {
			++numberOfSwaps;
			roundsBeforeSwapInc = (int) (roundsBeforeSwapInc * 1.1) + 1;
			roundOnSwapInc += roundsBeforeSwapInc;
			if (detailedOutput)
                printf("Number of swaps between rounds is now %d.\n",
                       numberOfSwaps);
		}

		/* Create the optimized keyboard for this round. */
		int fileReadRes = FILE_READ_NOT_HAPPEN;
		if (INIT_FROM_FILE && !isFileEmpty) {
			if ((fileReadRes = layoutFromFile(file, &k)) != 0) {
				isFileEmpty = TRUE;
				fclose(file);
				file = NULL;
			}
		} 
				
		if (isFileEmpty || fileReadRes == FILE_READ_NOT_HAPPEN) {
			if (roundNum > 0 && (double) rand() / RAND_MAX <= chanceToUsePreviousLayout) {				
				usedPreviousLayout = TRUE;
				/* There is a 1 out of subChanceToUseBestLayout chance that the 
				 * best layout will be used instead of the last layout.
				 */
				if ((double) rand() / RAND_MAX <= subChanceToUseBestLayout) copyKeyboard(&k, &bestk);
				else copyKeyboard(&k, &prevk);
				
				smartMutate(NULL, &k, numberOfSwaps);
			} else {
				usedPreviousLayout = FALSE;
				initKeyboard(&k);
			}
			
			if (REPEAT_LAYOUTSTORE) {
				if (file) fclose(file);
				file = fopen(filename, "r");
				CHECK_FILE_FOR_NULL(file, filename);
				isFileEmpty = FALSE;
			}
		}
		
		curEval = anneal(&k, NULL, 0);
		
		if (curEval < bestEval) {
			if (usedPreviousLayout && detailedOutput) {
				printf("\nFound from previous layout: \n");
			}
			copyKeyboard(&bestk, &k);
			bestEval = curEval;
			printPercentages(&k);

			finish = time(NULL);
			printf("Time elapsed after %d rounds: %ld hours, %ld minutes, %ld seconds\n", roundNum, (finish-start)/3600, ((finish - start)%3600)/60, (finish-start)%60);
		} else if (curEval == bestEval && detailedOutput) {
			printf("Same layout found\n");
		} else if (time(NULL) - finish >= intervalBetweenPrints) {
			finish = time(NULL);
			printf("Time elapsed after %d rounds: %ld hours, %ld minutes, %ld seconds\n", roundNum, (finish-start)/3600, ((finish - start)%3600)/60, (finish-start)%60);
			++intervalInc;
			if (intervalInc >= 4) {
				intervalInc = 0;
				intervalBetweenPrints *= 2;
			}
		}

		if (roundNum % greatToBestInterval == greatToBestInterval - 1) {
			greatToBest(&bestk, GTB_ROUNDS);
			
			if (greatToBestInterval > 1 && 
					roundNum % roundsBeforeGTBIDec == roundsBeforeGTBIDec - 1)
				greatToBestInterval /= 2;
			
			if (bestk.fitness < bestEval) {
				bestEval = bestk.fitness;
				printPercentages(&bestk);

				finish = time(NULL);
				printf("Time elapsed after greatToBest(): %ld hours, %ld minutes, %ld seconds\n", 
					(finish-start)/3600, ((finish - start)%3600)/60, (finish-start)%60);			
			}
		}
	}
		
	finish = time(NULL);
	printf("\nTime elapsed: %ld hours, %ld minutes, %ld seconds\n", (finish-start)/3600, ((finish - start)%3600)/60, (finish-start)%60);

    return 0;
}


/*
 * Uses multiple threads to search for an optimal keyboard layout.
 */
void runThreadedAlgorithm()
{	
	struct ThreadArg arg;
	copyKeyboard(&arg.bestk, &nilKeyboard);
	arg.numRounds = ALGORITHM_ROUNDS;
	arg.startTime = time(NULL);
	arg.threadCount = threadCount;
    arg.chanceToUsePreviousLayout = CHANCE_TO_USE_PREVIOUS_LAYOUT;
    arg.numberOfSwaps = NUM_SWAPS_BETWEEN_ROUNDS;
	arg.isFinished = FALSE;
    
    int runsBeforeChanceInc = RUNS_BEFORE_CHANCE_INC;
    int runsBeforeSwapsInc = RUNS_BEFORE_SWAPS_INC;
    int gtbRounds = GTB_ROUNDS;
    
    int64_t prevBestFitness = FITNESS_MAX;
	
    int runNum;
	for (runNum = 0; ; ++runNum) {
        if (runNum % runsBeforeChanceInc == 0) {
            arg.chanceToUsePreviousLayout =
                    pow(arg.chanceToUsePreviousLayout, 0.7);
			runsBeforeChanceInc = (int) (runsBeforeChanceInc * 1.2) + 1;
			if (detailedOutput)
                printf("Chance to use previous layout is now %f.\n",
                       arg.chanceToUsePreviousLayout);
        }
        
        if (runNum % runsBeforeSwapsInc == 0) {
            ++arg.numberOfSwaps;
			runsBeforeSwapsInc = (int) (runsBeforeSwapsInc * 1.1) + 1;
			if (detailedOutput)
                printf("Number of swaps between rounds is now %d.\n",
                       arg.numberOfSwaps);
        }
        
        if (runNum % RUNS_BEFORE_GTB_ROUNDS_INC == 0) {
            gtbRounds *= 2;
            if (detailedOutput)
                printf("Number of rounds in greatToBest() is now %d.\n",
                       gtbRounds);
        }
        
		runThreadsRec((void *) (&arg));
		
		/* TODO: Add convergence hacks. */
        
        if (arg.bestk.fitness < prevBestFitness) {
            prevBestFitness = arg.bestk.fitness;
            printPercentages(&arg.bestk);
            printTime(arg.startTime);
        } else if ((double) (int) (log2(runNum) * 4) == log2(runNum) * 4) {
            /* TODO: this never prints */
            printTime(arg.startTime);
        }
        
        int64_t bestBeforeGTB = arg.bestk.fitness;
        greatToBest(&arg.bestk, gtbRounds);
        
        if (arg.bestk.fitness < bestBeforeGTB) {
            prevBestFitness = arg.bestk.fitness;
            if (detailedOutput)
                printf("\n***Found from greatToBest()***\n");
            printPercentages(&arg.bestk);
            printTime(arg.startTime);
        }
	}
}

/* 
 * This is the core function called by runThreadedAlgorithm(). It uses the 
 * following process to avoid as much downtime as possible.
 * 
 *  1. If threadCount <= 1, run anneal() numRounds times.
 *  2. Otherwise, recursively call this function with a decremented threadCount.
 *  3. Until the inner call of this function returns, keep running anneal().
 *  4. If the inner call found a bestk better than the current call, set the 
 *     current bestk to the inner bestk.
 * 
 * The core of this algorithm is step 3. In the worst case, this function will 
 * just be starting a new call to anneal() as the inner function returns, so 
 * the most possible time that can be wasted is the length of one anneal() call 
 * multiplied by the depth of the recursive call tree. The greatest proportion 
 * of time that can be wasted is (threadCount / numRounds), so for sufficiently 
 * large values of numRounds (perhaps 16 and above), the wasted time is trivial.
 */
void * runThreadsRec(void *arg)
{
	struct ThreadArg *threadArg = (struct ThreadArg *) arg;
		
	struct ThreadArg innerArg;
	copyThreadArg(&innerArg, threadArg);
	innerArg.threadCount = threadArg->threadCount - 1;
	innerArg.isFinished = FALSE;
	
    if (threadArg->threadCount > 1) {
        pthread_t thread;
        int ret = pthread_create(&thread, NULL, &runThreadsRec, 
                                 (void *) (&innerArg));
        if (ret) {
            threadArg->isFinished = TRUE;
            return (void *) ret;
        }
    }
	
	Keyboard k, prevk;
    copyKeyboard(&k, &nilKeyboard);
    copyKeyboard(&prevk, &nilKeyboard);
    
    int i;
	for (i = 0; threadArg->threadCount <= 1 ? i < threadArg->numRounds :
                !innerArg.isFinished; ++i) {
        if (i > 0 && rand() / RAND_MAX < threadArg->chanceToUsePreviousLayout) {
            copyKeyboard(&k, &prevk);
            smartMutate(NULL, &k, threadArg->numberOfSwaps);
        } else {
            initKeyboard(&k);
        }
		
        anneal(&k, NULL, 0);
        copyKeyboard(&prevk, &k);
        
		if (k.fitness < threadArg->bestk.fitness) {
            copyKeyboard(&threadArg->bestk, &k);
            
            /* Only print keyboards on the bottom thread. This is less 
             * cluttered than printing from every thread, and the user still 
             * gets to see something.
             */
            if (threadArg->threadCount <= 1) {
                printPercentages(&threadArg->bestk);
                printTime(threadArg->startTime);
            }
        }
    }
		
	if (innerArg.bestk.fitness < threadArg->bestk.fitness) {
		copyKeyboard(&threadArg->bestk, &innerArg.bestk);
	}
	
	threadArg->isFinished = TRUE;
	return NULL;
}

/* Take a great keyboard and make it the best keyboard. Uses an optimization 
 * heuristic that works best for nearly-optimal keyboards.
 */
void greatToBest(Keyboard *k, int numRounds)
{
	struct ThreadArg arg;
    initThreadArg(&arg);
	copyKeyboard(&arg.bestk, k);
	arg.numRounds = numRounds;
	arg.threadCount = threadCount;
	arg.isFinished = FALSE;
	
	greatToBestThreadRec((void *) (&arg));
    copyKeyboard(k, &arg.bestk);
}

void * greatToBestThreadRec(void *arg)
{
    struct ThreadArg *threadArg = (struct ThreadArg *) arg;
    
	Keyboard k;
    copyKeyboard(&k, &threadArg->bestk);
	
	struct ThreadArg innerArg;
	copyThreadArg(&innerArg, threadArg);
	innerArg.threadCount = threadArg->threadCount - 1;
	innerArg.isFinished = FALSE;
	
    if (threadArg->threadCount > 1) {
        pthread_t thread;
        int ret = pthread_create(&thread, NULL, &greatToBestThreadRec, 
                                 (void *) (&innerArg));
        if (ret) {
            threadArg->isFinished = TRUE;
            return (void *) ret;
        }
    }
	
	int numberOfSwaps = GTB_NUMBER_OF_SWAPS;
	
	int i;
	for (i = 0; threadArg->threadCount <= 1 ? i < threadArg->numRounds :
                !innerArg.isFinished; ++i) {
		if (i % GTB_ROUNDS_BEFORE_SWAP_INC == GTB_ROUNDS_BEFORE_SWAP_INC - 1) {
			++numberOfSwaps;
		}
        
        copyKeyboard(&k, &threadArg->bestk);
		
		/* Any swaps made by smartMutate() are "locked in" and may not be 
         * undone by anneal().
         */
		int lockins[numberOfSwaps][2];
		smartMutate(lockins, &k, numberOfSwaps);
		
        /* Use lockins only half the time. */
		if (i % 2 == 0) anneal(&k, lockins, numberOfSwaps);
		else anneal(&k, NULL, 0);
		
		calcFitness(&k);
		if (k.fitness < threadArg->bestk.fitness) {
			copyKeyboard(&threadArg->bestk, &k);
		}
	}
	    
    threadArg->isFinished = TRUE;
	return NULL;
}

void greatToBestBruteForce(Keyboard *k)
{
	int i, j, length = 9;
	int locs[length], origLocs[length];
		
	/* Choose which locs to permute. */
	for (i = 0, j = monLen - 1; i < length; ++i, --j) {
		while (!isSwappable(monKeys[j]) || rand() % 4 == 0)
			--j;
		
		/* If we run out of characters (which is unlikely), loop back around. */
		if (j < 0) j = monLen - 1;
		origLocs[i] = locs[i] = locWithShifted(k, monKeys[j]);
	}
	
	for (i = 0; i < length; ++i) {
		if (isBracket(charAt(k, locs[i]))) {
			int savedLoc = locs[(i+1) % length];
			origLocs[(i+1) % length] = locs[(i+1) % length] = 
					locWithShifted(k, getMatchingBracket(charAt(k, locs[i])));
			++i;
			
			/* If locs already contained the matching bracket, remove one of 
			 * the copies of the matching bracket and replace it with the 
			 * character that was removed.
			 */
			for (j = 0; j < length; ++j)
				if (i != j && locs[i] == locs[j])
					origLocs[j] = locs[j] = savedLoc;
		}
	}
	
	/* Initialize the keyboards. */
	calcFitness(k);
	Keyboard origk;
	copyKeyboard(&origk, k);

	/* Find the best permutation. */
	int p[length];
	int used[length];
	memset(used, 0, sizeof(int) * length);
	tryPermutations(length, p, 0, used, locs, k, &origk);
}

/* 
 * Adapted from the permutation algorithm used in the Ruby 1.9 standard library.
 * 
 * p: the array of indices that we're filling in
 * index: what index we're filling in now
 * used: an array of booleans: whether a given index is already used
 * locs: the keyboard locations that we are permuting
 */
int tryPermutations(int length, int *p, int index, int *used, int *locs, 
	Keyboard *bestk, Keyboard *origk)
{
	int i, j;
	for (i = 0; i < length; ++i) {
		if (!used[i]) {
			p[index] = i;
			if (index < length - 1) {
				used[i] = TRUE;
				tryPermutations(length, p, index + 1, used, locs, bestk, origk);
				used[i] = FALSE;
			} else {
				char keys[length];
				for (j = 0; j < length; ++j)
					keys[j] = charAt(origk, locs[j]);
				
				for (j = 0; j < length; ++j)
					if (isLegalSwap(origk, locs[p[j]], locs[j]))
						setCharAt(origk, locs[p[j]], keys[j]);
				calcFitness(origk);
								
				if (origk->fitness < bestk->fitness)
					copyKeyboard(bestk, origk);

				/* Put keys back to their original spots. */
				for (j = 0; j < length; ++j)
					setCharAt(origk, locs[j], keys[j]);
			}
		}
	}
	
	return 0;
}

/* 
 * Simulated annealing algorithm written by Chris Johnson.
 */
int64_t anneal(Keyboard *k, int lockins[][2], size_t lockin_length)
{
	int64_t lastEvaluation, evaluation;
	int64_t lastImprovement = 0;
	int64_t evaluationToBeat = FITNESS_MAX;
		
	/* Do the "zeroth" iteration */
	calcFitness(k);
	lastEvaluation = evaluation = k->fitness;
		
	/* Keep doing iterations while the layout is still improving */
	do {
		if (evaluation < lastEvaluation) {
			lastImprovement = lastEvaluation - evaluation;
		} else {
			lastImprovement = 0;
		}
		
		lastEvaluation = evaluation;
		evaluationToBeat = lastEvaluation + lastImprovement;
		evaluation = improveLayout(evaluationToBeat, k, lockins, lockin_length);
		
	} while (evaluation < evaluationToBeat);

	return evaluation;
}

/* 
 * Written by Chris Johnson.
 */
int64_t improveLayout(int64_t evaluationToBeat, Keyboard *k, 
	int lockins[][2], size_t lockin_length)
{
	int64_t evaluation;
	int i, j, inx;
	
	/* Create a list of indices and shuffle it. */
	int indices[2 * trueksize];
	buildShuffledIndices(indices, 2 * trueksize);

	/* try swaps until we beat evaluationToBeat... */
	for (i = 0; i < 2 * trueksize; ++i) {
		for (j = i + 1; j < 2 * trueksize; ++j) {
			
			if (!isLegalSwap(k, indices[i], indices[j])) {
				continue;
			}
			
			/* TODO: This is slow. I don't think it will be a significant 
			 * bottleneck, but I should profile it just in case.
			 */
			int skipRound = FALSE;
			for (inx = 0; inx < lockin_length; ++inx) {
				if (lockins[inx][0] == indices[i] || 
						lockins[inx][0] == indices[j] || 
						lockins[inx][1] == indices[i] || 
						lockins[inx][1] == indices[j]) {
					skipRound = TRUE;
					break;
				}
			}
			if (skipRound) {
				continue;
			}
			
			swap(k, indices[i], indices[j]); 
			
			calcFitness(k);
			evaluation = k->fitness;

			if (evaluation < evaluationToBeat) {
				/* good swap--keep it */
				return evaluation;
			} else {
				/* bad swap--undo it */
				swap(k, indices[i], indices[j]);
			}
		}
	}

	/* ...or not */
	return evaluationToBeat;
}

/* 
 * Goes through the list of characters from least to most common. Selects 
 * characters to swap. Less frequent characters are more likely to be swapped.
 * 
 * For the nth character, the probability that it will be mutated is given by 
 *   P(1) = 1 / q
 *   P(n) = (1 / q) * (1 - (P(1) + P(2) + ... + P(n-1)))
 * where q is a constant.
 * 
 * Alternatively, 
 *   P(n) = (q-1)^(n-1) / q^n
 * (This formula does not necessarily apply for the last element.)
 * 
 * These probabilities are only correct if every swap is legal. If not, legal 
 * swaps are somewhat more probable and illegal swaps are of course completely 
 * impossible.
 * 
 * swapIndices: After the function returns, this contains a list of swaps made. 
 *   It must be able to contain (numberOfSwaps) swaps. If swapIndices is NULL, 
 *   this function will execute as normal but will not write anything to 
 *   swapIndices.
 */
int smartMutate(int swapIndices[][2], Keyboard *k, int numberOfSwaps)
{
	int q = monLen / 4;
	
	int swapslen = 2 * numberOfSwaps;	
	char charsToSwap[swapslen];
	
	int i, j;
	
	/* Fills charsToSwap. */
	for (j = 0; j < swapslen; ++j) {
		charsToSwap[j] = monKeys[0];

		for (i = monLen - 1; i >= 0; --i) {
			if (isSwappable(monKeys[i]) && rand() % q == 0) {
				charsToSwap[j] = monKeys[i];
				break;
			}
		}
	}
	
	int lc1, lc2;
	for (i = 0; i < swapslen; i += 2) {
		lc1 = locWithShifted(k, charsToSwap[i]);
		lc2 = locWithShifted(k, charsToSwap[i+1]);
		
		if (swapIndices) {
            swapIndices[i / 2][0] = lc1;
            swapIndices[i / 2][1] = lc2;
        }
		
		if (isLegalSwap(k, lc1, lc2)) {
			swap(k, lc1, lc2);
		}
	}
	
	return 0;
}

void initThreadArg(struct ThreadArg *arg)
{
    memset(arg, 0, sizeof(struct ThreadArg));
}

void copyThreadArg(struct ThreadArg *dest, struct ThreadArg *src)
{
    memcpy(dest, src, sizeof(struct ThreadArg));
}
