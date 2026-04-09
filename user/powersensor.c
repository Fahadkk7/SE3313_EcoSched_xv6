#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int power = 30;
  int direction = 1;

  while(1){
    updatesensor(1, power);
    printf("powersensor: power = %d\n", power);

    power += direction * 4;

    if(power >= 80)
      direction = -1;
    if(power <= 20)
      direction = 1;

    for(volatile int i = 0; i < 860000000; i++);
  }

  exit(0);
}
