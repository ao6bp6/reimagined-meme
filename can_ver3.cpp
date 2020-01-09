/* Final Project for ESLAB, 2019 Fall , NTUEE
   Version III 
   Author & Development
   2020.1.09
   New Feature : PWMout Blue&Green, setTxPower, Feedback bool->Int ,Event_queue 10->32
 */
#include <events/mbed_events.h>

#include <mbed.h>
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"
#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ButtonService.h"
#include "pretty_printer.h"
#include "VL53L0X.h"

/* setup VL53L0X */
static DevI2C devI2c(PB_11,PB_10);
static DigitalOut shutdown_pin(PC_6_ALT0);
VL53L0X Dist(&devI2c, &shutdown_pin, PC_7);

const static char DEVICE_NAME[] = "IOTrash_Can";

static EventQueue event_queue(32 * EVENTS_EVENT_SIZE);


class CanService {
public:
    const static uint16_t CAN_SERVICE_UUID              = 0xA002;
    const static uint16_t HUMID_CHARACTERISTIC_UUID         = 0xA003;
    const static uint16_t TEMP_CHARACTERISTIC_UUID      = 0xA004;
    const static uint16_t GAS_CHARACTERISTIC_UUID = 0xA005;
    const static uint16_t FEEDBACK_CHARACTERISTIC_UUID = 0xA006 ; 
    const static uint16_t PROX_CHARACTERISTIC_UUID = 0xA008;

    CanService(BLE &_ble, float humidInitial, float tempInitial, float gasInitial , int feedbackInitial, float proxInitial) :
        ble(_ble),
        humid (HUMID_CHARACTERISTIC_UUID, &humidInitial),
        temp  (TEMP_CHARACTERISTIC_UUID, &tempInitial),
        gas (GAS_CHARACTERISTIC_UUID , &gasInitial),
        feedback (FEEDBACK_CHARACTERISTIC_UUID, &feedbackInitial),
        prox (PROX_CHARACTERISTIC_UUID, &proxInitial)

    {
        GattCharacteristic *Table[] = {&humid, &temp, &gas ,&feedback, &prox};
        GattService        canService(CanService::CAN_SERVICE_UUID, Table, sizeof(Table) / sizeof(GattCharacteristic *));
        ble.gattServer().addService(canService);
    }

    void updateHumid(float newData){
        ble.gattServer().write(humid.getValueHandle(),(uint8_t *)&newData,sizeof(float));
    }
    void updateTemp(float newData){
        ble.gattServer().write(temp.getValueHandle(),(uint8_t *)&newData,sizeof(float));
    }
    void updateGas(float newData){
        ble.gattServer().write(gas.getValueHandle(),(uint8_t *)&newData,sizeof(float));
    }
    
    void InitialFeedback(bool newData){
        ble.gattServer().write(feedback.getValueHandle(),(uint8_t *)&newData,sizeof(int));
        printf("Maunually 0");
    }

    void updateProx(float newData){
        ble.gattServer().write(prox.getValueHandle(),(uint8_t *)&newData,sizeof(float));
    }
 
private:
    BLE                                 &ble;
    ReadOnlyGattCharacteristic<float>   humid ;
    ReadOnlyGattCharacteristic<float>   temp ;
    ReadOnlyGattCharacteristic<float>   gas ;
    ReadOnlyGattCharacteristic<float>   prox;
                                    //  Humidity.T.Gas,Proximity
    ReadWriteGattCharacteristic<int> feedback;

};

class BatteryDemo : ble::Gap::EventHandler {
public:
    BatteryDemo(BLE &ble, events::EventQueue &event_queue) :
        _ble(ble),
        _event_queue(event_queue),
        _led1(LED1, 1),
        _led4(LED4, 0),
        _red(PB_8),
        _green(PB_9),
        _blue(PA_6),
        _gas(PA_1),
        _button(BLE_BUTTON_PIN_NAME, BLE_BUTTON_PIN_PULL),
        _button_service(NULL),
        _can_service(NULL),
        _button_uuid(ButtonService::BUTTON_SERVICE_UUID),
        _TOF_Distance(NULL),
        _adv_data_builder(_adv_buffer) { }

    void start() {
        _ble.gap().setEventHandler(this);

        _ble.init(this, &BatteryDemo::on_init_complete);

        _event_queue.call_every(500, this, &BatteryDemo::blink);
        _event_queue.call_every(30000,Callback<void()>(this, &BatteryDemo::readht));

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
        Dist.init_sensor(VL53L0X_DEFAULT_ADDRESS);

        /* Setup primary service. */

        _button_service = new ButtonService(_ble, false /* initial value for button pressed */);
        _can_service = new CanService(_ble , 0 , 0 , 0 , 0 , 0 ); 
                                    // ble, H, T, Gas, Feedback, Proximity

        // Button event handler
        _button.fall(Callback<void()>(this, &BatteryDemo::button_pressed));
        _button.rise(Callback<void()>(this, &BatteryDemo::button_released));
            
        // Advertising Periodic 
        start_advertising();

        _ble.gattServer().onDataWritten(as_cb(&BatteryDemo::onDataWritten));
    }


    void start_advertising() {
        
        // Initailize LED to avoid asynchronous effect

        //init_feedback();
        //WashInit();

        /* Create advertising parameters and payload */

        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(1000)),
            ble::adv_interval_t(ble::millisecond_t(3000))
        );

        // UpdateSensorValue
        Dist.get_distance(&_TOF_Distance);
        UpdateSensorValue();

        _adv_data_builder.setFlags();
        _adv_data_builder.setLocalServiceList(mbed::make_Span(&_button_uuid, 1));
        _adv_data_builder.setName(DEVICE_NAME);
        _adv_data_builder.setTxPowerAdvertised(70);
        Gap::PhySet_t tx( 0 , 0 ,1);
        Gap::PhySet_t rx( 0 , 0 ,1);
        _ble.gap().setPreferredPhys( &tx, &rx); 
        //_ble.gap().setPhy( ,ble::phy_t::LE_CODED, ble::phy_t::LE_CODED, ble::coded_symbol_per_bit_t::S2);

        
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
        _event_queue.call(Callback<void()>(this, &BatteryDemo::alert_zero));
        _event_queue.call(Callback<void()>(this,&BatteryDemo::start_advertising));
    }

    void button_released(void) {
        _event_queue.call(Callback<void(bool)>(_button_service, &ButtonService::updateButtonState), false);
    }
    void UpdateSensorValue(void){
        _can_service->updateTemp(BSP_TSENSOR_ReadTemp());
        _can_service->updateHumid(BSP_HSENSOR_ReadHumidity());
        _can_service->updateGas(_gas.read());
        _can_service->updateProx((float)_TOF_Distance);
    }
    void blink(void) {       
        _led1 = !_led1;
    }

    void alert(void){        //     
        _led4 = !_led4;
    }

    void WashAlert(void){
         _blue.period(2.0f);  // 2 second period
         _blue.pulsewidth(2); 
    }

    void WashInit(void){
         _blue.period(2.0f);  // 2 second period
         _blue.write(0); 
    }
    void BLE_ConnectionAlert(void){
         _green.period(2.0f);  // 2 second period
         _green.pulsewidth(2); 
    }

    void BLE_DisconnAlert(void){
         _green.period(2.0f);  // 2 second period
         _green.write(0); 
    }

    void alert_zero(void){   // For Query LED Indication
        _led4 = 1 ;
    }

    void alert_feedback(){
        _red.period(2.0f);  // 2 second period
        _red.pulsewidth(1); 
    }

    void init_feedback(){
        _red.period(2.0f);  // 2 second period
        _red.pulsewidth(0); 
        //_red.write(0);
    }

    void readht(void){
    time_t sec = time(NULL);
    Dist.get_distance(&_TOF_Distance);
    printf ("H = %f\n", BSP_HSENSOR_ReadHumidity());
    printf ("T = %f\n", BSP_TSENSOR_ReadTemp());
    printf("MQ-3 value= 0x%4x, concentration= %3.3f mg/L\n",_gas.read_u16(), _gas.read());
    printf ("Proximity = %f mm\n", (float)_TOF_Distance);
    printf("%s", ctime(&sec));
    printf ("\n");
}   

private:
    /* Event handler */

    virtual void onDisconnectionComplete(const ble::DisconnectionCompleteEvent&) {
        _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
        _event_queue.call(printf,"Disconnection OK!\n");
        _event_queue.call(Callback<void()>(this, &BatteryDemo::BLE_DisconnAlert));
        _event_queue.call_in(4000,Callback<void()>(this, &BatteryDemo::init_feedback));
        _event_queue.call(Callback<void()>(this, &BatteryDemo::WashInit));

        //_can_service -> InitialFeedback(0);
        //ble_error_t error = _ble.shutdown(); 
    }
    virtual void onConnectionComplete(const ble::ConnectionCompleteEvent&){
        _event_queue.call(printf,"Connection OK!\n");
        _event_queue.call(Callback<void()>(this, &BatteryDemo::BLE_ConnectionAlert));
        _event_queue.call_every(1000, this, &BatteryDemo::UpdateSensorValue);
        _event_queue.call(Callback<void()>(this, &BatteryDemo::init_feedback));
    }

    void onDataWritten(const GattWriteCallbackParams *context){
        // 01 BLE Connection
        // 00 pass 
        // 02 Wash Service
        if (*(context -> data) == 0x01){
        _event_queue.call(printf,"STM32 has been written 1!\n");
        _event_queue.call(Callback<void()>(this, &BatteryDemo::alert_feedback));
    }
        else if (*(context -> data) == 0x00){
        _event_queue.call(printf,"STM32 has been written 0 !\n");
        _event_queue.call_in(2000,Callback<void()>(this, &BatteryDemo::init_feedback));
    }
        else if (*(context -> data) == 0x02){
        _event_queue.call(Callback<void()>(this, &BatteryDemo::WashAlert));
    }
    }

    template<typename Arg>
    FunctionPointerWithContext<Arg> as_cb(void (BatteryDemo::*member)(Arg))
    {
        return makeFunctionPointer(this, member);
    }

private:
    BLE &_ble;
    events::EventQueue &_event_queue;

    DigitalOut  _led1;
    DigitalOut  _led4;      // Use for Typical Alert
    PwmOut _red;            // Use for Query or Alert
    PwmOut _green;          // Use for BLE Connection Alert
    PwmOut _blue;           // Use for Wash Button
    AnalogIn _gas;          // Use for MQ-3 Gas Detector
    InterruptIn _button;
    ButtonService *_button_service;
    CanService *_can_service;

    UUID _button_uuid;

    uint32_t _TOF_Distance;
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