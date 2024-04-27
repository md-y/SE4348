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

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <job-file>\n", argv[0]);
    return 1;
  }

  int err_code;

  struct Job jobs[MAX_JOBS];
  unsigned int job_count = 0;
  if ((err_code = read_jobs(argv[1], jobs, &job_count)) != 0) {
    printf("Error reading job file \"%s\". Code: %d\n", argv[1], err_code);
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
 * Read jobs from a file and store them in an array and the count.
 * Returns 0 for success, -1 if the file fails to open, -2 for invalid format.
 */
int read_jobs(char *path, struct Job *jobs, unsigned int *job_count) {
  FILE *job_file = fopen(path, "r");
  if (job_file == NULL) {
    return -1; // Failed to open file
  }

  for (int i = 0;; i++) {
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
        return -2; // Invalid format
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
  for (int job_idx = 0; job_idx < count; job_idx++) {
    struct Job job = jobs[job_idx];
    for (int x_idx = 0; x_idx < job.duration; x_idx++) {
      // Pad X with spaces based on the job index
      printf("%*sX\n", job_idx, "");
    }
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
  }

  // Job index 0 is guaranteed to be running first
  int queue[count];
  int front = 0, rear = 1;
  int next_job = 1;
  queue[0] = 0;

  for (int i = 0; i < total_duration; i++) {
    // Dequeue job
    int job_idx = queue[front];
    front = (front + 1) % count;

    durations[job_idx]--;
    printf("%*sX\n", job_idx, "");

    // Enqueue next job if it arrives next
    if (next_job < count && jobs[next_job].arrival_time == i + 1) {
      queue[rear] = next_job;
      rear = (rear + 1) % count;
      next_job++;
    }

    // Re-enqueue job
    if (durations[job_idx] > 0) {
      queue[rear] = job_idx;
      rear = (rear + 1) % count;
    }
  }
}
