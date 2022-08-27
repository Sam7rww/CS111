#define _BSD_SOURCE
#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <pthread.h>
#include <string.h>

#define loopTime 200000
#define task3Time 10000
#define BUFFER_SIZE 4096
/**
 * the struct related to time storing
struct rusage
{
	struct timeval ru_utime; //user time used
	struct timeval ru_stime; //system time used
};
struct timeval
{
	time_t tv_sec;		 //seconds since Jan. 1, 1970
	suseconds_t tv_usec; //and microseconds
};
*/

/**
 * @brief calculate the elapsed time between start and now (type is microseconds)
 *
 * @param start
 * @param now
 * @return double
 */
double elpase(struct timeval start, struct timeval now)
{
	time_t seconds = now.tv_sec - start.tv_sec;
	suseconds_t microseconds = now.tv_usec - start.tv_usec;
	if (microseconds < 0)
	{
		microseconds += 1000000;
		seconds -= 1;
	}
	double e = (double)(seconds)*1000000.0 + ((double)microseconds);
	return e;
}

int main()
{
	int i;
	struct timeval loopStart;
	struct timeval loopEnd;
	gettimeofday(&loopStart, NULL);
	for (i = 0; i < task3Time; i++)
		;
	gettimeofday(&loopEnd, NULL);
	double basicTime = elpase(loopStart, loopEnd);

	struct rusage loopStart1;
	struct rusage loopEnd1;
	getrusage(RUSAGE_SELF, &loopStart1);
	for (i = 0; i < loopTime; i++)
		;
	getrusage(RUSAGE_SELF, &loopEnd1);
	double basicTime_utime = elpase(loopStart1.ru_utime, loopEnd1.ru_utime);
	double basicTime_stime = elpase(loopStart1.ru_stime, loopEnd1.ru_stime);

	// first task: allocate one page of memory with mmap()
	printf("first task: allocate one page of memory with mmap()!\n");
	int page_size = getpagesize();
	struct rusage mmap_1a;
	struct rusage mmap_1b;

	char *mp;
	getrusage(RUSAGE_SELF, &mmap_1a);
	for (i = 0; i < loopTime; i++)
	{
		// execute mmap()
		mp = (void *)mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	}
	getrusage(RUSAGE_SELF, &mmap_1b);
	// free mmap
	munmap(mp, page_size);

	double mmap_utime = elpase(mmap_1a.ru_utime, mmap_1b.ru_utime) - basicTime_utime;
	if (mmap_utime < 0)
		mmap_utime = 0.0;
	double mmap_stime = elpase(mmap_1a.ru_stime, mmap_1b.ru_stime) - basicTime_stime;
	if (mmap_stime < 0)
		mmap_stime = 0.0;
	double ave_utime_1 = mmap_utime / loopTime;
	double ave_stime_1 = mmap_stime / loopTime;
	printf("average user time (allocate one page)   is %lf \n", ave_utime_1);
	printf("average system time (allocate one page) is %lf \n", ave_stime_1);

	// second task: lock a mutex with pthread_mutex_lock()
	printf("second task: lock a mutex with pthread_mutex_lock()!\n");
	struct rusage mutex_2a;
	struct rusage mutex_2b;
	pthread_mutex_t mutex[loopTime];
	// initiate the mutex
	for (i = 0; i < loopTime; i++)
	{
		pthread_mutex_init(&mutex[i], NULL);
	}

	getrusage(RUSAGE_SELF, &mutex_2a);
	for (i = 0; i < loopTime; i++)
	{

		// execute pthread_mutex_lock()
		pthread_mutex_lock(&mutex[i]);
	}
	getrusage(RUSAGE_SELF, &mutex_2b);
	// unlock mutex
	for (i = 0; i < loopTime; i++)
	{
		pthread_mutex_unlock(&mutex[i]);
	}

	double mutex_utime = elpase(mutex_2a.ru_utime, mutex_2b.ru_utime) - basicTime_utime;
	if (mutex_utime < 0)
		mutex_utime = 0;
	double mutex_stime = elpase(mutex_2a.ru_stime, mutex_2b.ru_stime) - basicTime_stime;
	if (mutex_stime < 0)
		mutex_stime = 0;
	double ave_utime_2 = (double)mutex_utime / loopTime;
	double ave_stime_2 = (double)mutex_stime / loopTime;
	printf("average user time (lock a mutex)   is %lf \n", ave_utime_2);
	printf("average system time (lock a mutex) is %lf \n", ave_stime_2);

	// third task
	printf("third task: measure wall-clock time!\n");
	// mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
	char *filename = "/tmp/wruan03.txt";

	// 1.writing 4096 Bytes directly (bypassing the disk page cache) to /tmp
	struct timeval current_time_1a;
	struct timeval current_time_1b;

	// init the data we write
	char *buf;
	int length = BUFFER_SIZE;
	buf = (char *)memalign(getpagesize(), length);
	if (buf == NULL)
	{
		printf("memalign error!/n");
		return -1;
	}
	for (int i = 0; i < length; i++)
	{
		buf[i] = 'a';
	}

	// open file
	int file = open(filename, O_CREAT | O_RDWR | O_DIRECT | O_SYNC);
	if (-1 == file) // fail to open file, return error
	{
		printf("fail open!");
		return -1;
	}
	gettimeofday(&current_time_1a, NULL);
	for (i = 0; i < task3Time; i++)
	{
		write(file, buf, BUFFER_SIZE);
	}
	gettimeofday(&current_time_1b, NULL);
	close(file);

	double wd_time = elpase(current_time_1a, current_time_1b) - basicTime;
	double ave_wd = (double)wd_time / task3Time;
	printf("The average time of writing 4096 Bytes directly is: %lf \n", ave_wd);
	free(buf);

	// 2.reading 4096 Bytes directly (bypassing the disk page cache) from /tmp
	struct timeval current_time_2a;
	struct timeval current_time_2b;

	// init the space we read to
	char *buf2;
	buf2 = (char *)memalign(getpagesize(), length);
	if (buf2 == NULL)
	{
		printf("memalign error!/n");
		return -1;
	}

	// open file
	file = open(filename, O_CREAT | O_RDWR | O_DIRECT | O_SYNC);
	if (-1 == file) // fail to open file, return error
	{
		printf("fail open!");
		return -1;
	}
	gettimeofday(&current_time_2a, NULL);
	for (i = 0; i < task3Time; i++)
	{
		read(file, buf2, BUFFER_SIZE);
	}
	gettimeofday(&current_time_2b, NULL);
	close(file);

	double rd_time = elpase(current_time_2a, current_time_2b) - basicTime;
	double ave_rd = (double)rd_time / task3Time;
	printf("The average time of reading 4096 Bytes directly is: %lf \n", ave_rd);

	// 3.writing 4096 Bytes to the disk page cache
	struct timeval current_time_3a;
	struct timeval current_time_3b;

	char *buf3;
	buf3 = (char *)memalign(getpagesize(), length);
	if (buf3 == NULL)
	{
		printf("memalign error!/n");
		return -1;
	}
	for (int i = 0; i < length; i++)
	{
		buf3[i] = 'b';
	}

	// open file
	int file_cache = open(filename, O_WRONLY);
	if (-1 == file_cache) // fail to open file, return error
	{
		printf("fail open!");
		return -1;
	}
	gettimeofday(&current_time_3a, NULL);
	for (i = 0; i < task3Time; i++)
	{
		write(file_cache, buf3, BUFFER_SIZE);
	}
	gettimeofday(&current_time_3b, NULL);
	close(file_cache);

	double w_time = elpase(current_time_3a, current_time_3b) - basicTime;
	double ave_w = (double)w_time / task3Time;
	printf("The average time of writing 4096 Bytes is: %lf \n", ave_w);
	free(buf3);

	// 4.reading 4096 Bytes from the disk page cache
	struct timeval current_time_4a;
	struct timeval current_time_4b;

	// open file
	file_cache = open(filename, O_RDONLY);
	if (-1 == file_cache) // fail to open file, return error
	{
		printf("fail open!");
		return -1;
	}
	gettimeofday(&current_time_4a, NULL);
	for (i = 0; i < task3Time; i++)
	{
		read(file_cache, buf2, BUFFER_SIZE);
	}
	gettimeofday(&current_time_4b, NULL);
	close(file_cache);

	double r_time = elpase(current_time_4a, current_time_4b) - basicTime;
	double ave_r = (double)r_time / task3Time;
	printf("The average time of reading 4096 Bytes is: %lf \n", ave_r);
	free(buf2);

	// remove file
	remove(filename);

	printf("finish!\n");
	return 0;
}
