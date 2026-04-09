#include "kernel/types.h"
#include "user/user.h"

int
main(void)
{
  while(1){
    int temp = getsensorstat(0);
    int power = getsensorstat(1);

    printf("readsensors: temp=%d power=%d\n", temp, power);
    for(volatile int i = 0; i < 500000000; i++);
  }

  exit(0);
}
