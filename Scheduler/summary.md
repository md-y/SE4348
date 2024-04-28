## Initial Plan

I chose to use C for this project because I felt it was similar to project 1, and I enjoyed using C for that project. I think this was a good choice because development was mostly smooth, and the code was less verbose compared to Java.

The structure of the program was also similar to project 1. Essentially, there was a function that read in the job file, and this information would be passed to two display functions (FCFS and RR). This structure ended up working perfectly.

## Implementation

The first function I developed was the job file parser. This was the obvious choice since the jobs need to be parsed before the visualizations can be shown. This process was actually quite simple since I was able to use the `fscanf` function to parse each line. The function looks for a character and two unsigned integers separated by tabs (`\t`) or spaces.

The `FCFS` implementation is also rather simple. Since each job is ordered in the file by arrival date according to the project requirements, each job can be printed in the order it appears in the file.

Each `X` was printed using:

```c
printf("%*sX\n", job_idx, "");
```

Essentially, this prints `X` padded by `job_idx` spaces. This works since string substitutions can be prefixed by any number of spaces using `%*s`. Although, the actual string that is substituted must be nothing (`""`). The padding length is `job_idx` since the schedule is printed vertically, and `job_idx` represents the job's column.

The `RR` implementation was more complicated since it needed a queue to keep track of the jobs. Since C has no queue implementation, I made one using an array. Since the maximum number of items is the same as the number of jobs, the array is initialized to that length. Two index numbers (front and rear) are used to point to the start and end of the queue. These numbers can be used to queue and dequeue.

Because `RR` only needs to work with a quantum size of one, each job is immediately re-added to the queue once it runs for one time period. Each time period also causes the remaining duration of the job to be decremented. Theses durations are stored in an array indexed by each job ID. This makes it simple to track the remaining time without modifying the original job array.

The `RR` function uses the same printing method as `FCFS`.

## Challenges

Most of the project was implemented smoothly except `RR`. This was because I initially added new jobs after re-queued jobs instead of before, so I kept getting the wrong output. However, when I fixed this, I kept getting segmentation faults. This is because the queue starts with the first job already added, but I forgot to increment the rear index. Then whenever the queue was used after being dequeued, it would use a random integer value stored in an unused queue index with the job array, causing a random memory address to be accessed. After I fixed this, the implementation worked perfectly, but it did remind me of why using C can be challenging.

## Lessons Learned

The most interesting thing I learned was how to implement queues in C. I already knew the theory behind making queues using arrays, but this was the first time I actually did it so it was very enlightening.

Of course, I also learned how `FCFS` and `RR` scheduling work.

Overall, I am proud of the end result, and I think it works wonderfully.
