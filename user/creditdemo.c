/*
 * creditdemo.c -- Feature 4: Eco-Credit System demonstration.
 *
 * Spawns two kinds of child processes:
 *   - "heavy" workers that spin-loop burning CPU non-stop.
 *   - "light" workers that frequently sleep (via pause()), simulating
 *     efficient, intermittent workloads.
 *
 * The parent queries each child's eco_credits every few seconds so you
 * can observe how the credit system rewards light processes and
 * penalises heavy ones.  It then puts the system into ECO mode to show
 * that the scheduler prefers the high-credit (light) processes.
 *
 * Expected behaviour:
 *   - All processes start at 5 credits.
 *   - After a few windows, heavy processes drift toward 0 credits.
 *   - Light processes drift toward 10 credits.
 *   - In ECO mode, light processes visibly get more CPU time.
 *
 * Constants mirror kernel/proc.h (cannot include kernel headers).
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

#define NUM_HEAVY 2
#define NUM_LIGHT 2
#define TOTAL     (NUM_HEAVY + NUM_LIGHT)

static const char *
state_name(int s)
{
  if(s == ECO_CRITICAL) return "CRITICAL";
  if(s == ECO_ECO)      return "ECO";
  return "NORMAL";
}

/*
 * Heavy worker: pure user-mode spin-loop.
 * Burns CPU continuously in user mode → timer interrupts land in
 * usertrap() → eco_credit_update() increments credit_cpu_ticks →
 * credits decrease over time.
 *
 * IMPORTANT: Do NOT call uptime() in a tight loop here!
 * uptime() is a syscall that puts the process in kernel mode.
 * Timer interrupts in kernel mode go to kerneltrap(), which does
 * NOT update eco credits.
 */
static void __attribute__((noreturn))
heavy_worker(int id)
{
  int round = 0;
  for(;;){
    volatile int count = 0;
    for(int i = 0; i < 50000000; i++)
      count++;
    round++;
    // Only print every 10 rounds to keep output readable.
    // The process still burns full CPU between prints.
    if((round % 10) == 0)
      printf("[heavy %d] pid=%d  round=%d\n", id, getpid(), round);
  }
}

/*
 * Light worker: does a small amount of work then sleeps.
 * Uses little CPU → should gain credits over time.
 */
static void __attribute__((noreturn))
light_worker(int id)
{
  for(;;){
    volatile int count = 0;
    // Do a tiny bit of work
    for(int i = 0; i < 5000; i++)
      count++;
    // Then sleep for a while (pause = sleep for N ticks)
    pause(20);
    printf("[light %d] pid=%d  iters=%d (slept)\n", id, getpid(), count);
  }
}

int
main(void)
{
  int pids[TOTAL];
  int kind[TOTAL];   // 0 = heavy, 1 = light

  printf("========================================\n");
  printf("  creditdemo: Feature 4 -- Eco-Credits  \n");
  printf("========================================\n\n");

  /* Ensure we start in NORMAL mode */
  updatesensor(SENSOR_TEMP, 50);
  updatesensor(SENSOR_POWER, 30);
  printf("eco_state = %s (starting in NORMAL)\n\n", state_name(getecostate()));

  /* Spawn heavy workers */
  for(int i = 0; i < NUM_HEAVY; i++){
    int pid = fork();
    if(pid < 0){ printf("fork failed\n"); exit(1); }
    if(pid == 0) heavy_worker(i);
    pids[i] = pid;
    kind[i] = 0;
  }

  /* Spawn light workers */
  for(int i = 0; i < NUM_LIGHT; i++){
    int pid = fork();
    if(pid < 0){ printf("fork failed\n"); exit(1); }
    if(pid == 0) light_worker(i);
    pids[NUM_HEAVY + i] = pid;
    kind[NUM_HEAVY + i] = 1;
  }

  printf("Spawned %d heavy + %d light workers.\n", NUM_HEAVY, NUM_LIGHT);
  printf("PIDs:");
  for(int i = 0; i < TOTAL; i++)
    printf(" %d(%s)", pids[i], kind[i] ? "light" : "heavy");
  printf("\n\n");

  /* -------- Phase 1: NORMAL – let credits diverge -------- */
  printf("--- Phase 1: NORMAL mode – watching credits diverge ---\n");
  for(int round = 0; round < 4; round++){
    pause(30);  // wait ~3 seconds
    printf("  [credit snapshot round %d]\n", round + 1);
    for(int i = 0; i < TOTAL; i++){
      int cr = getecocredits(pids[i]);
      printf("    pid %d (%s): eco_credits = %d\n",
             pids[i], kind[i] ? "light" : "heavy", cr);
    }
  }

  /* -------- Phase 2: ECO mode – scheduler prefers high-credit -------- */
  printf("\n");
  printf("**** PHASE 2: ECO mode (temp=72, power=55) ****\n");
  updatesensor(SENSOR_TEMP, 72);
  updatesensor(SENSOR_POWER, 55);
  printf("**** eco_state = %s  (scheduler now credit-aware) ****\n\n", state_name(getecostate()));

  for(int round = 0; round < 3; round++){
    pause(30);
    printf("  [credit snapshot round %d]\n", round + 1);
    for(int i = 0; i < TOTAL; i++){
      int cr = getecocredits(pids[i]);
      printf("    pid %d (%s): eco_credits = %d\n",
             pids[i], kind[i] ? "light" : "heavy", cr);
    }
  }

  /* -------- Phase 3: CRITICAL mode -------- */
  printf("\n");
  printf("**** PHASE 3: CRITICAL mode (temp=90, power=80) ****\n");
  updatesensor(SENSOR_TEMP, 90);
  updatesensor(SENSOR_POWER, 80);
  printf("**** eco_state = %s  (heavy workers strongly throttled) ****\n\n", state_name(getecostate()));

  for(int round = 0; round < 2; round++){
    pause(30);
    printf("  [credit snapshot round %d]\n", round + 1);
    for(int i = 0; i < TOTAL; i++){
      int cr = getecocredits(pids[i]);
      printf("    pid %d (%s): eco_credits = %d\n",
             pids[i], kind[i] ? "light" : "heavy", cr);
    }
  }

  /* -------- Clean up -------- */
  printf("\n--- Restoring NORMAL and cleaning up ---\n");
  updatesensor(SENSOR_TEMP, 45);
  updatesensor(SENSOR_POWER, 25);

  for(int i = 0; i < TOTAL; i++)
    kill(pids[i]);
  for(int i = 0; i < TOTAL; i++)
    wait(0);

  printf("\ncreditdemo: done.\n");
  exit(0);
}
