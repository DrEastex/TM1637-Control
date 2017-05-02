#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "led_control.h"

int main() {
  InitLed();
  TimeData_T cur_time;

  struct tm* p;
  time_t timep;
  int tempMin = -1;
  while( 1) {
    time(&timep);
    p = localtime(&timep);
    //printf("tempMin=%d\r\n", tempMin);
    if ( /*p->tm_min == tempMin*/0 ) {
      FlashOpts(p->tm_min);
      printf("0");
    } else {
      cur_time.hour = p->tm_hour;
      cur_time.min = p->tm_min;
      SendTime(&cur_time);
      tempMin = p->tm_min;
    }
    usleep(200000);
    
  }

  return 0;

}
