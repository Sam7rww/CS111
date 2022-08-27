# Measurement Assignment Questions
You may add your answers to the assignment questions in the space provided
below.  This is a markdown file (.md), so you may use markdown formatting. If
you do not know about markdown (or know but don't care to format anything) then
you may just treat this as a plain text file.


# Part A
Answer these questions before you start coding.  You may discuss the **Part A
questions *only*** with your classmates, but you must write your own answers
in your own words.


## Question 1
Research the `gettimeofday()` function.  (`man gettimeofday`)  What does this
function do?  How would you use it to measure the amount of time required
("latency" or "delay") to perform an action (say, calling the `foo()` function)?

### Answer:
The gettimeofday() function gets the system’s clock time.
I will use gettimeofday() function before and after an action (like calling the fo0() function). The difference between two timeval (stored the detailed time information) is the amount of time required.


## Question 2
Now research the `getrusage()` function (`man getrusage`), paying particular
attention to the `ru_utime` and `ru_stime` fields in the result.  (Assume it's
being called with the `RUSAGE_SELF` argument.)  How are the user time
(`ru_utime`), system time (`ru_stime`), and the time returned by
`gettimeofday()` all different from each other?

### Answer:
user time(ru_utime) is the amount of time CPU spent executing user mode. And system time(ru_stime) is the amount of time CPU spent in kernel mode(usually spent servicing system call). While the time returned by gettimeofday() is the real time elapsed, which is same as real world time.


## Question 3
Suppose you want to measure the time it take to do one fast thing; something
that takes less than a minute to do (e.g., fill one glass with water).  However,
the only tool you have for taking measurements is a digital clock (regular
clock, not a stopwatch) that does not show seconds, only shows hours and
minutes.  How would you get an accurate measure of how long it takes to do the
thing once?

### Answer:
I can measure the cost of time doing this fast thing one thousand times(suppose is k). So k/1000 will be the time it take to do that fast thing.

## Question 4
Suppose you want to find the average amount of time required to run function
`foo()`.  What is the difference between the following two approaches?  Which
one is better, and why?  (You may assume `foo()` is very fast.)

```c
latency = 0
loop N times
    measure start time
    call foo()
    measure end time
    latency += end time - start time
print latency/N
```

```c
measure start time
loop N times
    call foo()
measure end time
print (end time - start time) / N
```

### Answer:
The first one is to calculate the difference between start time and end time in each loop while the second one is to calculate the end time after calling foo() N times and them divide N. I think the second one is better because when the time calling foo() is short, each latency in loop is so hard to calculate, which may cause some uncontrolled problem.


## Question 5
Consider the following code.  What work is this code doing, besides calling
`foo()`?

```c
int i;
for (i = 0; i < N; ++i) {
  foo();
}
```

### Answer:
i is added from 0 to N-1, which consists of N loops to call foo() function. There are one initialize i as 0 and N times compare between i and N and N times addition for i.


## Question 6
If `foo()` is very fast, and the time to perform `for (i = 0; i < N; ++i)` may
be significant (relative to the time needed to call `foo()`), how could you make
your final measurement value includes *only* the average time required to call
`foo()`?

### Answer:
We can measure the time cost of executing  `for (i = 0; i < N; ++i)`  without calling foo() function. The difference will be the time of calling foo() N times.


# Part B
Now that you've run all your experiments, answer the questions in "Part B".
You should **complete these questions on your own**, without discussing the
answers with anyone (unless you have questions for the instructor or TAs, of
course). Each question should only require approximately *a couple sentences*
to answer properly, so don't write an entire that isn't needed.


## Question 7
What was your general strategy for getting good measurements for all the
items?  (i.e., things you did in common for all of them, not the one-off
adjustments you had to figure out just for specific ones)

### Answer:
I will record start time before loop and end time after loop. In 'for' loop, I will call the specific function. Finally, calculate the latency betweern start and end time, decrease empty loop time, and them divide the times of loop, which will be my final measurements.

## Question 8
What measurement result did you get for each of the six measurements?  Based on
these results, which functions do you think are system calls (syscalls) and
which do you think are not?

### Answer:

* allocate one page of memory with `mmap()`
  * user time:       **[0.034630 us]**
  * system time:     **[0.287440 us]**

* lock a mutex with `pthread_mutex_lock()`
  * user time:       **[0.007915 us]**
  * system time:     **[0.000995 us]**

* writing 4096 Bytes directly (bypassing the disk page cache) to /tmp
  * wall-clock time: **[866.064600 us]**

* reading 4096 Bytes directly (bypassing the disk page cache) from /tmp
  * wall-clock time: **[288.397500 us]**

* writing 4096 Bytes to the disk page cache
  * wall-clock time: **[2.977700 us]**

* reading 4096 Bytes from the disk page cache
  * wall-clock time: **[1.010400 us]**


* Syscalls:     **[mmap(),pthread_mutex_lock(),open(),write(),read()]**
* Not syscalls: **[]**


## Question 9
What is the memory page size?  (i.e., that you used with `mmap`)

### Answer:
4096, I use function called getpagesize().


## Question 10
How did you deal with the problem of not being able to lock a mutex
more than once without unlocking it first?

### Answer:
We can declare an mutex array and we lock each mutex in loop to meet the requirement.


## Question 11
Was performance affected by whether a file access operation is a read or write?
If so, how?

### Answer:
Yes, from my measurement, reading is faster than writing.

## Question 12
What affect did the disk page cache have on file access performance?  Did it
affect reads and write differently?

### Answer:
When I use O_DIRECT|O_SYNC flags to bypass disk cache, the average of writing and reading speed is slower a lot than using disk page cache, which means the time of bypassing the disk page cache will larger.  No, disk page cache save read and write a lot of time.
