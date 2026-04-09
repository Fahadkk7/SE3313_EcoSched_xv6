#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  int temp = 50;
  int direction = 1;

  while(1){
    updatesensor(0, temp);
    printf("tempsensor: temperature = %d\n", temp);

    temp += direction * 5;

    if(temp >= 90)
      direction = -1;
    if(temp <= 40)
      direction = 1;

    for(volatile int i = 0; i < 700000000; i++);
  }

  exit(0);
}
