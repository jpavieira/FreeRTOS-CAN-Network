#include "mbed.h"


#include "C12832.h"
#include "LPC17xx.h"
#include "AnalogIn.h"
#include "FreeRTOS.h"
#include "task.h"
#include "LM75B.h"
#include "semphr.h"
#include "MMA7660.h"


#define LED1_GPIO  18
#define LED2_GPIO  20
#define LED3_GPIO  21
#define LED4_GPIO  23

#define LED_GPIO   LED1_GPIO

#define TEMP_TH 1 //temperature threshold - value that temperature needs to vary to be sended via CAN

/*CAN Id ordered by priority - 0=Highest priority*/
#define ALARM_ID 0
#define POT_ID 10
#define ACC_ID 11
#define SONAR_ID 12
#define BMS_ID 13
#define TEMP_ID 14
#define CMD_ID 15

// LED Config
DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);
DigitalOut led4(LED4);

// CAN Config
CAN can(p9, p10);

char temp = 0;
typedef union can_union {
    float fl[2];
    char bytes[8];
} data2send;


// Joystick Config
BusIn Up(p15);
BusIn Down(p12);
//BusIn Left(p13);
//BusIn Right(p16);

// Accelerometer
MMA7660 MMA(p28, p27);
DigitalOut connectionLed(LED1);


bool flag = 0;
    
    
// Temperature sensor config
LM75B tmp(p28,p27);
float fTemp, fLastTemp=0;

//LCD configs
C12832 lcd(p5, p7, p6, p8, p11);

// Potenciometer Config
AnalogIn pot1(p19);
float pot = 0;


// Queue
xQueueHandle MyQueue;
float buffer_pot;

typedef enum {
    INT,
    FLOAT,
    CHAR,
    BMS
} DATA_TYPE;

typedef struct queueData {
    DATA_TYPE type;
    void* pointer;
    int id;
} queueData;

// Semaphores
xSemaphoreHandle xMutexLCD;
xSemaphoreHandle xMutexI2C;

void init_hardware()
{
    /* set system tick for 1ms interrupt */
    SystemCoreClockUpdate();
}

void blink_led1_task(void *pvParameters)
{
    while(1) {
        led1 = !led1;
        vTaskDelay(1000/portTICK_RATE_MS);
    }
}


void can_send(void *pvParameters)
{
    int err;
    data2send data;
    queueData msg;
    for(;;) {
        xQueueReceive(MyQueue,&msg,portMAX_DELAY);
       
        if(msg.type==FLOAT || msg.type==INT) {
            data.fl[0]=*((float *)msg.pointer);
            can.write(CANMessage(msg.id, &data.bytes[0], 5));
        } else if(msg.type==CHAR) {
            data.bytes[0]=*((char *)msg.pointer);
            can.write(CANMessage(msg.id, &data.bytes[0], 4));
        }
        
        led2 = !led2;
        
    }

}


void acc_read (void *pvParameters)
{
    float acc[3];
    queueData msg_acc;
    int aux[3];
    //if (MMA.testConnection())
    //    led3 = !led3;

    while(1) {
        vTaskDelay(250/portTICK_RATE_MS);
        xSemaphoreTake(xMutexI2C,portMAX_DELAY);
        MMA.readData(acc);
        xSemaphoreGive(xMutexI2C);

        aux[0]=acc[0];
        aux[1]=acc[1];
        aux[2]=acc[2];
        msg_acc.type=CHAR;
        msg_acc.id=ACC_ID;
        msg_acc.pointer=(void *)&aux[0];
        
        xQueueSend(MyQueue,&msg_acc,10);
        xSemaphoreTake(xMutexLCD,portMAX_DELAY);
            //lcd.locate(40,1);    // row 18, col 1
            lcd.printf("X-Y-Z: %2.2f %2.2f %2.2f", acc[0], acc[1], acc[2]);
        xSemaphoreGive(xMutexLCD);
    }

}

void readPot(void *pvParameters){
    queueData msg_pot;
    
    while(1) {
        vTaskDelay(150/portTICK_RATE_MS);
        msg_pot.type=FLOAT;
        msg_pot.id=POT_ID;
        buffer_pot=pot1.read();
        buffer_pot=buffer_pot*10;
        msg_pot.pointer=(void *)(&buffer_pot);
        xSemaphoreTake(xMutexLCD,portMAX_DELAY);
            //lcd.locate(25,0);    // row 18, col 1
            lcd.printf("pot=%.2f\n",*((float *)msg_pot.pointer));
        xSemaphoreGive(xMutexLCD);
        xQueueSend(MyQueue,&msg_pot,10);
        
    }
}



void readTemp(void *pvParameters){
    queueData msg_temp;
    while(1){
        vTaskDelay(500/portTICK_RATE_MS);
        
        xSemaphoreTake(xMutexI2C,portMAX_DELAY);
        fTemp=tmp.read();
        xSemaphoreGive(xMutexI2C);
        
        if (fLastTemp==0){
            fLastTemp=fTemp;
        }else if (abs(fLastTemp-fTemp)>=(float)TEMP_TH){  
            msg_temp.type=FLOAT;
            msg_temp.id=TEMP_ID;
            msg_temp.pointer=(void *)(&fTemp); 
            xSemaphoreTake(xMutexLCD,portMAX_DELAY);
                //lcd.locate(0,0);    // row 18, col 1
                lcd.printf("temp=%.2f\n",*((float *)msg_temp.pointer));
            xSemaphoreGive(xMutexLCD);           
            xQueueSend(MyQueue,&msg_temp,10);
        }
    }
}


void read_joystick(void *pvParameters){

   queueData msg_joy;
 
    while(1){
        vTaskDelay(200/portTICK_RATE_MS);
     
        if(Up){
            flag = 1;
            msg_joy.type=CHAR;
            msg_joy.id=CMD_ID;
            msg_joy.pointer=(char *)(&flag);
        }
        if(Down){
            flag = 0;
            msg_joy.type=CHAR;      
            msg_joy.id=CMD_ID;
            msg_joy.pointer=(char *)(&flag);
        } 
        xQueueSend(MyQueue,&msg_joy,10);
        led3 = flag;
    }
}


int main(void)
{
    xTaskHandle blink_led1_task_handle = NULL;
    xTaskHandle can_send_handle = NULL;
    xTaskHandle readTemp_handle = NULL;
    xTaskHandle acc_recv_handle = NULL;
    xTaskHandle readPot_handle = NULL;
    xTaskHandle check_joystick_handle = NULL;

    xMutexLCD = xSemaphoreCreateMutex();
    xMutexI2C = xSemaphoreCreateMutex();
    MyQueue = xQueueCreate( 20, sizeof(queueData) );
    //if(MyQueue == NULL) led4=1;//error code

    can.frequency(1000000);
    lcd.locate(0,0);
    int task_error;

    /* initialize hardware */
    init_hardware();

    // create task to heartbeat LED
    task_error = xTaskCreate(blink_led1_task, "heartbeat", 2*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, &blink_led1_task_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}

    // create task to send through CAN
    task_error = xTaskCreate(can_send, "CAN Send", 4*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+2, &can_send_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}

    //create task to read accelerometer
    task_error = xTaskCreate(acc_read, "ACC Read", 2*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+3, &acc_recv_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}
    
    // create task to read temperature
    task_error = xTaskCreate(readTemp, "Temp Read", 2*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+2, &check_joystick_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}
    
    // create task to read joystick
    task_error = xTaskCreate(read_joystick, "Joy Read", 2*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+2, &readTemp_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}
    
    
    // create task to write value of pot 
    task_error = xTaskCreate(readPot, "Read Pot", 2*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+4, &readPot_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}
    
    vPortSuppressTicksAndSleep( 25 );

    /* Start the scheduler. */
    vTaskStartScheduler();

    /* should never reach here! */
    for(;;);
}