#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include "led_control.h"

int main() {
  InitLed();
  while( 1) {
    sleep(1);
  }



}
