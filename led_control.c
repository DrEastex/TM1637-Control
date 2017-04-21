#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <inttypes.h>

#include "led_control.h"

//************definitions for TM1637*********************
#define ADDR_AUTO  0x40
#define ADDR_FIXED 0x44

#define STARTADDR  0xc0 
/**** definitions for the clock point of the digit tube *******/
#define POINT_ON   1
#define POINT_OFF  0
/**************definitions for brightness***********************/
#define  BRIGHT_DARKEST 0
#define  BRIGHT_TYPICAL 2
#define  BRIGHTEST      7

//************definitions for ap147 gpio**************************
#define LED_CLK 11
#define LED_DIO 4

#define DATA_HIGH 1
#define DATA_LOW 0


typedef struct __SET_COMMAND{
  int b7:1;
  int b6:1;
  int b5:1;
  int b4:1;
  int b3:1;
  int b2:1;
  int b1:1;
  int b0:1;
}LED_COMMAND1_T;


typedef struct __SET_ADDR{
  int b7:1;
  int b6:1;
  int b5:1;
  int b4:1;
  int b3:1;
  int b2:1;
  int b1:1;
  int b0:1;
}LED_SET_ADDRESS_T;

typedef struct __SET_DISP{
  int b7:1;
  int b6:1;
  int b5:1;
  int b4:1;
  int b3:1;
  int b2:1;
  int b1:1;
  int b0:1;
}LED_DISP_CONTROL_T;

static int8_t TubeTab[] = {0x3f,0x06,0x5b,0x4f,
                           0x66,0x6d,0x7d,0x07,
                           0x7f,0x6f,0x77,0x7c,
                           0x39,0x5e,0x79,0x71};//0~9,A,b,C,d,E,F  

static int Cmd_SetData = 0x0;
static int Cmd_DispCtrl = 0x88+7;
static int Cmd_SetAddr = 0x0;


static int SetPinMode(const int pin, int mode) {
  char cmd[128] = "";
  if ( mode == 0 ) {
    sprintf(cmd, "echo in > /sys/class/gpio/gpio%d/direction", pin);
  } else if (mode == 1 ) {
  sprintf(cmd, "echo out > /sys/class/gpio/gpio%d/direction", pin);
  } else {
  return -1;
  }
  //printf("cmd:'%s'\r\n", cmd);
  system(cmd);
  return 0;
}

static int ReadPinData(const int pin) {
  return 0; 
}

static int ReadDIOData() {
  return ReadPinData(LED_DIO);
}

static int SetPinInput() {
  return SetPinMode(LED_DIO, 0);
}

static int SetPinOutput() {
  return SetPinMode(LED_DIO, 1);
}

static int WriteBit(int port, int data) {
  char cmd[128] = "";
  sprintf(cmd, "echo %d >> /sys/class/gpio/gpio%d/value", data, port);
  printf("cmd:'%s'\r\n", cmd);
  system(cmd);
  return 0;
}

static int WriteDataHigh() {
  return WriteBit(LED_DIO, DATA_HIGH);
}

static int WriteDataLow() {
  return WriteBit(LED_DIO, DATA_LOW);
}

static int DeInactiveClk() {
  return WriteBit(LED_CLK, DATA_HIGH);
}

static int IncativeClk() {
  return WriteBit(LED_CLK, DATA_LOW);
}

static int WriteByte(int8_t wr_data) {
  uint8_t i,count1;   
  for(i=0;i<8;i++)        //sent 8bit data
  {
    IncativeClk();      
    if((wr_data & 0x01)) WriteDataHigh();//LSB first
    else WriteDataLow();
    wr_data >>= 1;      
    DeInactiveClk();
      
  }  
  IncativeClk(); //wait for the ACK
  //WriteDataHigh();
  //DeInactiveClk();   
  DeInactiveClk();
  IncativeClk();
  SetPinInput();
  while(ReadDIOData())    
  { 
    count1 +=1;
    if(count1 == 200)//
    {
     SetPinOutput();
     WriteDataLow();
     count1 =0;
    }
    SetPinInput();
  }
  SetPinOutput();
  WriteDataLow();
  
}

static int Start() {
  DeInactiveClk();//send start signal to TM1637
  WriteDataHigh(); 
  WriteDataLow(); 
  //IncativeClk();
  return 0;
}

static int Stop() {
/*
  IncativeClk();
  WriteDataLow();
  DeInactiveClk();
  WriteDataHigh(); 
*/
  DeInactiveClk();
  WriteDataLow();
  WriteDataHigh();
  
}

static int CodeData(int8_t DispData[]) {
  uint8_t PointData;
  if(/*_PointFlag == POINT_ON*/1)PointData = 0x80;
  else PointData = 0; 
  for(uint8_t i = 0;i < 4;i ++)
  {
    if(DispData[i] == 0x7f)DispData[i] = 0x00;
    else DispData[i] = TubeTab[DispData[i]] + PointData;
  }
}

static int CodeSingleData(int8_t DispData) {
  uint8_t PointData;
  if(/*_PointFlag == POINT_ON*/1)PointData = 0x80;
  else PointData = 0; 
  if(DispData == 0x7f) DispData = 0x00 + PointData;//The bit digital tube off
  else DispData = TubeTab[DispData] + PointData;
  return DispData;

}

static int DisplayDataByAutoAddr(int8_t DispData[]) {
  int8_t SegData[4];
  uint8_t i;
  for(i = 0;i < 4;i ++) {
    SegData[i] = DispData[i];
  }
  CodeData(SegData);
  Start();          //start signal sent to TM1637 from MCU
  WriteByte(ADDR_AUTO);//
  Stop();           //
  Start();          //
  WriteByte(Cmd_SetAddr);//
  for(i=0;i < 4;i ++)
  {
    WriteByte(SegData[i]);        //
  }
  Stop();           //
  Start();          //
  WriteByte(Cmd_DispCtrl);//
  Start();           //  
  return 0;
}

static int DisplayDataByAddr(uint8_t BitAddr,int8_t DispData) {
  int8_t SegData;
  SegData = CodeSingleData(DispData);
  Start();          //start signal sent to TM1637 from MCU
  WriteByte(ADDR_FIXED);//
  Stop();           //
  Start();          //
  WriteByte(BitAddr|0xc0);//
  //Stop();
  //Start();
  WriteByte(SegData);//
  Stop();            //
  Start();          //
  WriteByte(Cmd_DispCtrl);//
  Stop();           //
  return 0;

}

static int ClearDisplay() {
  // Display --:--
  #if 1
  DisplayDataByAddr(0x00,0);
  DisplayDataByAddr(0x01,8);
  DisplayDataByAddr(0x02,2);
  DisplayDataByAddr(0x03,9);
  #else

  // Test Start
  Start();
  //usleep(200000);
  WriteByte(0x88);
  Stop();

  #endif
}






int InitLed() {
  // TODO: If Not Have File
  char cmd[128] = "";
  sprintf(cmd, "echo %d > /sys/class/gpio/export", LED_DIO);
  system(cmd);

  ClearDisplay();
  return 0;
}

int SendTime(const TimeData_T* const _time) {

}

int DeInitLed(const TimeData_T* const _time) {
  char cmd[128] = "";
  sprintf(cmd, "echo %d > /sys/class/gpio/unexport", LED_DIO);
  system(cmd);

  ClearDisplay();

}


