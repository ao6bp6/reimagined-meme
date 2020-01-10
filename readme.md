# IOTrash Can

A service that can notify users to drop garbage or wash trash can. 

## The project covers

* Sensor node(STM32 IOT01a) collects data of temperature, humidity, proximity and alcohol concentration.
* Communication between sensor node and central MCU(Raspberry 3). 
* Sensor node data processed in central MCU.

### Prerequisites

* Install bluepy for Raspberry.
* Install Mbed Studio for STM32.
* 3 PWMout LED , 1 reset button and 1 MQ-3 sensor.

## Running the programs

For Raspberry
```
python ble_test.py
```

For STM32
  Include required libraries in Mbed Studio and run the program.

## Built With

* [Mbed OS 5 BLE](https://os.mbed.com/docs/mbed-os/v5.15/tutorials/ble-tutorial.html) - The communication interface
* [Mbed BLE button services](https://os.mbed.com/teams/mbed-os-examples/code/mbed-os-example-ble-Button/) - The framework used

## Version

Version I II III for STM32. Ver.III is the final demo version.
Version I II for Raspberry. trashcan_bluepy.py is the final demo version.


## Authors

* **Shuen-Yu Tsai** 
* **Husan-Fung Huang**

Both are 2019 Fall students in ESLAB class instructed by Prof. Wang Shen-De. 

