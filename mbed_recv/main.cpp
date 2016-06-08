#include "mbed.h"
#include "C12832.h"
#include "SDFileSystem.h"
#include "PowerControl.h"
#include "EthernetPowerControl.h"

/*CAN Id ordered by priority - 0=Highest priority*/
#define ALARM_ID 0
#define POT_ID 10
#define ACC_ID 11
#define SONAR_ID 12
#define BMS_ID 13
#define TEMP_ID 14
#define CMD_ID 15


// CAN Config
CAN can(p30, p29);//p30, p29

//LCD configs
//C12832 lcd(p5, p7, p6, p8, p11);

// File System Config
SDFileSystem sd(p5, p6, p7, p8, "SD");
//SparkFun MicroSD Breakout Board
/*MicroSD Breakout    mbed
    GND o-------------o GND
    3.3Vo-------------o VOUT
    5V o-------------o NC
    CS  o-------------o 8    (DigitalOut cs)
    mosi  o-------------o 5    (SPI mosi)
    SCK o-------------o 7    (SPI sclk)
    miso  o-------------o 6    (SPI miso)
    GND o-------------o GND*/

// LED Config
DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);
DigitalOut led4(LED4);

// RGB LED
PwmOut r (p23);
PwmOut g (p24);
PwmOut b (p25);

// Ticker Config
Ticker can_tick;
Ticker sdcard;
Ticker blinky;

uint64_t start;
float temp;
float pot;
float acc[3];

typedef union can_union {
    float fl[2];
    char bytes[8];
} data2send;

void save_sdcard(CANMessage msg){
    data2send data;
    data.bytes[0] = msg.data[0];
    data.bytes[1] = msg.data[1];
    data.bytes[2] = msg.data[2];
    data.bytes[3] = msg.data[3];
    data.bytes[4] = msg.data[4];
    data.bytes[5] = msg.data[5];
    data.bytes[6] = msg.data[6];
    data.bytes[7] = msg.data[7];
    
    mkdir("/SD/mydir", 0777);

    FILE *fp = fopen("/SD/mydir/test.txt", "a");
    if(fp == NULL) led4=1;
    if(msg.id==ALARM_ID)
        fprintf(fp, "%ld\t id= %d\t %u\n", us_ticker_read(),msg.id,data.bytes[0]);
    else if(msg.id==BMS_ID){
        //TODO:deve vir formatado devidamente do no que envia, ou seja, s começar o float no byte 4 ou no 0
        data.bytes[4] = msg.data[3];
        data.bytes[5] = msg.data[4];
        data.bytes[6] = msg.data[5];
        data.bytes[7] = msg.data[6];
        fprintf(fp,"%ld\t id= %d\t %u %u %u %.2f\n",us_ticker_read(),msg.id,data.bytes[0],data.bytes[1],data.bytes[2],data.fl[1]);
    }else if(msg.id==POT_ID || msg.id==TEMP_ID) {
        fprintf(fp,"%ld\t id= %d\t data=%.2f\n",us_ticker_read(),msg.id,data.fl[0]);
        if(msg.id==POT_ID) {
            r = 1-data.fl[0]/10;
            g = data.fl[0]/10;
        }
    }
    else if(msg.id==ACC_ID)
        fprintf(fp, "%ld\t id= %d:\t accX=%d; accY=%d; accZ=%d\n",us_ticker_read(),msg.id,(signed char)data.bytes[0], (signed char)data.bytes[1], (signed char)data.bytes[2]);
    else if(msg.id==CMD_ID)
        fprintf(fp, "%ld\t id= %d:\t %u\n",us_ticker_read(),msg.id,data.bytes[0]);
    else 
        fprintf(fp,"%ld\t id= %d\t %u %u %u %u %u %u %u %u\n",
                    us_ticker_read(), msg.id, 
                    data.bytes[0], data.bytes[1], data.bytes[2], data.fl[3], data.bytes[4], data.bytes[5], data.bytes[6],  data.fl[7]);
    
    fclose(fp);

}

void blinky_led(void)
{
    led1=!led1;
}

void can_recv(void)
{
    CANMessage msg;
    //data2send data;
    if(can.read(msg)) {
        led2 = !led2;
        save_sdcard(msg);
    }
}


int main(void)
{
    start = us_ticker_read();
    //can.filter(1337, 0xffff, CANStandard);

    can.frequency(1000000);

   blinky.attach(blinky_led, 1);
    
    // RGB LED = 1->off 0->on
    r=1; g=0; b=1;

    PHY_PowerDown();//turn off ethernet to save power
    
    FILE *fp = fopen("/SD/mydir/test.txt", "w");
    if(fp==NULL) led3=1;
    fclose(fp);
    
    // check if received CAN message
    can.attach(can_recv, CAN::RxIrq);
    
    while(1) {
        Sleep();
    }

}