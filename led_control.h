#ifndef __LED_CONTROL_H__
#define __LED_CONTROL_H__



typedef struct __TIMEDATA {
  unsigned int hour;
  unsigned int min;
}TimeData_T;

int InitLed();
int SendTime(const TimeData_T* const _time);
int DeInitLed();


#endif //__LED_CONTROL_H__
