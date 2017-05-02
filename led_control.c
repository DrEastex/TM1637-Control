#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

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

#define GET_GPIO_VALUE_CMD "cat /sys/class/gpio/gpio4/value"

#define MSG(args...) printf(args) 

#define USE_SYS_GPIO 0

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

static int Flash_flag = 0;

static int gpio_export(int pin)  
{  
    char buffer[64];  
    int len;  
    int fd;  
  
    fd = open("/sys/class/gpio/export", O_WRONLY);  
    if (fd < 0) {  
        MSG("Failed to open export for writing!\n");  
        return(-1);  
    }  
  
    len = snprintf(buffer, sizeof(buffer), "%d", pin);  
    if (write(fd, buffer, len) < 0) {  
        MSG("Failed to export gpio!");  
        return -1;  
    }  
     
    close(fd);  
    return 0;  
}  

static int gpio_unexport(int pin)  
{  
    char buffer[64];  
    int len;  
    int fd;  
  
    fd = open("/sys/class/gpio/unexport", O_WRONLY);  
    if (fd < 0) {  
        MSG("Failed to open unexport for writing!\n");  
        return -1;  
    }  
  
    len = snprintf(buffer, sizeof(buffer), "%d", pin);  
    if (write(fd, buffer, len) < 0) {  
        MSG("Failed to unexport gpio!");  
        return -1;  
    }  
     
    close(fd);  
    return 0;  
} 

//dir: 0-->IN, 1-->OUT
static int gpio_direction(int pin, int dir)  
{  
    static const char dir_str[] = "in\0out";  
    char path[64];  
    int fd;  
  
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);  
    fd = open(path, O_WRONLY);  
    if (fd < 0) {  
        MSG("Failed to open gpio direction for writing!\n");  
        return -1;  
    }  
  
    if (write(fd, &dir_str[dir == 0 ? 0 : 3], dir == 0 ? 2 : 3) < 0) {  
        MSG("Failed to set direction!\n");  
        return -1;  
    }  
  
    close(fd);  
    return 0;  
}  

//value: 0-->LOW, 1-->HIGH
static int gpio_write(int pin, int value)  
{  
    static const char values_str[] = "01";  
    char path[64];  
    int fd;  
  
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);  
    fd = open(path, O_WRONLY);  
    if (fd < 0) {  
        MSG("Failed to open gpio value for writing!\n");  
        return -1;  
    }  
  
    if (write(fd, &values_str[value == 0 ? 0 : 1], 1) < 0) {  
        MSG("Failed to write value!\n");  
        return -1;  
    }  
  
    close(fd);  
    return 0;  
}

static int gpio_read(int pin)  
{  
    char path[64];  
    char value_str[3];  
    int fd;  
  
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);  
    fd = open(path, O_RDONLY);  
    if (fd < 0) {  
        MSG("Failed to open gpio value for reading!\n");  
        return -1;  
    }  
  
    if (read(fd, value_str, 3) < 0) {  
        MSG("Failed to read value!\n");  
        return -1;  
    }  
  
    close(fd);  
    return (atoi(value_str));
}  


static int SetPinMode(const int pin, int mode) {
#if USE_SYS_GPIO
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
#else

  return gpio_direction(pin, mode);

#endif
}

static int ExecuteCmd(const char* cmd) {

  FILE* fp = popen(cmd, "r");
  char buf[128] = "";
  fgets(buf, sizeof(buf), fp);
  buf[1] = 0;
  //printf("'%s'\r\n", buf);
  pclose(fp);
  int ret = atoi(buf);
  //printf("%d\r\n", ret);
  return 0;
}   

static int ReadPinData(const int pin) {
#if USE_SYS_GPIO
  return ExecuteCmd(GET_GPIO_VALUE_CMD); 
#else
  return gpio_read((int)pin);
#endif
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
#if USE_SYS_GPIO
  char cmd[128] = "";
  sprintf(cmd, "echo %d >> /sys/class/gpio/gpio%d/value", data, port);
  //printf("cmd:'%s'\r\n", cmd);
  system(cmd);
  return 0;
#else
  return gpio_write(port, data);

#endif
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
  if(Flash_flag)PointData = 0x80;
  else PointData = 0; 
  for(uint8_t i = 0;i < 4;i ++)
  {
    if(DispData[i] == 0x7f)DispData[i] = 0x00;
    else DispData[i] = TubeTab[DispData[i]] + PointData;
  }
}

static int FlashDot() {
  Flash_flag = !Flash_flag;
}

static int CodeSingleData(int8_t DispData) {
  uint8_t PointData;
  if(Flash_flag)PointData = 0x80;
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
  Stop();           //  
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

static int DisplayDataByMultiAddr(const uint8_t addr[], const int8_t data[], unsigned int length) {

  int8_t SegData;
  Start();          //start signal sent to TM1637 from MCU
  WriteByte(ADDR_FIXED);//
  Stop();           //
  Start();          //
  int i = 0;
  for ( i = 0; i < length; ++i ) {
    Start();
    SegData = CodeSingleData(data[i]);
    WriteByte(addr[i] | 0xc0);
    WriteByte(SegData);
    Stop();            // 
  }
  Start();       //
  WriteByte(Cmd_DispCtrl);//
  Stop();           //
  return 0;

  
  return 0;
}


static int DisplayOrgDataByAddr(uint8_t addr, uint8_t data) {
  int8_t SegData;
  SegData = data;
  Start();          //start signal sent to TM1637 from MCU
  WriteByte(ADDR_FIXED);//
  Stop();           //
  Start();          //
  WriteByte(addr|0xc0);//
  //Stop();
  //Start();
  WriteByte(SegData);//
  Stop();            //
  Start();          //
  WriteByte(Cmd_DispCtrl);//
  Stop();           //
  return 0;
}

int FlashOpts(const int min) {
  FlashDot();
  uint8_t data = CodeSingleData(min%10);
  printf("Data=%02X\r\n", data);
  DisplayOrgDataByAddr(0x03, data);
  return 0;
}


static int ClearDisplay() {
  // Display --:--
  #if 0
  DisplayDataByAddr(0x00,0);
  DisplayDataByAddr(0x01,8);
  DisplayDataByAddr(0x02,2);
  DisplayDataByAddr(0x03,9);
  #else



  #endif
}






int InitLed() {
#if  USE_SYS_GPIO
  // TODO: If Not Have File
  char cmd[128] = "";
  sprintf(cmd, "echo %d > /sys/class/gpio/export", LED_DIO);
  system(cmd);
#else
  return gpio_export(LED_DIO);
#endif
  ClearDisplay();
  return 0;
}

int SendTime(const TimeData_T* const _time) {
  //printf("%d%d\r\n", _time->hour, _time->min);
  uint8_t addr[]= {0x00, 0x01, 0x02, 0x03};
  int8_t data[4] = "";
  data[0] = (_time->hour)/10;
  data[1] = (_time->hour%10);
  data[2] = (_time->min/10);
  data[3] = (_time->min%10);
  FlashDot();
  DisplayDataByMultiAddr(addr, data, 4);
  //DisplayDataByAutoAddr(data);
  return 0;
}

int DeInitLed(const TimeData_T* const _time) {
#if USE_SYS_GPIO
  char cmd[128] = "";
  sprintf(cmd, "echo %d > /sys/class/gpio/unexport", LED_DIO);
  system(cmd);
#else 
  gpio_unexport(LED_DIO);
#endif
  ClearDisplay();

 }


