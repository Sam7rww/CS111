#include "/comp/111/assignments/aardvarks/anthills.h"
// #include "anthills.h"
#include <semaphore.h>
#include <time.h>

#define TRUE 1
#define FALSE 0
#define wait_time 1.001

int initialized = FALSE; // semaphores and mutexes are not initialized

// define your mutexes and semaphores here
// they must be global variables.
// static sem_t aard[ANTHILLS];

static pthread_mutex_t antMutex[ANTHILLS];
static pthread_mutex_t mainMutex[ANTHILLS];
static pthread_mutex_t aardAll[ANTHILLS];

// how much aard current eat in ANTHILL
static int aard_num[ANTHILLS];
// how much aard in total in ANTHILL
static int aard_all[ANTHILLS];

static int ant_left[ANTHILLS];

static double slurp_time[ANTHILLS][ANTS_PER_HILL + 10];

static int ant_time_point[ANTHILLS];
static int wait_point[ANTHILLS];

void msleep(long msec);

// first thread initializes mutexes
void *aardvark(void *input)
{
    char aname = *(char *)input;
    int anthill_point = (aname - 'A') / 3;
    anthill_point = anthill_point >= 3 ? 2 : anthill_point;
    // first caller needs to initialize the mutexes!
    pthread_mutex_lock(&init_lock);
    if (!initialized++)
    { // this succeeds only for one thread
        // initialize your mutexes and semaphores here
        aard_all[0] = 3;
        aard_all[1] = 3;
        aard_all[2] = 5;
        int i, j;
        for (i = 0; i < ANTHILLS; i++)
        {
            ant_left[i] = 100;
            aard_num[i] = 0;
            ant_time_point[i] = 0;
            wait_point[i] = 0;
            for (j = 0; j < (ANTS_PER_HILL + 10); j++)
            {
                slurp_time[i][j] = 0.0;
            }
            // sem_init(&aard[i], 0, AARDVARKS_PER_HILL);
            pthread_mutex_init(&antMutex[i], NULL);
            pthread_mutex_init(&mainMutex[i], NULL);
            pthread_mutex_init(&aardAll[i], NULL);
        }
    }
    pthread_mutex_unlock(&init_lock);

    // now be an aardvark
    while (chow_time())
    {

        pthread_mutex_t *temp = &antMutex[anthill_point];
        pthread_mutex_lock(temp);
        if (ant_left[anthill_point] > 0)
        {
            ant_left[anthill_point]--;
            pthread_mutex_unlock(temp);

            pthread_mutex_lock(&mainMutex[anthill_point]);
            if (aard_num[anthill_point] < AARDVARKS_PER_HILL)
            {

                slurp_time[anthill_point][ant_time_point[anthill_point]] = elapsed();
                ant_time_point[anthill_point]++;

                aard_num[anthill_point] += 1;
                pthread_mutex_unlock(&mainMutex[anthill_point]);

                int res = slurp(aname, anthill_point);

                if (!res)
                {
                    // minus one ant
                    pthread_mutex_lock(temp);
                    ant_left[anthill_point]++;
                    pthread_mutex_unlock(temp);
                }
            }
            else
            {
                if (wait_point[anthill_point] >= ant_time_point[anthill_point])
                {
                    pthread_mutex_unlock(&mainMutex[anthill_point]);
                    pthread_mutex_lock(temp);
                    ant_left[anthill_point]++;
                    pthread_mutex_unlock(temp);
                    anthill_point = anthill_point == 2 ? 0 : anthill_point + 1;
                    continue;
                }
                double start_time = slurp_time[anthill_point][wait_point[anthill_point]];
                wait_point[anthill_point]++;
                pthread_mutex_unlock(&mainMutex[anthill_point]);

                double end_time = elapsed();
                pthread_mutex_lock(&mainMutex[anthill_point]);
                slurp_time[anthill_point][ant_time_point[anthill_point]++] = end_time >= (start_time + wait_time) ? end_time : (start_time + wait_time);
                pthread_mutex_unlock(&mainMutex[anthill_point]);

                if ((end_time - start_time) < wait_time)
                {
                    // required to sleep until one second elapsed
                    long time = (long)((start_time + wait_time - end_time) * 1000000);
                    msleep(time);
                    // usleep(time * 1000000);
                    // sleep(start_time + wait_time - end_time);
                }

                int res = slurp(aname, anthill_point);

                if (!res)
                {
                    // minus one ant
                    pthread_mutex_lock(temp);
                    ant_left[anthill_point]++;
                    pthread_mutex_unlock(temp);
                }
            }
        }
        else
        {
            pthread_mutex_unlock(temp);
            anthill_point = anthill_point == 2 ? 0 : anthill_point + 1;
            pthread_mutex_lock(&aardAll[anthill_point]);
            if (aard_all[anthill_point] >= 6)
            {
                pthread_mutex_unlock(&aardAll[anthill_point]);
                anthill_point = anthill_point == 2 ? 0 : anthill_point + 1;
            }
            else
            {
                aard_all[anthill_point]++;
                pthread_mutex_unlock(&aardAll[anthill_point]);
            }
        }
    }

    pthread_mutex_lock(&init_lock);
    initialized--;
    if (initialized == 0)
    {
        int i;
        for (i = 0; i < 3; i++)
        {
            pthread_mutex_destroy(&antMutex[i]);
            pthread_mutex_destroy(&mainMutex[i]);
            pthread_mutex_destroy(&aardAll[i]);
        }
    }
    pthread_mutex_unlock(&init_lock);

    return NULL;
}

/* msleep(): Sleep for the requested number of milliseconds. */
void msleep(long msec)
{
    struct timespec ts;

    ts.tv_sec = msec / 1000000;
    ts.tv_nsec = (msec % 1000000) * 1000;

    nanosleep(&ts, NULL);
}
