//по мотивам https://raw.githubusercontent.com/matandoocorpo/Zwift-Steer/main/steerer.ino
#include <BLEDevice.h>
#include <BLE2902.h>
#include "SSD1306Wire.h"

#define DEBUG
#define Y_PIN 12 // Joystick Yaxis to GPIO12
#define B_PIN 13 // Joystick button to GPIO13
#define X_PIN 14 // Joystick Xaxis to GPIO14
#define ZERO_BOUNCE 40 // дребезг около начального значения
#define MAX_ADC_RESOLUTION 4096 //ESP32 ADC is 12bit
#define MAX_STEER_ANGLE 35.0f
//BLE Definitions
#define STEERING_DEVICE_UUID "347b0001-7635-408b-8918-8ff3949ce592"
#define STEERING_ANGLE_CHAR_UUID "347b0030-7635-408b-8918-8ff3949ce592" //notify
//#define STEERING_RX_CHAR_UUID "347b0031-7635-408b-8918-8ff3949ce592"  //write
#define STEERING_TX_CHAR_UUID "347b0032-7635-408b-8918-8ff3949ce592"    //indicate

int adcNullX = 0, adcNullY; //Joystick Calibration
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool auth = false;

uint8_t authSuccess[3] = {0xff, 0x13, 0xff};

BLEServer *pServer = NULL;
BLECharacteristic *pAngle = NULL;
BLECharacteristic *pTx = NULL;
BLEAdvertising *pAdvertising;

SSD1306Wire display(0x3c, 5, 4);

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    BLEDevice::startAdvertising();
#ifdef DEBUG
    Serial.println("onConnect");
#endif
  };
  void onDisconnect(BLEServer *pServer) { 
    deviceConnected = false; 
    auth = false;
#ifdef DEBUG
    Serial.println("onDisconnect");
#endif
  }
};
float readAngle(int pin, int adcNull) { //Joystick read angle from axis
  int potVal = analogRead(pin) - adcNull;
#ifdef DEBUG
   Serial.print(potVal); Serial.print(' '); Serial.println(pin);
#endif
  float angle = 0.0;
  if (potVal > ZERO_BOUNCE || potVal < -ZERO_BOUNCE) {
    if(potVal < 0)
      angle = float(potVal) / adcNull * MAX_STEER_ANGLE;
    else
      angle = float(potVal) / (MAX_ADC_RESOLUTION - adcNull - 1) * MAX_STEER_ANGLE;
  }
  return angle;
}
void initBle() {
  BLEDevice::init("ESP Steer");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
#ifdef DEBUG
  Serial.println("Define service...");
#endif
  BLEService *pService = pServer->createService(STEERING_DEVICE_UUID);
#ifdef DEBUG
  Serial.println("Define characteristics");
#endif
  pTx = pService->createCharacteristic(STEERING_TX_CHAR_UUID, BLECharacteristic::PROPERTY_INDICATE | BLECharacteristic::PROPERTY_READ);
  pTx->addDescriptor(new BLE2902());
  pAngle = pService->createCharacteristic(STEERING_ANGLE_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pAngle->addDescriptor(new BLE2902());
#ifdef DEBUG
  Serial.println("Staring BLE service...");
#endif
  pService->start();
#ifdef DEBUG
  Serial.println("Define the advertiser...");
#endif
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(true);
  pAdvertising->addServiceUUID(STEERING_DEVICE_UUID);
  pAdvertising->setMinPreferred(0x06); // set value to 0x00 to not advertise this parameter
#ifdef DEBUG
  Serial.println("Starting advertiser...");
#endif
  BLEDevice::startAdvertising();
#ifdef DEBUG
  Serial.println("Waiting a client connection to notify...");
#endif
}
void setup() {
  Serial.begin(115200);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_24);
  display.clear();
  display.drawString(0, 0, "Starting...");
  display.display();
  adcNullX = analogRead(X_PIN);
  if (adcNullX <= 0 || adcNullX == MAX_ADC_RESOLUTION)
      adcNullX = MAX_ADC_RESOLUTION / 2;
  adcNullY = analogRead(Y_PIN);
  if (adcNullY <= 0 || adcNullY == MAX_ADC_RESOLUTION)
      adcNullY = MAX_ADC_RESOLUTION / 2;
  initBle();
  display.clear();
  display.drawString(0, 0, "Waiting...");
  display.display();
}
int buttonPressCount = 0;
void loop() {
  if (deviceConnected) {
    if(auth) {
      float angle = 0.0;
      float angleX = int(readAngle(X_PIN, adcNullX));
      float angleY = readAngle(Y_PIN, adcNullY);
      if(angleX > 0) {
        angle = angleX + angleY / 100.0;
      } else if(angleX < 0 || angleY != 0){
        angle = angleX + angleY / 100.0;
      } else if(analogRead(B_PIN) == 0) {
        delay(10);
        if(++buttonPressCount < 5) return; //anti-bounce
        buttonPressCount = 0;
        angle = 100.0; //button pressed
      } else {
        delay(25);
      }
#ifdef DEBUG
      Serial.println(angle);
#endif
      pAngle->setValue(angle);
      pAngle->notify();
      delay(250);
      display.clear();
      display.drawString(0, 0, String(angle));
      display.display();
    } else {
#ifdef DEBUG
      Serial.println("Auth Challenging");
#endif
      pTx->setValue(authSuccess, 3);
      pTx->indicate();
      auth = true;
      delay(250);
    }
  }
  if (!deviceConnected && oldDeviceConnected) { //Advertising
    delay(300);                  // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
#ifdef DEBUG
    Serial.println("Nothing connected, start advertising");
#endif
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) { //Connecting
    oldDeviceConnected = deviceConnected;
#ifdef DEBUG
    Serial.println("Connecting...");
#endif
  }
}
