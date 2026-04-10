/*
 * sensordemo.c — Feature 1 concurrency demonstration.
 *
 * Forks two child processes:
 *   Child 1 : oscillates temperature (type 0) between 40 and 90.
 *   Child 2 : oscillates power usage (type 1) between 20 and 80.
 *   Parent  : reads both sensors and prints the current values.
 *
 * The busy-loop delays are intentionally asymmetric so the three
 * processes interleave and the output shows living, changing values.
 *
 * Sensor type constants mirror the kernel definitions in proc.h
 * WITHOUT including any kernel header — clean user/kernel separation.
 */

#include "kernel/types.h"
#include "user/user.h"

/* Mirror of SENSOR_TEMP / SENSOR_POWER from kernel/proc.h.
 * User space must NOT include proc.h directly. */
#define SENSOR_TEMP  0
#define SENSOR_POWER 1

/* Busy-wait for approximately 'iters' iterations. */
static void
delay(int iters)
{
  for (volatile int i = 0; i < iters; i++)
    ;
}

/* ──────────────────────────────────────────────────────────
 * Child 1: temperature sensor loop (40 – 90 °C)
 * ────────────────────────────────────────────────────────── */
static void
run_temp_sensor(void)
{
  int temp      = 50;
  int direction = 1;

  while (1) {
    updatesensor(SENSOR_TEMP, temp);
    printf("[tempsensor] temperature updated -> %d C\n", temp);

    temp += direction * 5;
    if (temp >= 90) direction = -1;
    if (temp <= 40) direction =  1;

    delay(700000000);
  }
}

/* ──────────────────────────────────────────────────────────
 * Child 2: power-usage sensor loop (20 – 80 W)
 * ────────────────────────────────────────────────────────── */
static void
run_power_sensor(void)
{
  int power     = 30;
  int direction = 1;

  while (1) {
    updatesensor(SENSOR_POWER, power);
    printf("[powersensor] power updated -> %dW\n", power);

    power += direction * 4;
    if (power >= 80) direction = -1;
    if (power <= 20) direction =  1;

    delay(860000000);
  }
}

/* ──────────────────────────────────────────────────────────
 * Parent: sensor read loop
 * ────────────────────────────────────────────────────────── */
static void
run_read_loop(void)
{
  while (1) {
    int temp  = getsensorstat(SENSOR_TEMP);
    int power = getsensorstat(SENSOR_POWER);
    printf("[readsensors] temp=%d C  power=%dW\n", temp, power);

    delay(500000000);
  }
}

/* ──────────────────────────────────────────────────────────
 * main: fork children, run roles
 * ────────────────────────────────────────────────────────── */
int
main(void)
{
  printf("sensordemo: starting - forking sensor processes...\n");

  /* Fork Child 1: temperature sensor */
  int pid1 = fork();
  if (pid1 < 0) {
    printf("sensordemo: fork failed for temp sensor\n");
    exit(1);
  }
  if (pid1 == 0) {
    /* ── Child 1 ── */
    run_temp_sensor();
    exit(0); /* unreachable */
  }

  /* Fork Child 2: power sensor */
  int pid2 = fork();
  if (pid2 < 0) {
    printf("sensordemo: fork failed for power sensor\n");
    kill(pid1);
    exit(1);
  }
  if (pid2 == 0) {
    /* ── Child 2 ── */
    run_power_sensor();
    exit(0); /* unreachable */
  }

  /* ── Parent: read loop ── */
  printf("sensordemo: child PIDs - temp=%d  power=%d\n", pid1, pid2);
  run_read_loop();

  /* unreachable in normal operation */
  wait(0);
  wait(0);
  exit(0);
}
