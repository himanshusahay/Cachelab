#include "cachelab.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>

#define DEBUG 0

typedef struct _line {
	int valid; // valid bit equivalent in line
	unsigned long int tag; // tag to identify match for address
	int age;
} line;

FILE *tracefile; // pointer to file used to read in

extern char *optarg; 
extern int optind;

/****************************************
 *  Functions for LRU
 ****************************************/

// returns the line with the oldest age in the cache
int LRU(line **cacheArray, int sets, int lines, int usedSet) {
	int j, r_line, r_age = -1;

	for (j = 0; j < lines; j++) {
		if ((cacheArray[usedSet][j]).age > r_age) {
			r_age = (cacheArray[usedSet][j]).age;
			r_line = j;
		}
	}

	return r_line;
}

// increments the age for all cache lines except for 'work' one which get set to zero
void updateAge(line **cacheArray, int sets, int lines, int usedSet, int usedLine) {
	//int i; 
	int j;
	//for (i = 0; i < sets; i++) {
		for (j = 0; j < lines; j++) {
			if (/*i == usedSet && */j == usedLine)
				(cacheArray[usedSet][j]).age = 0; //reset
			else 
				(cacheArray[usedSet][j]).age += 1; //increment

		}
	//}
}

void printAges(line **cacheArray, int sets, int lines) {
	int i, j;
	for (i = 0; i < sets; i ++) {
		for (j = 0; j < lines; j++) {
			printf("%d ", (cacheArray[i][j]).age );
		}
		printf("\n");
	}
}

/****************************************
 *  Cleaning up the allocated cache
 ****************************************/

void clean(line ** cacheArray, int sets, int lines) {
	int i;
	for (i = 0; i < sets; i++) {
		free(cacheArray[i]);
	}
	free (cacheArray);
}



// Author: Tim Petri
// userid: tapetri
int main(int argc, char** argv)
{

	int hit_count = 0, miss_count = 0, eviction_count = 0;

		int i; // loop counter

		/******************************
		 * Variables used for command line arguments
		 *****************************/
		int opt;
		int VERBOSE_MODE = 0; // print information, default off
		int s; // Number of set index bits 
		int numSets; // Number of sets (2^s)
		int E; // Associativity (number of lines per set)
		int b; // Number of block bits (the block size is 2^b)
		char * t = "default"; // Name of the valgrind trace to replay

	/******************************
	 * Variables used for the cache
	 *****************************/
	struct _line **cacheArray; 
	 unsigned long int addr, set, tag; // used when working with address
	int bool_hit, bool_evict; // true/false for hit and eviction
	int first_invalid_line, lru_line; // used to find space in cache to place fetched memory

	/******************************
	 * Variables reading in and tokenizing trace input
	 *****************************/
	char line[32]; // used to read in file from traces file
	const char delimiters[] = ", "; // ',' & ' '
	char *token_ins, *token_addr, *token_size;
	int size;



	while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
			switch (opt) {
				case 'v': VERBOSE_MODE = 1; break;
				case 's': s = atoi(optarg); break;
				case 'E': E = atoi(optarg); break;
				case 'b': b = atoi(optarg); break;
				case 't': t = optarg; break;

				case 'h':
				default:
						fprintf(stderr, "Usage: %s [-hvsEbt] [tracefile...]\n", argv[0]);
						exit(EXIT_FAILURE);
			}
	}

	if (DEBUG) {
		printf("%s\n", "Result from command line arguments:");
		printf("Verbose mode: %d \ns: %d \nE: %d \nb: %d \nt: %s\n",VERBOSE_MODE, s, E, b, t);
	}

	// open file
	tracefile = fopen(t , "r");
	if(tracefile == NULL) {
		printf("Error opening file");
		return(-1);
	}

	// compute the number of sets as 2^s
	numSets = pow(2, s);

	// allocate space for an array of pointer to lines, sets element
	cacheArray = malloc(numSets*sizeof(struct _line*));

	// error check
	if (cacheArray == NULL) {
		printf("%s\n", "Malloc error");
		return 1;
	}

	// allocate space for E lines in each set
	for (i = 0; i < numSets; i++) {
		cacheArray[i] = malloc( E*sizeof(struct _line) );
		if (cacheArray[i] == NULL) {
			printf("%s\n", "Malloc error");
			return 1;
		}
	}

	// read in and 'act' on each line
	while(fgets(line, 32, tracefile) != NULL) {

		bool_hit = 0; // reset
		bool_evict = 0; // reset
		first_invalid_line = 0; // reset

		// if first character on line is space, ignore line completely
		if (line[0] == ' ') {

			token_ins = strtok (line, delimiters); // token = instruction letter
			token_addr = strtok (NULL, delimiters); // token = address
			token_size = strtok (NULL, delimiters); // token = size [Convert to integer in order to skip newline]
			size = atoi(token_size);
			if (VERBOSE_MODE) printf("%s %s,%d ", token_ins, token_addr, size); 


			addr = strtol(token_addr, NULL, 16); // get address from trace
			tag = (addr >> s) >> b; // get tag from address
			set = ((addr << (64 - s - b)) >> (64 - s)); // get set from address


			if (DEBUG) {
				printf("Address in hex: %lx\n", addr);
				printf("Tag in hex: %lx\n", tag);
				printf("Set in hex: %lx\n", set);
				printf("Set index: %ld\n", set);
			}

			// =========== What happens below ===============
			// find first valid line in set and compare tag
			// if equal report hit
			// if not equal keep looking
			// if no match is found, get first invalid and replace tag
			// if no line is invalid, get LRU line and replace tag
			// ==============================================

			// in the set given by set part of address, look for valid line with matching tag
			for (i = 0; i < E; i++) {
				// if the line is valid and the tag matches, it is a hit
				if ( (cacheArray[set][i]).valid == 1 && (cacheArray[set][i]).tag == tag ) {
					bool_hit = 1; // set hit boolean to true
					updateAge(cacheArray, numSets, E, set, i); // // update ages of lines in cache (For LRU)
					break;
				}
				// if the line is invalid and  and it is the first invalid line, save it
				else if (!((cacheArray[set][i]).valid) && !first_invalid_line) { 
					first_invalid_line = i+1; // the (+1) is a fix use to be able to use the first_invalid_line as boolean and the actual line
				}
			}

			// if the cache lookup resulted in a miss, grab data from lower level cache and place in cache
			if (!bool_hit) {

				// if we found an invalid line, just use that one (= NO EVICTION)
				if (first_invalid_line) {
					bool_evict = 0;
					(cacheArray[set][first_invalid_line-1]).valid = 1; // set valid to true
					(cacheArray[set][first_invalid_line-1]).tag = tag; // set tag to match the one from requested address
					updateAge(cacheArray, numSets, E, set, first_invalid_line-1); // update ages of lines in cache (For LRU)
				}

				// else set the LRU line (= EVICTION)
				else {
					bool_evict = 1;
					lru_line = LRU(cacheArray, numSets, E, set);
					(cacheArray[set][lru_line]).valid = 1; // set valid to true
					(cacheArray[set][lru_line]).tag = tag; // set tag to match the one from requested address
					updateAge(cacheArray, numSets, E, set, lru_line); // update ages of lines in cache (For LRU)
				}
			}


			// switch to update  hit_count, 
			switch (*token_ins) {

				case 'M': // L + S
					if (bool_hit) {
						hit_count +=2;
						if (VERBOSE_MODE) printf("hit hit\n");
					}
					else if (bool_evict) {
						miss_count +=1;
						eviction_count += 1;
						hit_count +=1;
						if (VERBOSE_MODE) printf("miss eviction hit\n");
					}
					else {
						miss_count +=1;
						hit_count +=1;
						if (VERBOSE_MODE) printf("miss hit\n");
					}
					break;


				case 'L':
					if (bool_hit) {
						hit_count +=1;
						if (VERBOSE_MODE) printf("hit\n");
					}
					else if (bool_evict) {
						miss_count +=1;
						eviction_count += 1;
						if (VERBOSE_MODE) printf("miss eviction\n");
					}
					else {
						miss_count +=1;
						if (VERBOSE_MODE) printf("miss\n");
					}
					break;

				case 'S': 
					if (bool_hit) {
						hit_count +=1;
						if (VERBOSE_MODE) printf("hit\n");
					}
					else if (bool_evict) {
						miss_count +=1;
						eviction_count += 1;
						if (VERBOSE_MODE) printf("miss eviction\n");
					}
					else {
						miss_count +=1;
						if (VERBOSE_MODE) printf("miss\n");
					}
					break;
			}

			// debug too see ages for all lines in cache
			if (DEBUG) printAges(cacheArray, numSets, E);

		}
 }


 /***************
	* Print summary, close file and clean memory
	**************/
	printSummary(hit_count, miss_count, eviction_count);
	fclose(tracefile);
	clean(cacheArray, numSets, E);

	return 0;
}
