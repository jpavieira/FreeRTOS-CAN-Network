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

uint8_t aux_2[8];
int filtered_handler;
bool flag=1;

// Accelerometer
MMA7660 MMA(p28, p27);
DigitalOut connectionLed(LED1);

// Temperature sensor config
LM75B tmp(p28,p27);

//LCD configs
C12832 lcd(p5, p7, p6, p8, p11);

// Potenciometer Config
AnalogIn voltage1(p19);
float pot = 0;

AnalogIn temp1(p17);
AnalogIn temp2(p16);
AnalogIn temp3(p15);
AnalogIn sonar(p18);

// Queue
xQueueHandle MyQueue;

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
        //ATENÇÃO:Na leitura dos sensores apenas enviar os dados quando houver uma variação significativa, e.g. a temp variar 1ºC

        if(msg.type==FLOAT || msg.type==INT) {
            data.fl[0]=*((float *)msg.pointer);
            can.write(CANMessage(msg.id, &data.bytes[0], 5));
        } else if(msg.type==CHAR) {
            data.bytes[0]=*((char *)msg.pointer);
            can.write(CANMessage(msg.id, &data.bytes[0], 2));
        } else if(msg.type==BMS) {
            for(int i = 0; i< 8; i++) {
                data.bytes[i]=*((char *)msg.pointer+i);
            }
            can.write(CANMessage(msg.id, &data.bytes[0], 8));
        }
        if(!err) {
            led4=1;//error led
            xSemaphoreTake(xMutexLCD,portMAX_DELAY);
            lcd.locate(1,1);    // row 1, col 1
            lcd.printf("Message sent: ERROR");
            xSemaphoreGive(xMutexLCD);
        }
        led2 = !led2;
        vTaskDelay(100/portTICK_RATE_MS);
    }

}


void can_recv(void *pvParameters)
{
    CANMessage msg;
    for(;;) {
        can.read(msg);
        if(msg.id==CMD_ID) {
            flag = msg.data[0];
            led3 = flag;
        }

        vTaskDelay(50/portTICK_RATE_MS);
    }

}

void readsensors(void *pvParameters)
{
    queueData msg_bms;
    data2send data;
    float aux[4];
    char aux_1[4];
    while(1) {

        vTaskDelay(150/portTICK_RATE_MS);
        aux[0]=temp1.read()*330;//pin 17
        aux[1]=temp2.read()*330;//pin 16
        aux[2]=temp3.read()*330;//pin 15
        aux[3]=voltage1.read()*3.3;//pin 19

        if(flag) {
            data.fl[0]=aux[3];
            aux_2[3]=data.bytes[0];
            aux_2[4]=data.bytes[1];
            aux_2[5]=data.bytes[2];
            aux_2[6]=data.bytes[3];

            lcd.locate(1,18);    // row 18, col 1
            lcd.printf("temps:%.f %.f %.f. Volts: %.02f", aux[0],aux[1],aux[2],aux[3]);
            for(int i = 0; i<3; i++) {
                sprintf(aux_1,"%.f",aux[i]);
                aux_2[i]=(uint8_t)atoi(aux_1);
            }
            msg_bms.type=BMS;
            msg_bms.id=13;
            msg_bms.pointer=&aux_2;
            xQueueSend(MyQueue,&msg_bms,10);
        } else {
            vTaskDelay(1000/portTICK_RATE_MS);
            
            lcd.cls();
            lcd.locate(1,1);    // row 18, col 1
             if(aux[0] > 30) {
                msg_bms.type=CHAR;
                msg_bms.id=ALARM_ID;
                int sensor_number=0;
                msg_bms.pointer= &sensor_number;
                lcd.locate(1,1);    // row 18, col 1
                lcd.printf("T_1:%.f ", aux[0]);

                xQueueSend(MyQueue,&msg_bms,10);
            }

            if(aux[1] > 30) {
                msg_bms.type=CHAR;
                msg_bms.id=ALARM_ID;
                char sensor_number=1;
                msg_bms.pointer= &sensor_number;
                //lcd.locate(1,24);    // row 18, col 1
                lcd.printf("       T_2:%.f ", aux[1]);
                xQueueSend(MyQueue,&msg_bms,10);
            }

            if(aux[2] > 30) {
                msg_bms.type=CHAR;
                msg_bms.id=ALARM_ID;
                char sensor_number=2;
                msg_bms.pointer= &sensor_number;

                //lcd.locate(1,48);    // row 18, col 1
                lcd.printf("T_3:%.f ", aux[2]);
                xQueueSend(MyQueue,&msg_bms,10);
            }
            if(aux[3] < 2.7) {
                msg_bms.type=CHAR;
                msg_bms.id=ALARM_ID;
                char sensor_number=3;
                msg_bms.pointer= &sensor_number;

                //lcd.locate(1,72);    // row 18, col 1
                lcd.printf("V: %.02f ", aux[3]);
                xQueueSend(MyQueue,&msg_bms,10);
            }

        }
    }
    //xQueueSend(MyQueue,&msg,10);

}


int main(void)
{
    xTaskHandle blink_led1_task_handle = NULL;
    xTaskHandle can_send_handle = NULL;
    xTaskHandle can_recv_handle = NULL;
    xTaskHandle acc_recv_handle = NULL;
    xTaskHandle readPot_handle = NULL;
    xMutexLCD = xSemaphoreCreateMutex();
    xMutexI2C = xSemaphoreCreateMutex();
    MyQueue = xQueueCreate( 20, sizeof(queueData) );
    if(MyQueue == NULL) {
        led4=1;
    }

    can.frequency(1000000);

    int task_error;

    /* initialize hardware */
    init_hardware();

    // create task to heartbeat LED
    task_error = xTaskCreate(blink_led1_task, "heartbeat", 2*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, &blink_led1_task_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}

    // create task to send through CAN
    task_error = xTaskCreate(can_send, "CAN Send", 4*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+3, &can_send_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}

    //create task to receive through CAN
    task_error=xTaskCreate(can_recv, "CAN Recv", 2*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+5, &can_recv_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}

    //create task to write value of pot to usb
    task_error = xTaskCreate(readsensors, "Read sensors", 2*configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+2, &readPot_handle);
    if(task_error == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY) {led4=1;}

    /* Start the scheduler. */
    vTaskStartScheduler();

    /* should never reach here! */
    for(;;);
}