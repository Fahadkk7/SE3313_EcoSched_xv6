 /*
 * sensordemo.c -- Feature 1 + Feature 2 combined demonstration.
 *
 * Forks two child processes:
 *   Child 1 : oscillates temperature (type 0) between 40 and 90.
 *   Child 2 : oscillates power usage (type 1) between 20 and 80.
 *   Parent  : reads both sensors AND the kernel eco state, prints all three.
 *
 * Eco state transitions are driven by the kernel sustainability monitor
 * (Feature 2) and become visible as sensor values cross thresholds:
 *   CRITICAL : temp >= 85  OR  power >= 75
 *   ECO      : temp >= 70  OR  power >= 50
 *   NORMAL   : otherwise
 *
 * Constants mirror kernel/proc.h without including any kernel header.
 */

#include "kernel/types.h"
#include "user/user.h"

/* Sensor type constants -- mirror of kernel/proc.h */
#define SENSOR_TEMP  0
#define SENSOR_POWER 1

/* Eco state constants -- mirror of kernel/proc.h */
#define ECO_NORMAL   0
#define ECO_ECO      1
#define ECO_CRITICAL 2

/* Map the eco state integer returned by getecostate() to a label. */
static const char *
state_name(int state)
{
  if(state == ECO_CRITICAL) return "CRITICAL";
  if(state == ECO_ECO)      return "ECO";
  return "NORMAL";
}

/* Busy-wait delay. */
static void
delay(int iters)
{
  for(volatile int i = 0; i < iters; i++)
    ;
}

/* ----------------------------------------------------------
 * Child 1: temperature sensor loop (40 - 90 C)
 * ---------------------------------------------------------- */
static void
run_temp_sensor(void)
{
  int temp      = 50;
  int direction = 1;

  while(1){
    updatesensor(SENSOR_TEMP, temp);
    printf("[tempsensor] temperature -> %d C\n", temp);

    temp += direction * 5;
    if(temp >= 90) direction = -1;
    if(temp <= 40) direction =  1;

    delay(700000000);
  }
}

/* ----------------------------------------------------------
 * Child 2: power-usage sensor loop (20 - 80 W)
 * ---------------------------------------------------------- */
static void
run_power_sensor(void)
{
  int power     = 30;
  int direction = 1;

  while(1){
    updatesensor(SENSOR_POWER, power);
    printf("[powersensor] power -> %dW\n", power);

    power += direction * 4;
    if(power >= 80) direction = -1;
    if(power <= 20) direction =  1;

    delay(860000000);
  }
}

/* ----------------------------------------------------------
 * Parent: read loop -- shows sensors AND eco state together
 * ---------------------------------------------------------- */
static void
run_read_loop(void)
{
  while(1){
    int temp  = getsensorstat(SENSOR_TEMP);
    int power = getsensorstat(SENSOR_POWER);
    int state = getecostate();

    printf("[monitor] temp=%d C  power=%dW  state=%s\n",
           temp, power, state_name(state));

    delay(500000000);
  }
}

/* ----------------------------------------------------------
 * main: fork children, parent runs the monitor loop
 * ---------------------------------------------------------- */
int
main(void)
{
  printf("sensordemo: starting (Feature 1 + Feature 2)...\n");
  printf("  Thresholds: CRITICAL=temp>=85||power>=75  ECO=temp>=70||power>=50\n\n");

  /* Fork Child 1: temperature sensor */
  int pid1 = fork();
  if(pid1 < 0){
    printf("sensordemo: fork failed for temp sensor\n");
    exit(1);
  }
  if(pid1 == 0){
    run_temp_sensor();
    exit(0);
  }

  /* Fork Child 2: power sensor */
  int pid2 = fork();
  if(pid2 < 0){
    printf("sensordemo: fork failed for power sensor\n");
    kill(pid1);
    exit(1);
  }
  if(pid2 == 0){
    run_power_sensor();
    exit(0);
  }

  /* Parent: monitor loop */
  printf("sensordemo: child PIDs - temp=%d  power=%d\n\n", pid1, pid2);
  run_read_loop();

  /* unreachable */
  wait(0);
  wait(0);
  exit(0);
}
