
#include "kernel/types.h"
#include "user/user.h"

/* Mirror kernel/proc.h constants */
#define SENSOR_TEMP  0
#define SENSOR_POWER 1
#define ECO_NORMAL   0
#define ECO_ECO      1
#define ECO_CRITICAL 2

#define NUM_HEAVY    2
#define NUM_LIGHT    2
#define TOTAL        (NUM_HEAVY + NUM_LIGHT)
#define WINDOW_TICKS 20   /* measure iterations over this many timer ticks */

static const char *
state_name(int s)
{
  if(s == ECO_CRITICAL) return "CRITICAL";
  if(s == ECO_ECO)      return "ECO";
  return "NORMAL";
}

/* ------------------------------------------------------------------ *
 * Heavy worker
 * Always CPU-bound. Burns cpu_ticks every timer tick -> loses credits.
 * In ECO/CRITICAL: throttled by scheduler AND loses credit comparison.
 * Reports iterations completed per WINDOW_TICKS.
 * ------------------------------------------------------------------ */
static void __attribute__((noreturn))
heavy_worker(int id)
{
  /* Stagger startup so workers don't all print at the same instant. */
  pause(id * 5 + 1);

  int round = 0;
  for(;;){
    uint t0 = uptime();
    int  cnt = 0;
    /* Spin until WINDOW_TICKS of wall time elapses. */
    for(;;){
      for(volatile int i = 0; i < 10000; i++) cnt++;
      if(uptime() - t0 >= WINDOW_TICKS) break;
    }
    round++;
    printf("[heavy %d] pid=%d  round=%d  iters=%d\n",
           id, getpid(), round, cnt);
    pause(2);  /* yield so console can drain */
  }
}

/* ------------------------------------------------------------------ *
 * Light worker
 * Phase 1: work a tiny bit then sleep (pause).
 *          Low cpu_ticks -> gains eco_credits each window.
 * Phase 2: detects eco_state != NORMAL via getecostate(), stops sleeping,
 *          becomes CPU-bound like heavy worker.
 *          At switch point: cpu_ticks ~= 0 (not throttled), credits ~= 8-9.
 *          -> Wins EVERY scheduler pick in ECO/CRITICAL mode.
 * ------------------------------------------------------------------ */
static void __attribute__((noreturn))
light_worker(int id)
{
  /* Stagger startup */
  pause(id * 5 + 3);

  /* --- Phase 1: build credits by sleeping --- */
  for(;;){
    volatile int x = 0;
    for(int i = 0; i < 5000; i++) x++;

    pause(20);   /* sleep 20 ticks: almost zero cpu_ticks accumulated */
    printf("[light %d] pid=%d  slept  (building credits)\n", id, getpid());
    pause(2);  /* yield so console can drain */

    /* Switch as soon as the parent puts the system into ECO or CRITICAL. */
    if(getecostate() != ECO_NORMAL)
      break;
  }

  /* Announce transition */
  printf("[light %d] pid=%d  *** ECO detected! credits=%d ***\n",
         id, getpid(), getecocredits(getpid()));
  pause(2);

  /* --- Phase 2+: CPU-bound, measure iters per window --- */
  int round = 0;
  for(;;){
    uint t0 = uptime();
    int  cnt = 0;
    for(;;){
      for(volatile int i = 0; i < 10000; i++) cnt++;
      if(uptime() - t0 >= WINDOW_TICKS) break;
    }
    round++;
    printf("[light %d] pid=%d  round=%d  iters=%d\n",
           id, getpid(), round, cnt);
    pause(2);  /* yield so console can drain */
  }
}

/* ------------------------------------------------------------------ *
 * main
 * ------------------------------------------------------------------ */
int
main(void)
{
  int pids[TOTAL];
  int kind[TOTAL];   /* 0=heavy, 1=light */

  printf("============================================\n");
  printf("  creditdemo: Feature 3 + Feature 4 proof  \n");
  printf("============================================\n\n");
  printf("HOW TO READ:\n");
  printf("  iters = iterations completed in a %d-tick window\n", WINDOW_TICKS);
  printf("  In ECO/CRITICAL: light iters >> heavy iters = scheduler working\n\n");

  /* Start in NORMAL state */
  updatesensor(SENSOR_TEMP, 50);
  updatesensor(SENSOR_POWER, 30);
  printf("Starting eco_state = %s\n\n", state_name(getecostate()));

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

  printf("PIDs:");
  for(int i = 0; i < TOTAL; i++)
    printf(" %d(%s)", pids[i], kind[i] ? "light" : "heavy");
  printf("\n\n");

  /* -------- PHASE 1: NORMAL -- let credits diverge -------- */
  printf("**** PHASE 1: NORMAL mode  (credits diverging) ****\n");
  printf("     Scheduler: plain round-robin, all processes equal\n\n");

  for(int snap = 0; snap < 4; snap++){
    pause(40);
    printf("\n  -- credit snapshot %d --\n", snap + 1);
    for(int i = 0; i < TOTAL; i++){
      int cr = getecocredits(pids[i]);
      printf("    pid %d (%s): eco_credits = %d\n",
             pids[i], kind[i] ? "light" : "heavy", cr);
    }
    printf("\n");
    pause(3);
  }

  /* -------- PHASE 2: ECO -- scheduler picks highest-credit -------- */
  printf("\n**** PHASE 2: ECO mode  (temp=72, power=55) ****\n");
  updatesensor(SENSOR_TEMP, 72);
  updatesensor(SENSOR_POWER, 55);
  printf("**** eco_state = %s ****\n", state_name(getecostate()));
  printf("     Light workers detect ECO and go CPU-bound.\n");
  printf("     Expect: light iters >> heavy iters\n\n");

  for(int snap = 0; snap < 3; snap++){
    pause(40);
    printf("\n  -- credit snapshot %d --\n", snap + 1);
    for(int i = 0; i < TOTAL; i++){
      int cr = getecocredits(pids[i]);
      printf("    pid %d (%s): eco_credits = %d\n",
             pids[i], kind[i] ? "light" : "heavy", cr);
    }
    printf("\n");
    pause(3);
  }

  /* -------- PHASE 3: CRITICAL -- heavy workers nearly starved -------- */
  printf("\n**** PHASE 3: CRITICAL mode  (temp=90, power=80) ****\n");
  updatesensor(SENSOR_TEMP, 90);
  updatesensor(SENSOR_POWER, 80);
  printf("**** eco_state = %s ****\n", state_name(getecostate()));
  printf("     Heavy: 1/4 scheduler passes + lowest credit\n");
  printf("     Expect: heavy iters very low\n\n");

  for(int snap = 0; snap < 2; snap++){
    pause(40);
    printf("\n  -- credit snapshot %d --\n", snap + 1);
    for(int i = 0; i < TOTAL; i++){
      int cr = getecocredits(pids[i]);
      printf("    pid %d (%s): eco_credits = %d\n",
             pids[i], kind[i] ? "light" : "heavy", cr);
    }
    printf("\n");
    pause(3);
  }

  /* -------- Cleanup -------- */
  printf("\n**** Restoring NORMAL and exiting ****\n");
  updatesensor(SENSOR_TEMP, 45);
  updatesensor(SENSOR_POWER, 25);

  for(int i = 0; i < TOTAL; i++) kill(pids[i]);
  for(int i = 0; i < TOTAL; i++) wait(0);

  printf("creditdemo: done.\n");
  exit(0);
}
