/* Final Project for ESLAB, 2019 Fall , NTUEE
   Version I 
   Author & Development
   2019.12.28
 */
#include <events/mbed_events.h>

#include <mbed.h>
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ButtonService.h"
#include "pretty_printer.h"

const static char DEVICE_NAME[] = "IOTrash_Can";

static EventQueue event_queue(10 * EVENTS_EVENT_SIZE);


class CanService {
public:
    const static uint16_t CAN_SERVICE_UUID              = 0xA002;
    const static uint16_t HUMID_CHARACTERISTIC_UUID         = 0xA003;
    const static uint16_t TEMP_CHARACTERISTIC_UUID      = 0xA004;

    CanService(BLE &_ble, float humidInitial, float tempInitial) :
        ble(_ble),
        humid (HUMID_CHARACTERISTIC_UUID, &humidInitial),
        temp  (TEMP_CHARACTERISTIC_UUID, &tempInitial),


    {
        GattCharacteristic *Table[] = {&humid, &temp};
        GattService        canService(CanService::CAN_SERVICE_UUID, Table, sizeof(Table) / sizeof(GattCharacteristic *));
        ble.gattServer().addService(canService);
    }

    void updateHumid(float newData){
        ble.gattServer().write(humid.getValueHandle(),(uint8_t *)&newData,sizeof(float));
    }

    void updateTemp(float newData){
        ble.gattServer().write(temp.getValueHandle(),(uint8_t *)&newData,sizeof(float));
    }
 
private:
    BLE                                 &ble;
    ReadOnlyGattCharacteristic<float>   humid ;
    ReadOnlyGattCharacteristic<float>   temp ;
                                    //  Humidity.T.Gas

};

class BatteryDemo : ble::Gap::EventHandler {
public:
    BatteryDemo(BLE &ble, events::EventQueue &event_queue) :
        _ble(ble),
        _event_queue(event_queue),
        _led1(LED1, 1),
        _led4(LED4, 0),
        _red(PB_8),
        _button(BLE_BUTTON_PIN_NAME, BLE_BUTTON_PIN_PULL),
        _button_service(NULL),
        _can_service(NULL),
        _button_uuid(ButtonService::BUTTON_SERVICE_UUID),
        _adv_data_builder(_adv_buffer) { }

    void start() {
        _ble.gap().setEventHandler(this);

        _ble.init(this, &BatteryDemo::on_init_complete);

        _event_queue.call_every(500, this, &BatteryDemo::blink);

        _event_queue.dispatch_forever();
    }

private:
    /** Callback triggered when the ble initialization process has finished */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params) {
        if (params->error != BLE_ERROR_NONE) {
            printf("Ble initialization failed.");
            return;
        }

        BSP_TSENSOR_Init();
        BSP_HSENSOR_Init();
        print_mac_address();

        /* Setup primary service. */

        _button_service = new ButtonService(_ble, false /* initial value for button pressed */);
        _can_service = new CanService(_ble , 0 , 0); 

        // Button event handler
        _button.fall(Callback<void()>(this, &BatteryDemo::button_pressed));
        _button.rise(Callback<void()>(this, &BatteryDemo::button_released));
        
        // update Sensor Value
        _can_service->updateTemp(BSP_TSENSOR_ReadTemp());
        _can_service->updateHumid(BSP_HSENSOR_ReadHumidity());

        // Advertising Periodic 
        start_advertising();
    }


    void start_advertising() {
        /* Create advertising parameters and payload */

        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(1000))
            //,ble::adv_interval_t(ble::millisecond_t(50000))
        );

        _adv_data_builder.setFlags();
        _adv_data_builder.setLocalServiceList(mbed::make_Span(&_button_uuid, 1));
        _adv_data_builder.setName(DEVICE_NAME);

        /* Setup advertising */

        ble_error_t error = _ble.gap().setAdvertisingParameters(
            ble::LEGACY_ADVERTISING_HANDLE,
            adv_parameters
        );

        if (error) {
            print_error(error, "_ble.gap().setAdvertisingParameters() failed");
            return;
        }

        error = _ble.gap().setAdvertisingPayload(
            ble::LEGACY_ADVERTISING_HANDLE,
            _adv_data_builder.getAdvertisingData()
        );

        if (error) {
            print_error(error, "_ble.gap().setAdvertisingPayload() failed");
            return;
        }

        /* Start advertising */

        error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

        if (error) {
            print_error(error, "_ble.gap().startAdvertising() failed");
            return;
        }
    }

    void button_pressed(void) {
        _event_queue.call(Callback<void(bool)>(_button_service, &ButtonService::updateButtonState), true);
        //start_advertising();
    }

    void button_released(void) {
        _event_queue.call(Callback<void(bool)>(_button_service, &ButtonService::updateButtonState), false);
    }

    void blink(void) {
        _led1 = !_led1;
    }

private:
    /* Event handler */

    virtual void onDisconnectionComplete(const ble::DisconnectionCompleteEvent&) {
        _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
    }

private:
    BLE &_ble;
    events::EventQueue &_event_queue;

    DigitalOut  _led1;
    DigitalOut  _led4;      // Use for Typical Alert
    PwmOut _red;            // Use for Query or Alert
    InterruptIn _button;
    ButtonService *_button_service;
    CanService *_can_service;

    UUID _button_uuid;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;
};

/** Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

int main()
{
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);

    BatteryDemo demo(ble, event_queue);
    demo.start();

    return 0;
}

