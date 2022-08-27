#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <pthread.h> 
#include <sys/time.h> 
#include <string.h> 

// student's solution
extern void *aardvark(void *); 

#define TRUE	 		1
#define FALSE	 		0	 
#define AARDVARKS		11	// number of total threads
#define ANTHILLS 		3	// number of anthills 
#define ANTS_PER_HILL 		100	// number of ants available at each hill
#define AARDVARKS_PER_HILL	3	// number of aardvarks who can share 
#define SLURP_TIME 		1
#define SWALLOW_TIME 		1
#define ERROR_TIME 		4
#define JAIL_TIME 		30 
extern int slurp(char aname, int anthill); // eat one ant. 
extern int chow_time(); // whether there are ants to eat
extern double elapsed(); // how much time has been spent? 

extern pthread_mutex_t init_lock; // use to disambiguate initialization race conditions 
