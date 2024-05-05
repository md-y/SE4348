#include <stdio.h>

#define MAX_JOBS 100

struct Job {
  char name;
  unsigned int arrival_time;
  unsigned int duration;
};

int read_jobs(char *path, struct Job *jobs, unsigned int *job_count);
void fcfs(struct Job *jobs, unsigned int count);
void round_robin(struct Job *jobs, unsigned int count);

/**
 * Main function. Only orchestrates reading jobs, calling the scheduling, and
 * printing errors.
 */
int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <job-file>\n", argv[0]);
    return 1;
  }

  int err_code;
  struct Job jobs[MAX_JOBS];
  unsigned int job_count = 0;
  if ((err_code = read_jobs(argv[1], jobs, &job_count)) != 0) {
    // Print messages outside of read_jobs since it should only return the code
    switch (err_code) {
    case -1:
      printf("Failed to open file: %s\n", argv[1]);
      break;
    case -2:
      printf("Invalid job file format.\n");
      break;
    case -3:
      printf("Invalid arrival time (each job needs to be after the next).\n");
      break;
    }
    return err_code;
  }

  if (job_count == 0) {
    printf("No jobs found in file: %s\n", argv[1]);
    return 1;
  }

  fcfs(jobs, job_count);
  printf("\n");
  round_robin(jobs, job_count);

  return 0;
}

/**
 * Read jobs from a file and store them in an array along with the count.
 * Returns 0 for success, -1 if the file fails to open, -2 for invalid format,
 * or -3 for invalid arrival time sequence.
 */
int read_jobs(char *path, struct Job *jobs, unsigned int *job_count) {
  FILE *job_file = fopen(path, "r");
  if (job_file == NULL) {
    return -1; // Failed to open file
  }

  // Read each line, but stop at MAX_JOBS to prevent overflow
  for (int i = 0; i < MAX_JOBS; i++) {
    struct Job new_job;
    int res = fscanf(job_file, "%c%*[\t ]%u%*[\t ]%u%*[\n]", &new_job.name,
                     &new_job.arrival_time, &new_job.duration);

    if (res == EOF) {
      *job_count = i;
      break;
    } else if (res < 3) {
      fclose(job_file);
      return -2; // Invalid format
    }

    // Check job is valid based on last job
    if (i > 0) {
      struct Job last_job = jobs[i - 1];
      // Job arrival order must be in increasing order
      if (new_job.arrival_time < last_job.arrival_time) {
        fclose(job_file);
        return -3; // Invalid arrival time
      }
    }

    jobs[i] = new_job;
  }

  fclose(job_file);
  return 0;
}

/**
 * Performs FCFS scheduling on the jobs and prints the results.
 */
void fcfs(struct Job *jobs, unsigned int count) {
  printf("FCFS\n");

  // Print header
  for (int i = 0; i < count; i++) {
    struct Job job = jobs[i];
    printf("%c", job.name);
  }
  printf("\n");

  // According to the requirements, the jobs are already sorted by arrival time
  // So we can just iterate through the jobs and print the details
  for (int job_idx = 0, time = 0; job_idx < count; job_idx++) {
    struct Job job = jobs[job_idx];

    // Print empty spaces if there is a gap between jobs
    for (; time < job.arrival_time; time++) {
      printf("\n");
    }

    for (int x_idx = 0; x_idx < job.duration; x_idx++) {
      // Pad X with spaces based on the job index
      printf("%*sX\n", job_idx, "");
    }

    time += job.duration;
  }
}

/**
 * Performs Round Robin scheduling on the jobs and prints the results.
 * Assumes quantum size of 1.
 */
void round_robin(struct Job *jobs, unsigned int count) {
  printf("RR\n");

  // Print header
  for (int i = 0; i < count; i++) {
    struct Job job = jobs[i];
    printf("%c", job.name);
  }
  printf("\n");

  int durations[count], total_duration = 0;

  // Preprocess job durations
  for (int i = 0; i < count; i++) {
    durations[i] = jobs[i].duration;
    total_duration += jobs[i].duration;
    if (i > 0) {
      int gap = jobs[i].arrival_time -
                (jobs[i - 1].arrival_time + jobs[i - 1].duration);
      if (gap > 0) {
        total_duration += gap;
      }
    }
  }

  // Job index 0 is guaranteed to be running first
  int queue[count];
  int front = 0, rear = 1;
  int next_job = 1;
  queue[0] = 0;

  for (int i = 0; i < total_duration; i++) {
    int job_idx = -1;

    // Dequeue job if it exists
    if (front != rear) {
      job_idx = queue[front];
      front = (front + 1) % count;

      durations[job_idx]--;
      printf("%*sX\n", job_idx, "");
    } else {
      printf("\n");
    }

    // Enqueue next job if it arrives next
    if (next_job < count && jobs[next_job].arrival_time == i + 1) {
      queue[rear] = next_job;
      rear = (rear + 1) % count;
      next_job++;
    }

    // Re-enqueue job
    if (job_idx >= 0 && durations[job_idx] > 0) {
      queue[rear] = job_idx;
      rear = (rear + 1) % count;
    }
  }
}
