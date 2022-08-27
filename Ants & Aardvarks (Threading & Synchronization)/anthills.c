#include "/comp/111/assignments/aardvarks/anthills.h"

// note: need to serialize concept of state
// for ease of debugging

// no longer in stdio.h
void setlinebuf(FILE *stream);

///////////////////////////////////////
// private data used internally
///////////////////////////////////////

// states of an aardvark
#define IDLE 0
#define SLURPING 1
#define SWALLOWING 2
#define SULKING 3
#define JAILED 4
#define STATES 5
const char *state_names[STATES] = {
    "idle", "slurping", "swallowing", "sulking", "jailed"};

// the aardvarks
#define ANAME(i) ('A' + i)
static pthread_t threads[AARDVARKS];

// state variables
static pthread_mutex_t state_lock; // gate the hill state machine
static pthread_mutex_t time_lock;  // gate the elapsed function.
pthread_mutex_t init_lock;         // initialization lock

static int ants_left[ANTHILLS] = { // ants left to slurp.
    ANTS_PER_HILL, ANTS_PER_HILL, ANTS_PER_HILL};

static int slurping[ANTHILLS] = {0, 0, 0}; // how many aardvarks are slurping
static int activity[AARDVARKS] = {         // is an aardvark busy? doing what?
    IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE};

static int sulking = 0;    // number of aardvarks sulking
static int swallowing = 0; // number of aardvarks swallowing

// whether to enter debugging mode
static int debug = FALSE;
static int quiet = FALSE;
static int trace = FALSE;
static FILE *csv = NULL;

static struct timeval start, now;

///////////////////////////////////////
// public routines usable by student's program
///////////////////////////////////////

double elapsed()
{ // how much real time has elapsed?
    pthread_mutex_lock(&time_lock);
    gettimeofday(&now, NULL);
    time_t seconds = now.tv_sec - start.tv_sec;
    suseconds_t microseconds = now.tv_usec - start.tv_usec;
    if (microseconds < 0)
    {
        microseconds += 1000000;
        seconds -= 1;
    }
    double e = (double)(seconds) + ((double)microseconds) / 1000000.0;
    pthread_mutex_unlock(&time_lock);
    return e;
}

int chow_time()
{ // is there something to eat left?
    pthread_mutex_lock(&state_lock);
    int out = ants_left[0] > 0 || ants_left[1] > 0 || ants_left[2] > 0;
    pthread_mutex_unlock(&state_lock);
    return out;
}

///////////////////////////////////////
// utilities safely update state variables
// these are NOT available to the aardvark
///////////////////////////////////////

static void trace_state()
{
    if (trace)
        printf(
            "%09.6f (trace) Ants: %d %d %d Slurping: %d %d %d, Swallowing: %d, Sulking: %d\n",
            elapsed(),
            ants_left[0], ants_left[1], ants_left[2],
            slurping[0], slurping[1], slurping[2],
            swallowing, sulking);
}

// update counters
static void update_sulking(int increment)
{
    sulking += increment;
    trace_state();
}
static void update_swallowing(int increment)
{
    swallowing += increment;
    trace_state();
}
static void update_slurping(int hill, int increment)
{
    slurping[hill] += increment;
    trace_state();
}
/*
static void update_ants(int hill, int increment) {
    ants_left[hill]+=increment;
    trace_state();
}
*/

///////////////////////////////////////
// control aardvark concurrency
// aname must be a valid aardvark between 'A' and 'H'.
// (these routines are private to the simulator)
///////////////////////////////////////
static int make_aardvark_slurping(char aname)
{
    int index = aname - 'A';
    if (index < 0 || index >= AARDVARKS)
        return FALSE;
    int ret;
    if (activity[index] != IDLE)
    {
        ret = FALSE;
    }
    else
    {
        activity[index] = SLURPING;
        ret = TRUE;
    }
    return ret;
}

static int make_aardvark_swallowing(char aname)
{
    int index = aname - 'A';
    if (index < 0 || index >= AARDVARKS)
        return FALSE;
    int ret;
    if (activity[index] != SLURPING)
    {
        ret = FALSE;
    }
    else
    {
        activity[index] = SWALLOWING;
        ret = TRUE;
    }
    return ret;
}

static int make_aardvark_sulking(char aname)
{
    int index = aname - 'A';
    if (index < 0 || index >= AARDVARKS)
        return FALSE;
    int ret;
    if (activity[index] != IDLE)
    {
        ret = FALSE;
    }
    else
    {
        activity[index] = SULKING;
        ret = TRUE;
    }
    return ret;
}

static int make_aardvark_idle(char aname)
{
    int index = aname - 'A';
    if (index < 0 || index >= AARDVARKS)
        return FALSE;
    int ret;
    if (activity[index] == JAILED)
    {
        ret = FALSE;
    }
    else
    {
        activity[index] = IDLE;
        ret = TRUE;
    }
    return ret;
}

///////////////////////////////////////
// punish a naughty aardvark by making it sleep for a penalty
///////////////////////////////////////
static void make_aardvark_jailed(char aname)
{
    int index = aname - 'A';
    if (index < 0 || index >= AARDVARKS)
        return;
    activity[index] = JAILED;
}
static void make_aardvark_unjailed(char aname)
{
    int index = aname - 'A';
    if (index < 0 || index >= AARDVARKS)
        return;
    activity[index] = IDLE;
}
static int is_jailed(char aname)
{
    int index = aname - 'A';
    if (index < 0 || index >= AARDVARKS)
        return FALSE;
    return (activity[index] == JAILED);
}

///////////////////////////////////////
// utilities carry out various actions of an aardvark
///////////////////////////////////////
static void sulk(char aname)
{ // wait after an error
    if (!quiet)
        printf("%09.6f Aardvark %c sulking\n", elapsed(), aname);
    if (csv)
        fprintf(csv, "%09.6f,%c,sulking\n", elapsed(), aname);

    pthread_mutex_lock(&state_lock);
    update_sulking(1);
    make_aardvark_sulking(aname);
    pthread_mutex_unlock(&state_lock);

    sleep(ERROR_TIME); // simulate external time passing

    pthread_mutex_lock(&state_lock);
    update_sulking(-1);
    make_aardvark_idle(aname);
    pthread_mutex_unlock(&state_lock);

    if (!quiet)
        printf("%09.6f Aardvark %c idle\n", elapsed(), aname);
    if (csv)
        fprintf(csv, "%09.6f,%c,idle\n", elapsed(), aname);
}

static int lick(int anthill)
{ // get an ant if one is there
    int gotit;
    pthread_mutex_lock(&state_lock);
    if (ants_left[anthill] > 0)
    {
        gotit = TRUE;
        --ants_left[anthill];
    }
    else
    {
        gotit = FALSE;
    }
    trace_state();
    pthread_mutex_unlock(&state_lock);
    return gotit;
}

static void swallow(char aname)
{ // swallow an ant
    if (!quiet)
        printf("%09.6f Aardvark %c swallowing\n", elapsed(), aname);
    if (csv)
        fprintf(csv, "%09.6f,%c,swallowing\n", elapsed(), aname);

    pthread_mutex_lock(&state_lock);
    update_swallowing(1);
    make_aardvark_swallowing(aname);
    pthread_mutex_unlock(&state_lock);

    sleep(SWALLOW_TIME);

    pthread_mutex_lock(&state_lock);
    update_swallowing(-1);
    make_aardvark_idle(aname);
    pthread_mutex_unlock(&state_lock);

    if (debug)
        printf("%09.6f (debug) Aardvark %c finished swallowing\n",
               elapsed(), aname);
    if (!quiet)
        printf("%09.6f Aardvark %c idle\n", elapsed(), aname);
    if (csv)
        fprintf(csv, "%09.6f,%c,idle\n", elapsed(), aname);
}

int slurp(char aname, int anthill)
{ // try to slurp up an ant.
    if (aname < 'A' || aname > 'A' + AARDVARKS - 1)
    {
        if (debug)
            printf(
                "%09.6f (debug) ERROR: illegal aardvark %c specified\n",
                elapsed(), aname);
        sulk(aname);
        return FALSE;
    }
    else if (anthill < 0 || anthill >= ANTHILLS)
    {
        if (debug)
            printf(
                "%09.6f (debug) ERROR: aardvark %c tried to slurp from illegal anthill %d\n",
                elapsed(), aname, anthill);
        sulk(aname);
        return FALSE;
    }
    else if (ants_left[anthill] == 0)
    {
        if (debug)
            printf(
                "%09.6f (debug) ERROR: aardvark %c tried to slurp from empty anthill %d\n",
                elapsed(), aname, anthill);
        sulk(aname);
        return FALSE;
    }
    else if (is_jailed(aname))
    {
        if (!quiet)
            printf(
                "%09.6f CONCURRENCY BOTCH: aardvark %c is already in jail\n",
                elapsed(), aname);
        return FALSE;
    }
    else
    {
        pthread_mutex_lock(&state_lock);
        if (slurping[anthill] >= AARDVARKS_PER_HILL)
        {
            pthread_mutex_unlock(&state_lock);
            if (debug)
                printf(
                    "%09.6f (debug) ERROR: aardvark %c tried to slurp anthill %d where %d aardvarks were slurping\n",
                    elapsed(), aname, anthill, AARDVARKS_PER_HILL);
            sulk(aname);
            return FALSE;
        }
        else
        {
            if (make_aardvark_slurping(aname))
            {
                update_slurping(anthill, 1);
                pthread_mutex_unlock(&state_lock);

                if (!quiet)
                    printf("%09.6f Aardvark %c slurping Anthill %d\n",
                           elapsed(), aname, anthill);
                if (csv)
                    fprintf(csv, "%09.6f,%c,slurping,%d\n",
                            elapsed(), aname, anthill);

                sleep(SLURP_TIME); // simulate external time passing

                pthread_mutex_lock(&state_lock);
                update_slurping(anthill, -1);
                pthread_mutex_unlock(&state_lock);

                // at this point, you have gotten done with a slurp.
                // check whether you got an ant. This is a race condition.
                int gotit = lick(anthill);
                if (gotit)
                {
                    if (debug)
                        printf(
                            "%09.6f (debug) Aardvark %c slurped an ant from anthill %d\n",
                            elapsed(), aname, anthill);
                    swallow(aname);
                    return TRUE;
                }
                else
                {
                    if (debug)
                        printf(
                            "%09.6f (debug) FAILURE: Aardvark %c slurped from empty anthill %d\n",
                            elapsed(), aname, anthill);

                    // no swallow() or sulk() in this case: lost the race
                    // just make the aardvark idle
                    pthread_mutex_lock(&state_lock);
                    make_aardvark_idle(aname);
                    pthread_mutex_unlock(&state_lock);

                    if (!quiet)
                        printf("%09.6f Aardvark %c is idle\n",
                               elapsed(), aname);
                    if (csv)
                        fprintf(csv, "%09.6f,%c,idle\n",
                                elapsed(), aname);

                    return FALSE;
                }
            }
            else
            { // aardvark is already busy
                int old_activity = activity[aname - 'A'];
                make_aardvark_jailed(aname);
                pthread_mutex_unlock(&state_lock);

                if (!quiet)
                    printf("%09.6f CONCURRENCY BOTCH: Aardvark %c is already %s! Jailed for %d seconds!\n", elapsed(), aname, state_names[old_activity], JAIL_TIME);
                if (csv)
                    fprintf(csv, "%09.6f,%c,jailed\n", elapsed(), aname);

                sleep(JAIL_TIME);

                pthread_mutex_lock(&state_lock);
                make_aardvark_unjailed(aname);
                pthread_mutex_unlock(&state_lock);

                if (!quiet)
                    printf("%09.6f Aardvark %c is idle (out of jail)\n",
                           elapsed(), aname);
                if (csv)
                    fprintf(csv, "%09.6f,%c,idle\n", elapsed(), aname);

                return FALSE; // no swallowing in this case
            }
        }
    }
    fprintf(stderr, "ERROR: simulator failure: notify instructor!\n");
    return FALSE;
}

int main(int argc, char **argv)
{

    debug = FALSE;
    int i;
    setlinebuf(stdout);
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "debug") == 0)
        {
            debug = TRUE;
        }
        else if (strcmp(argv[i], "quiet") == 0)
        {
            quiet = TRUE;
        }
        else if (strcmp(argv[i], "trace") == 0)
        {
            trace = TRUE;
        }
        else if (strcmp(argv[i], "csv") == 0)
        {
            csv = fopen("output.csv", "w");
            if (!csv)
            {
                fprintf(stderr, "can't write output.csv\n");
                exit(1);
            }
        }
        else
        {
            fprintf(stderr, "%s usage: %s [debug] [trace] [quiet] [csv]\n", argv[0], argv[0]);
            fprintf(stderr, "  debug: print detailed debugging information\n");
            fprintf(stderr, "  trace: print a trace of system state\n");
            fprintf(stderr, "  quiet: suppress running narrative\n");
            fprintf(stderr, "  csv: write state log to output.csv\n");
            exit(1);
        }
    }

    gettimeofday(&start, NULL);

    char anames[AARDVARKS]; // names of aardvarks: A-G
    pthread_mutex_init(&state_lock, NULL);
    pthread_mutex_init(&time_lock, NULL);
    pthread_mutex_init(&init_lock, NULL);
    int anumber;
    for (anumber = 0; anumber < AARDVARKS; anumber++)
    {
        anames[anumber] = ANAME(anumber);
        pthread_create(&threads[anumber], NULL,
                       aardvark, (void *)(anames + anumber));
    }
    for (anumber = 0; anumber < AARDVARKS; anumber++)
    {
        void *retptr;
        pthread_join(threads[anumber], (void **)&retptr);
    }
    pthread_mutex_destroy(&state_lock);
    pthread_mutex_destroy(&time_lock);
    double e = elapsed();
    printf("total elapsed real time is %09.6f seconds\n", e);
    if (chow_time())
    {
        printf("ERROR: not all ants were consumed!\n");
    }
    exit(0);
}
