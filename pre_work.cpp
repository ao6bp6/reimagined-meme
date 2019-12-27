#include "mbed.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "ble/BLE.h"
#include "VL53L0X.h"

InterruptIn button(BUTTON1);
//DigitalOut led4(LED4);
AnalogIn gas(PA_1);
DigitalOut led2(LED2);
DigitalOut led1(LED1);
PwmOut Green_alert(PB_9);
PwmOut Red_alert(PB_8);
EventQueue queue(32*EVENTS_EVENT_SIZE);
static DigitalOut shutdown_pin(PC_6);
static DevI2C devI2c(PB_11,PB_10);

void boot_shine(){
    led2 = !led2;
}

void reminder(){
    led1 = !led1 ;
}

void alert(int condition){
    if (condition == 1){
         Green_alert.period(15);
    }
    else if (condition == 2) {
        Red_alert.period(5);
    }
}

void readht(){
    time_t sec = time(NULL);
    printf ("H = %f\n", BSP_HSENSOR_ReadHumidity());
    printf ("T = %f\n", BSP_TSENSOR_ReadTemp());
    printf("MQ-3 value= 0x%4x, percentage= %3.3f%%\n\r",gas.read_u16(), gas.read()*100.0f);
    //printf ("Proximity = %d\n",sensor.readRangeSingleMillimeters());
    printf("%s", ctime(&sec));
    printf ("\n");
}

int main()
{
    BSP_HSENSOR_Init();
    BSP_TSENSOR_Init();
    queue.call(boot_shine);
    uint32_t distance;
    int i; 
    static VL53L0X range(&devI2c, &shutdown_pin, PC_7);
    int status = range.get_distance(&distance);
    printf("%d\n",status);
    if (status == VL53L0X_ERROR_NONE) {
        printf("VL53L0X [mm]:            %6d\r\n",distance);
    } else {
        printf("VL53L0X [mm]:                --\r\n");
    }
    button.rise(queue.event(alert,1));
    queue.call_every(2000,readht);
    queue.dispatch_forever();
    
}
