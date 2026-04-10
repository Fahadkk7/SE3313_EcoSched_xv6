/*
 * readecostate.c -- Feature 2: Kernel Sustainability Monitor debug program.
 *
 * Continuously reads both sensor values and the kernel eco state, then
 * prints them together so the state transitions are plainly visible as
 * the background sensor processes drive temperature and power up/down.
 *
 * Eco state integer constants mirror kernel/proc.h without including it:
 *   0 = NORMAL
 *   1 = ECO
 *   2 = CRITICAL
 *
 * Threshold summary (for reference in output):
 *   CRITICAL : temp >= 85  OR  power >= 75
 *   ECO      : temp >= 70  OR  power >= 50
 *   NORMAL   : otherwise
 */

#include "kernel/types.h"
#include "user/user.h"

/* Mirror of sensor type constants -- no proc.h included. */
#define SENSOR_TEMP  0
#define SENSOR_POWER 1

/* Mirror of eco state constants. */
#define ECO_NORMAL   0
#define ECO_ECO      1
#define ECO_CRITICAL 2

static const char *
state_name(int state)
{
  if(state == ECO_CRITICAL) return "CRITICAL";
  if(state == ECO_ECO)      return "ECO";
  return "NORMAL";
}

int
main(void)
{
  printf("readecostate: monitoring kernel sustainability state...\n");
  printf("  CRITICAL : temp>=85 OR power>=75\n");
  printf("  ECO      : temp>=70 OR power>=50\n");
  printf("  NORMAL   : otherwise\n\n");

  while(1){
    int temp  = getsensorstat(SENSOR_TEMP);
    int power = getsensorstat(SENSOR_POWER);
    int state = getecostate();

    printf("temp=%d power=%d state=%s\n", temp, power, state_name(state));

    /* Delay between reads -- shorter than the sensor update loops so
     * we catch every transition without flooding the console. */
    for(volatile int i = 0; i < 400000000; i++);
  }

  exit(0);
}
