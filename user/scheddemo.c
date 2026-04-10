/*
 * scheddemo.c -- Feature 3: Eco-Aware Scheduler demonstration.
 *
 * Spawns several CPU-heavy "worker" child processes that spin and
 * count iterations.  Meanwhile, the parent drives the sensor values
 * through NORMAL -> ECO -> CRITICAL -> back to NORMAL so you can
 * observe the scheduler throttling heavy non-essential processes.
 *
 * Expected behaviour:
 *   NORMAL   : all workers get roughly equal CPU (high iteration counts).
 *   ECO      : workers are throttled -- iteration counts drop ~50 %.
 *   CRITICAL : workers are heavily throttled -- iteration counts drop ~75 %.
 *
 * Constants mirror kernel/proc.h (we can't include kernel headers).
 */

#include "kernel/types.h"
#include "user/user.h"

/* Sensor type constants */
#define SENSOR_TEMP  0
#define SENSOR_POWER 1

/* Eco state constants */
#define ECO_NORMAL   0
#define ECO_ECO      1
#define ECO_CRITICAL 2

#define NUM_WORKERS  3
#define COUNTING_ITERS 80000000  /* iterations per measurement window */

static const char *
state_name(int s)
{
  if(s == ECO_CRITICAL) return "CRITICAL";
  if(s == ECO_ECO)      return "ECO";
  return "NORMAL";
}

/* Busy-wait delay that is visible to the scheduler (burns CPU). */
static void
delay(int n)
{
  for(volatile int i = 0; i < n; i++)
    ;
}

/*
 * Worker child: spins forever, periodically printing how many
 * iterations it completed.  Throttled workers will report fewer
 * iterations per window because the scheduler gives them less CPU.
 */
static void
worker(int id)
{
  int round = 0;
  while(1){
    volatile int count = 0;
    for(int i = 0; i < COUNTING_ITERS; i++)
      count++;
    round++;
    printf("[worker %d] round %d  iters=%d\n", id, round, count);
  }
}

int
main(void)
{
  int pids[NUM_WORKERS];

  printf("=== scheddemo: Feature 3 -- Eco-Aware Scheduler ===\n\n");
  printf("Spawning %d CPU-heavy workers...\n", NUM_WORKERS);

  /* Fork worker children */
  for(int i = 0; i < NUM_WORKERS; i++){
    int pid = fork();
    if(pid < 0){
      printf("scheddemo: fork failed\n");
      exit(1);
    }
    if(pid == 0){
      worker(i);
      exit(0);  /* unreachable */
    }
    pids[i] = pid;
  }

  printf("Workers started (PIDs:");
  for(int i = 0; i < NUM_WORKERS; i++)
    printf(" %d", pids[i]);
  printf(")\n\n");

  /* ------- Phase 1: NORMAL (low sensor values) ------- */
  printf("--- Phase 1: NORMAL mode (temp=50, power=30) ---\n");
  updatesensor(SENSOR_TEMP, 50);
  updatesensor(SENSOR_POWER, 30);
  printf("  eco_state = %s\n\n", state_name(getecostate()));
  delay(900000000);  /* let workers run for a while */

  /* ------- Phase 2: ECO (cross eco thresholds) ------- */
  printf("--- Phase 2: ECO mode (temp=72, power=55) ---\n");
  updatesensor(SENSOR_TEMP, 72);
  updatesensor(SENSOR_POWER, 55);
  printf("  eco_state = %s\n", state_name(getecostate()));
  printf("  Workers should be throttled ~50%%\n\n");
  delay(900000000);

  /* ------- Phase 3: CRITICAL (cross critical thresholds) ------- */
  printf("--- Phase 3: CRITICAL mode (temp=90, power=80) ---\n");
  updatesensor(SENSOR_TEMP, 90);
  updatesensor(SENSOR_POWER, 80);
  printf("  eco_state = %s\n", state_name(getecostate()));
  printf("  Workers should be heavily throttled ~75%%\n\n");
  delay(900000000);

  /* ------- Phase 4: back to NORMAL ------- */
  printf("--- Phase 4: back to NORMAL (temp=45, power=25) ---\n");
  updatesensor(SENSOR_TEMP, 45);
  updatesensor(SENSOR_POWER, 25);
  printf("  eco_state = %s\n", state_name(getecostate()));
  printf("  Workers should return to full speed\n\n");
  delay(900000000);

  /* Clean up */
  printf("scheddemo: killing workers...\n");
  for(int i = 0; i < NUM_WORKERS; i++)
    kill(pids[i]);
  for(int i = 0; i < NUM_WORKERS; i++)
    wait(0);

  printf("scheddemo: done.\n");
  exit(0);
}
