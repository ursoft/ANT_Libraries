//по мотивам https://raw.githubusercontent.com/matandoocorpo/Zwift-Steer/main/steerer.ino
//Сборка в Platformio: не работает ADC, читает MAX (конфликт с BLE)
//https://randomnerdtutorials.com/esp32-built-in-oled-ssd1306/ - тут пины можно подсмотреть
//c симуляцией мембранной клавиатуры помогли на форуме http://arduino.ru/forum/apparatnye-voprosy/programmnaya-simulyatsiya-nazhatiya-knopki-membrannoi-klaviatury
//Резистор у светодиода оптопары: 5кОм
//Резистор у оптопары на черном проводе (без него штатные кнопки Schwinn не срабатывают - видимо, просаживается уровень): 10кОм
//Список пинов разъёма джойстика (начинается снизу разъёма):
// - 1. GND
// - 2. 5V (хватает трех)
// - 3. Х
// - 4. У
// - кнопка (0: нажата)
//Список пинов разъёма изделия (начинается с желтого, снизу)
// - 1. Принимающий импульс кнопки "вверх"
// - 2. Задающий импульс кнопки "вверх"
// - 3. Принимающий импульс кнопки "вниз"
// - 4. Земля
// - 5. Задающий импульс кнопки "вниз"
// - 6. 9 вольт
//#define DEBUG
#include "common.h"

struct CrankInfo {
  CrankInfo() : m_curSchwinnPowerIncomeTm(0), m_reportedResistance(0), m_lastDeviceTime(0) {
    m_cranks[0] = m_cranks[1] = m_cranks[2] = 0;
  }
  unsigned long m_curSchwinnPowerIncomeTm;
  uint8_t m_reportedResistance, m_cranks[3];
  uint16_t m_lastDeviceTime;
};

enum Chunks { Line1, Line2, Line3, Line2a, Line2b, Line3a, CHUNKS };
class Display {
  SSD1306Wire m_display;
  String m_lines[CHUNKS];
  const int m_x[CHUNKS] = {0,  0,  0, 25, 70, 80};
  const int m_y[CHUNKS] = {0, 21, 42, 21, 21, 42};
public:
  Display() : m_display(0x3c, 5, 4) {}
  void begin() {
    m_display.init();
    m_display.flipScreenVertically();
    m_display.setFont(ArialMT_Plain_24);
  }
  void update(const char *newLine[CHUNKS]) {
    bool ch = false;
    for(int i = 0; i < CHUNKS; i++) {
      if (newLine[i] && String(newLine[i]) != m_lines[i]) {
        ch = true;
        m_lines[i] = newLine[i];
      }
    }
    if (ch) {
      m_display.clear();
      for(int i = 0; i < CHUNKS; i++) {
        m_display.drawString(m_x[i], m_y[i], m_lines[i]);
      }
      m_display.display();
    }
  }
} display;

volatile uint8_t glbCurResist = 0;

class SchwinnResistButton { //resistance control - up and down
  const int m_outPin; // какой пин командует оптопарой
  const char *m_name;
  TaskHandle_t m_taskHandle;
  static void task(void *p) {
    SchwinnResistButton *self = (SchwinnResistButton *)p;
    while(true) {
      vTaskSuspend(NULL);
      uint8_t startResist = glbCurResist;
      unsigned long start = millis();
      digitalWrite(self->m_outPin, HIGH);
      while (millis() - start < 500 && startResist == glbCurResist) {
        delay(10);
      }
      digitalWrite(self->m_outPin, LOW);
    }
  }

public:
  SchwinnResistButton(const char *name, int outPin) : m_outPin(outPin), m_name(name) {}  
  void begin() {
    pinMode(m_outPin, OUTPUT);
    xTaskCreatePinnedToCore(task, m_name, 2000, this, configMAX_PRIORITIES - 1, &m_taskHandle, 1);
  }
  void activate() {
    debugPrint("activated %d", m_outPin);
    vTaskResume(m_taskHandle);
    const char *s[CHUNKS] = {}; s[4] = m_name;
    display.update(s); //сохранить последнее на экране
  }
} resistUpBtn("UP", 16), resistDnBtn("DN", 15);

class Joystick { //steering, manual resistance and PC mouse control
  enum Setup { ZeroBounce = 40 /* дребезг около начального значения */, MaxSteerAngle = 35 };
  int m_adcNullX, m_adcNullY; //Joystick Calibration
public:
  enum Pins { Yaxis = 12, Button = 0, Xaxis = 14 };
  Joystick() : m_adcNullX(0), m_adcNullY(0) {}
  void begin() {
    pinMode(Pins::Xaxis, INPUT); pinMode(Pins::Button, INPUT_PULLUP); pinMode(Pins::Xaxis, INPUT);
    m_adcNullX = analogRead(Pins::Xaxis);
    if (m_adcNullX <= 0 || m_adcNullX == MAX_ADC_VAL) {
      m_adcNullX = (MAX_ADC_VAL + 1) / 2;
      debugPrint("Fixed adcNullX");
    }
    m_adcNullY = analogRead(Pins::Yaxis);
    if (m_adcNullY <= 0 || m_adcNullY == MAX_ADC_VAL) {
      m_adcNullY = (MAX_ADC_VAL + 1) / 2;
      debugPrint("Fixed adcNullY");
    }
  }
  bool readAngle(Pins pin, float *pVal) { //Joystick read angle from axis
    int adcVal = analogRead(pin);
    int adcNull = (pin == Yaxis) ? m_adcNullY : m_adcNullX;
    if (adcVal == MAX_ADC_VAL) {
      adcVal = analogRead(pin);
    }
    int potVal = adcVal - adcNull;
    float angle = 0.0;
    if (potVal > ZeroBounce || potVal < -ZeroBounce) {
      if (potVal < 0)
        angle = float(potVal) / adcNull * MaxSteerAngle;
      else
        angle = float(potVal) / (MAX_ADC_VAL - adcNull) * MaxSteerAngle;
    }
    *pVal = angle;
    return true;
  }
  bool packedAngle(float *pVal) { //лишнего поля в протоколе steering нет, поэтому хитро пакуем 2 угла и кнопку в одно число
    float angle = 0.0;
    float angleX = 0.0; if (!readAngle(Pins::Xaxis, &angleX)) return false;
    angleX = -int(angleX);
    float angleY = 0.0; if (!readAngle(Pins::Yaxis, &angleY)) return false;
    angleY = -int(angleY);
    bool buttonJoy = (analogRead(Pins::Button) == 0);
    static int buttonPressCount = 0;
    if (buttonJoy) {
      if (++buttonPressCount < 20)
        buttonJoy = false;
    } else {
      buttonPressCount = 0;
    }
    unsigned long nowTime = millis();
    static unsigned long lastBtn = nowTime;
    if (buttonJoy && nowTime - lastBtn > 1000) {
      if (angleY <= -MaxSteerAngle/2) { resistUpBtn.activate(); lastBtn = nowTime; }
      else if (angleY >= MaxSteerAngle/2) { resistDnBtn.activate(); lastBtn = nowTime; }
    }
    if (angleX > 0) {
      angle = angleX + angleY / 100.0;
    } else if (angleX < 0 || angleY != 0) {
      angle = angleX + angleY / 100.0;
    }
    if (buttonJoy) {
      if(angle >= 0.0) angle += 0.001; else angle -= 0.001;
    }
    *pVal = angle;
    return true;
  }
} joystick;

void schwinnEventRecordNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
void schwinnSdr0NotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);

struct ResistanceManager {
  ResistanceManager() : m_lastGrade(0), m_goalGrade(0), m_goalResist(0), m_goalPower(0), m_curPower(0), m_state(IDLE) {}
  int m_lastGrade, m_goalGrade;
  void GoalGrade(int newGrade) { //каждые 50 единиц - смена уровня
    switch(m_state) {
      case RESISTANCE:
      case POWER:
      case IDLE:
        m_lastGrade = m_goalGrade = newGrade;
        m_state = GRADE;
        return;
      case GRADE:
        m_goalGrade = newGrade;
        break;
    }
    doTick();
  }
  int m_goalResist;
  void GoalResistance(int newResistance) { //сам уровень
    m_goalResist = newResistance;
    if (m_goalResist < 1) m_goalResist = 1;
    if (m_goalResist > 25) m_goalResist = 25;
    m_state = RESISTANCE;
    doTick();
  }
  int m_goalPower, m_curPower;
  void GoalPower(int newPower) {           //мощность
    m_goalPower = newPower;
    m_state = POWER;
    doTick();
  }
  enum States { IDLE, GRADE, RESISTANCE, POWER } m_state;
  void doTick() {
    switch(m_state) {
      case RESISTANCE:
        if (m_goalResist > glbCurResist) resistUpBtn.activate(); 
        else if (m_goalResist < glbCurResist) resistDnBtn.activate();
        else m_state = IDLE;
        break;
      case POWER:
        if (m_goalPower > 1.2 * m_curPower) resistUpBtn.activate();
        else if (m_curPower > 1.2 * m_goalPower) resistDnBtn.activate();
        break;
      case IDLE:
        break;
      case GRADE:
        if (m_goalGrade > m_lastGrade + 50) { resistUpBtn.activate(); m_lastGrade += 50; }
        else if (m_goalGrade < m_lastGrade - 50) { resistDnBtn.activate(); m_lastGrade -= 50; }
        break;
    }
  }
  void tick() {
    unsigned long nowTime = millis();
    static unsigned long lastTick = nowTime;
    if (nowTime - lastTick > 1000) {
      doTick();
      lastTick = nowTime;
    }
  }
} resistMng;

struct EspSmartBike {
  EspSmartBike() : m_pBleServer(NULL), m_pSteerAngleChar(NULL), m_pSteerTxChar(NULL), 
    m_hrmChar(BLEUUID((uint16_t)0x2A37), BLECharacteristic::PROPERTY_NOTIFY),
    m_spCadChar(BLEUUID((uint16_t)0x2A5B), BLECharacteristic::PROPERTY_NOTIFY),
    m_powChar(BLEUUID((uint16_t)0x2A63), BLECharacteristic::PROPERTY_NOTIFY),
    m_schwinnEventRecordChar(NULL), m_schwinnSdr0Char(NULL),
    m_schwinnAddr("84:71:27:27:4A:44"), m_actualResistance(0),
    m_connectedToConsumer(false), m_prevConnectedToConsumer(false), m_steeringAuthenticated(false),
    m_lastPower(-1.0),
    m_fitnessMachineFeatureCharacteristics(BLEUUID((uint16_t)0x2ACC), BLECharacteristic::PROPERTY_READ),
    m_indoorBikeDataCharacteristic(BLEUUID((uint16_t)0x2AD2), BLECharacteristic::PROPERTY_NOTIFY),
    m_resistanceLevelRangeCharacteristic(BLEUUID((uint16_t)0x2AD6), BLECharacteristic::PROPERTY_READ),
    m_powerLevelRangeCharacteristic(BLEUUID((uint16_t)0x2AD8), BLECharacteristic::PROPERTY_READ),
    m_fitnessMachineControlPointCharacteristic(BLEUUID((uint16_t)0x2AD9), BLECharacteristic::PROPERTY_INDICATE | BLECharacteristic::PROPERTY_WRITE),
    m_fitnessMachineStatusCharacteristic(BLEUUID((uint16_t)0x2ADA), BLECharacteristic::PROPERTY_NOTIFY),
    m_heartRateCache(0),
    m_avgCadence2x(0),
    m_resistanceSet(4), m_powerSet(100), m_grade(0), m_ctrlQueue(NULL) {}
  BLEServer *m_pBleServer;
  BLECharacteristic *m_pSteerAngleChar;
  BLECharacteristic *m_pSteerTxChar;
  BLECharacteristic m_hrmChar, m_spCadChar, m_powChar;
  BLERemoteCharacteristic *m_schwinnEventRecordChar, *m_schwinnSdr0Char;
  BLEAddress m_schwinnAddr;
  uint8_t m_actualResistance;
  bool m_connectedToConsumer, m_prevConnectedToConsumer, m_steeringAuthenticated;
  double m_lastPower;
  BLECharacteristic m_fitnessMachineFeatureCharacteristics, m_indoorBikeDataCharacteristic, m_resistanceLevelRangeCharacteristic, m_powerLevelRangeCharacteristic, m_fitnessMachineControlPointCharacteristic, m_fitnessMachineStatusCharacteristic;
  uint8_t m_heartRateCache;
  const int16_t m_minPowLev = 0; // power level range settings, no app cares about this
  const int16_t m_maxPowLev = 1500;
  const int16_t m_minPowInc = 1;
  const int16_t m_maxResLev = 25; // resistance level range settings, no app cares about this
  const uint16_t m_indoorBikeDataCharacteristicDef = 0b0000001001000101; // flags for indoor bike data characteristics - no more data, power and cadence (inst), hr
  const uint32_t m_fitnessMachineFeaturesCharacteristicsDef = 0b00000000000000000100000010000010; // flags for Fitness Machine Features Field - cadence, resistance level and inclination level
  const uint32_t m_targetSettingFeaturesCharacteristicsDef = 0b00000000000000000010000000001100;  // flags for Target Setting Features Field - power and resistance level, Indoor Bike Simulation Parameters 

  class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer);
    void onDisconnect(BLEServer *pServer);
  };
  struct FtmsCtrlRequest {
    uint8_t m_size; 
    uint8_t m_data8[8]; 
    FtmsCtrlRequest() : m_size(0) {} 
    FtmsCtrlRequest(const std::string &src) {
      if (src.length() > sizeof(m_data8)) {
        debugPrint("FtmsCtrlRequest ofv: %d", src.length());
        return;
      }
      m_size = src.length();
      if (m_size) memcpy(m_data8, &src[0], m_size);
    }
  };
  class ftmsCtrlPointCallback : public BLECharacteristicCallbacks {  // write performed on the control point characteristic
    void onWrite(BLECharacteristic* pCharacteristic);
  };  
  bool connectToSchwinn() {
    int iter = 0;
    const char *arr[2] = { "Schwinn...", "Schwinn..?" };
    const char *s[CHUNKS] = {}; s[2] = arr[iter++];
    display.update(s);
    BLEClient *pClient = BLEDevice::createClient();
    while(!pClient->connect(m_schwinnAddr)) {
      delay(500);
      s[2] = arr[(iter++) & 1];
      display.update(s);
    }
    s[2] = "SwOK";
    display.update(s);
    debugPrint("Connected to Schwinn");
   
    BLERemoteService *pRemoteService = pClient->getService("3bf58980-3a2f-11e6-9011-0002a5d5c51b");
    if (pRemoteService == nullptr) {
      debugPrint("Failed to find our service UUID");
      return false;
    }
    m_schwinnEventRecordChar = pRemoteService->getCharacteristic("5c7d82a0-9803-11e3-8a6c-0002a5d5c51b");  
    if (m_schwinnEventRecordChar == NULL) {
      debugPrint("Failed to find EventRecord char");
      return false;
    }
    m_schwinnSdr0Char = pRemoteService->getCharacteristic("6be8f580-9803-11e3-ab03-0002a5d5c51b");  
    if (m_schwinnSdr0Char == NULL) {
      debugPrint("Failed to find SDR0 char");
      return false;
    }
    BLERemoteCharacteristic *cmdRecordChar = pRemoteService->getCharacteristic("1717b3c0-9803-11e3-90e1-0002a5d5c51b");  
    if (cmdRecordChar == NULL) {
      debugPrint("Failed to find CommandRecord char");
      return false;
    }
    static uint8_t startHrStreamCmd[] = {0x5 /*len*/, 0x3 /*seq-с потолка*/, 0xd9 /*crc = sum to 0*/, 0, 0x1f /*cmd*/};
    /*bool ok = */cmdRecordChar->writeValue(startHrStreamCmd, sizeof(startHrStreamCmd), true);
    //debugPrint(ok ? "Found all our characteristics, write OK" : "Found all our characteristics, write fail");
   
    m_schwinnEventRecordChar->registerForNotify(schwinnEventRecordNotifyCallback);
    m_schwinnSdr0Char->registerForNotify(schwinnSdr0NotifyCallback);
    return true;
  }
  
  void begin() {
    m_ctrlQueue = xQueueCreate(10, sizeof(FtmsCtrlRequest));
    BLEDevice::init("Schwinn");
    m_pBleServer = BLEDevice::createServer();
    m_pBleServer->setCallbacks(new ServerCallbacks());
  
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
#ifdef HAS_STEERING
    const char *STEERING_SERVICE_UUID = "347b0001-7635-408b-8918-8ff3949ce592", *STEERING_TX_CHAR_UUID = "347b0032-7635-408b-8918-8ff3949ce592", *STEERING_ANGLE_CHAR_UUID = "347b0030-7635-408b-8918-8ff3949ce592";
    //#define STEERING_RX_CHAR_UUID "347b0031-7635-408b-8918-8ff3949ce592"  //write
    debugPrint("Define services...");
    BLEService *pSteerService = m_pBleServer->createService(STEERING_SERVICE_UUID);
    debugPrint("Define characteristics");
    m_pSteerTxChar = pSteerService->createCharacteristic(STEERING_TX_CHAR_UUID, BLECharacteristic::PROPERTY_INDICATE | BLECharacteristic::PROPERTY_READ);
    m_pSteerTxChar->addDescriptor(new BLE2902());
    m_pSteerAngleChar = pSteerService->createCharacteristic(STEERING_ANGLE_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    m_pSteerAngleChar->addDescriptor(new BLE2902());
    debugPrint("Staring BLE service...");
    pSteerService->start();
    pAdvertising->addServiceUUID(STEERING_SERVICE_UUID);
#endif    
#ifdef EXCLUSIVE_HEART
    #define heartRateService BLEUUID((uint16_t)0x180D)
    BLEService *pHeartService = m_pBleServer->createService(heartRateService);
    pHeartService->addCharacteristic(&m_hrmChar);
    static BLEDescriptor heartRateDescriptor(BLEUUID((uint16_t)0x2901));
    heartRateDescriptor.setValue("Heart Rate from Schwinn 570U");
    m_hrmChar.addDescriptor(&heartRateDescriptor);
    m_hrmChar.addDescriptor(new BLE2902());
    pHeartService->start();
#endif
#ifdef EXCLUSIVE_CADENCE
    #define speedCadService BLEUUID((uint16_t)0x1816)
    BLEService *pSpeedCadenceService = m_pBleServer->createService(speedCadService);
    pSpeedCadenceService->addCharacteristic(&m_spCadChar);
    m_spCadChar.addDescriptor(new BLE2902());
    pSpeedCadenceService->start();
    pAdvertising->addServiceUUID(speedCadService);
    pAdvertising->addServiceUUID(heartRateService);
#endif
#ifdef EXCLUSIVE_POWER
    #define powerService BLEUUID((uint16_t)0x1818)
    BLEService *pPowerService = m_pBleServer->createService(powerService);
    pPowerService->addCharacteristic(&m_powChar);
    m_powChar.addDescriptor(new BLE2902());
    pPowerService->start();
    pAdvertising->addServiceUUID(powerService);
#endif
#ifdef EXCLUSIVE_FTMS
    #define fitnessMachineService BLEUUID((uint16_t)0x1826)
    const std::string fitnessData = { 0b00000001, 0b00100000, 0b00000000 };  // advertising data on "Service Data AD Type" - byte of flags (little endian) and two for Fitness Machine Type (little endian) indoor bike supported
    BLEAdvertisementData ftmsAdvertisementData;
    ftmsAdvertisementData.setServiceData(fitnessMachineService, fitnessData);  // already includdes Service Data AD Type ID and Fitness Machine Service UUID with fitnessData 6 bytes
    BLEService *pFitnessService = m_pBleServer->createService(fitnessMachineService);
    pFitnessService->addCharacteristic(&m_indoorBikeDataCharacteristic);
    m_indoorBikeDataCharacteristic.addDescriptor(new BLE2902());
    pFitnessService->addCharacteristic(&m_fitnessMachineFeatureCharacteristics);
    pFitnessService->addCharacteristic(&m_resistanceLevelRangeCharacteristic);
    pFitnessService->addCharacteristic(&m_fitnessMachineControlPointCharacteristic);  
    BLE2902* descr = new BLE2902();
    descr->setIndications(1); // default indications on  
    m_fitnessMachineControlPointCharacteristic.addDescriptor(descr);    
    pFitnessService->addCharacteristic(&m_fitnessMachineStatusCharacteristic);
    m_fitnessMachineStatusCharacteristic.addDescriptor(new BLE2902());
    m_fitnessMachineControlPointCharacteristic.setCallbacks(new ftmsCtrlPointCallback()); // set callback for write
    uint8_t powerRange[6] = { 
      (uint8_t)(m_minPowLev & 0xff),
      (uint8_t)(m_minPowLev >> 8),
      (uint8_t)(m_maxPowLev & 0xff),
      (uint8_t)(m_maxPowLev >> 8),
      (uint8_t)(m_minPowInc & 0xff),
      (uint8_t)(m_minPowInc >> 8)
    };    
    uint8_t resistanceRange[6] = { 
      (uint8_t)(0 & 0xff),
      (uint8_t)(0 >> 8),
      (uint8_t)(m_maxResLev & 0xff),
      (uint8_t)(m_maxResLev >> 8),
      (uint8_t)(1 & 0xff),
      (uint8_t)(1 >> 8)
    };
    m_resistanceLevelRangeCharacteristic.setValue(resistanceRange, 6);
    m_powerLevelRangeCharacteristic.setValue(powerRange, 6);
    uint8_t fitnessMachineFeatureCharacteristicsData[8] = {  // values for setup - little endian order
       (uint8_t)(m_fitnessMachineFeaturesCharacteristicsDef & 0xff),
      (uint8_t)(m_fitnessMachineFeaturesCharacteristicsDef >> 8),
      (uint8_t)(m_fitnessMachineFeaturesCharacteristicsDef >> 16),
      (uint8_t)(m_fitnessMachineFeaturesCharacteristicsDef >> 24),
      (uint8_t)(m_targetSettingFeaturesCharacteristicsDef & 0xff),
      (uint8_t)(m_targetSettingFeaturesCharacteristicsDef >> 8),
      (uint8_t)(m_targetSettingFeaturesCharacteristicsDef >> 16),
      (uint8_t)(m_targetSettingFeaturesCharacteristicsDef >> 24) 
    };
    m_fitnessMachineFeatureCharacteristics.setValue(fitnessMachineFeatureCharacteristicsData, 8); // flags
    pFitnessService->start();
    pAdvertising->setAdvertisementData(ftmsAdvertisementData);
    pAdvertising->addServiceUUID(fitnessMachineService);
#endif

    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // set value to 0x00 to not advertise this parameter
    debugPrint("Starting advertiser...");
    BLEDevice::startAdvertising();

#ifndef SIMULATE_SCHWINN    
    debugPrint("Connect to Schwinn...");
    connectToSchwinn();
#endif
  }
  void doHR(unsigned long nowTime) {
    static uint8_t heart[2] = {0, 0};
    static unsigned long lastHr = nowTime;
    static uint8_t hrRev = 0;
    if (nowTime - lastHr > 500) {
#ifdef SIMULATE_SCHWINN
      m_heartRateCache = 78 + ((nowTime / 1000) % 5);
#else
      if (m_heartRate.HasNew(&hrRev, &m_heartRateCache))
#endif //SIMULATE_SCHWINN
      {
        lastHr = nowTime;
        heart[1] = m_heartRateCache;
#ifdef EXCLUSIVE_HEART
        m_hrmChar.setValue(heart, sizeof(heart));
        m_hrmChar.notify();
#endif //EXCLUSIVE_HEART
        String str = String("H:") + String(m_heartRateCache);
        const char *s[CHUNKS] = {}; s[2] = str.c_str();
        display.update(s);
      }
    }
  }
  void doSteering(unsigned long nowTime) {
    static unsigned long lastPackedAngleTxTime = nowTime;
    float packedAngle = 0.0;
#ifdef HAS_STEERING
    if (!m_steeringAuthenticated) {
      debugPrint("Steering Auth Challenging (delay 250)");
      static uint8_t authSuccess[3] = {0xff, 0x13, 0xff};
      m_pSteerTxChar->setValue(authSuccess, sizeof(authSuccess));
      m_pSteerTxChar->indicate();
      m_steeringAuthenticated = true;
      delay(250);
    }
    if (nowTime - lastPackedAngleTxTime < 30) return;
    if (joystick.packedAngle(&packedAngle)) {
      static float lastPackedAngle = -200.0;
      if (packedAngle == lastPackedAngle && nowTime - lastPackedAngleTxTime < 150) {
        return; // не повторяемся слишком часто
      }
      m_pSteerAngleChar->setValue(packedAngle);
      m_pSteerAngleChar->notify();
      char tick_char = ((nowTime / 1000) & 1) ? ' ' : ':';
      static char last_tick_char = tick_char;
      if (packedAngle != lastPackedAngle || last_tick_char != tick_char) {
        lastPackedAngle = packedAngle;
        char dispSteer[16]; //100.001
        sprintf(dispSteer, "%+07.3f", packedAngle);
        if (dispSteer[0] == '+') dispSteer[0] = '0'; // а то разная ширина
        if (dispSteer[0] == '-') dispSteer[0] = '1';
        //debugPrint(dispSteer);
        dispSteer[3] = tick_char;
        last_tick_char = tick_char;
        static unsigned long lastDisplayUpdate = nowTime;
        if (nowTime - lastDisplayUpdate > 250) {
          lastDisplayUpdate = nowTime;
          const char *s[CHUNKS] = {}; s[0] = dispSteer;
          display.update(s);
        }
      }
      lastPackedAngleTxTime = nowTime;
    }
#endif //HAS_STEERING
  }
  static double CalcPowerNewAlgo(double cad, int resist) {
      static const double coeffs[25][3] = { //from Excel trends
          { -0.0005, 0.6451, -11.599 },
          { 0.0004, 0.773, -12.325 },
          { 0,      1.0186, -17.566 },
          { 0.0008, 1.0256, -16.83 },
          { 0.0049, 0.85, -10.95 },
          { 0.0012, 1.549, -30.093 },
          { 0.0018, 1.7378, -35.606 },
          { 0.0048, 1.5708, -28.509 },
          { 0.0055, 1.6728, -29.06 },
          { 0.007, 1.8186, -31.254 },
          { -0.002, 3.1643, -71.208 },
          { 0.0046, 2.707, -48.821 },
          { 0.0014, 3.5323, -72.418 },
          { 0.0074, 2.8258, -51.258 },
          { 0.0037, 3.7922, -72.192 },
          { 0.0059, 3.7542, -69.219 },
          { 0.0046, 4.0031, -73.953 },
          { 0.0281, 2.3647, -33.012 },
          { 0.0175, 3.1842, -53.739 },
          { 0.0047, 4.5335, -75.077 },
          { 0.0428, 1.7454, -14.955 },
          { 0.0191, 4.044, -61.586 },
          { 0.0518, 2.0345, -18.703 },
          { 0.0325, 3.5994, -44.863 },
          { -0.006, 7.7179, -128.45 }
      };
      int idx = (resist - 1) % 25;
      double ret = coeffs[idx][0] * cad * cad + coeffs[idx][1] * cad + coeffs[idx][2];
      return ret > 0.0 ? ret : 0.0;
  }
  CrankInfo m_crankInfoCache;
  void doPowerMeter(unsigned long nowTime) {
    static uint8_t pow[4] = {}, cad[5] = {2 /*flags*/ };
#ifdef SIMULATE_SCHWINN
    m_lastPower = 123 - ((nowTime / 1000) % 5);
    m_avgCadence2x = 180 + ((nowTime / 1000) % 5);
    int pwr = (int)(m_lastPower + 0.5);
    pow[2] = (byte)(pwr & 0xFF);
    pow[3] = (byte)((pwr >> 8) & 0xFF);
#ifdef EXCLUSIVE_POWER
    m_powChar.setValue(pow, sizeof(pow));
    m_powChar.notify();
#endif
#ifdef EXCLUSIVE_CADENCE
    static uint16_t cr = 0;
    static unsigned long lastOut = 0;
    if (nowTime - lastOut > 1000) {
      lastOut = nowTime;
      cr++;
      //crank revolution #
      cad[1] = (byte)cr; cad[2] = (byte)(cr >> 8);
      static uint16_t tim = 0;
      tim += 120000 / m_avgCadence2x;
      cad[3] = (byte)tim; cad[4] = (byte)(tim >> 8);
      m_spCadChar.setValue(cad, sizeof(cad));
      m_spCadChar.notify();
    }
#endif //EXCLUSIVE_CADENCE
#else //SIMULATE_SCHWINN
    static uint8_t crInfoRev = 0;
    bool newCrankInfo = m_crankInfo.HasNew(&crInfoRev, &m_crankInfoCache);
    if (newCrankInfo || m_lastPower != 0) {      
      if (newCrankInfo) {
        const int averageStepDur = 750; //750ms даём на шаг сопротивления
        if (m_actualResistance == 0) m_actualResistance = m_crankInfoCache.m_reportedResistance;        
        else if (m_crankInfoCache.m_reportedResistance != m_actualResistance) { 
          static unsigned long glbLastResStep = 0;
          while(nowTime - glbLastResStep > averageStepDur) {
            glbLastResStep += averageStepDur;
            if (m_crankInfoCache.m_reportedResistance > m_actualResistance) m_actualResistance++;
            else if (m_crankInfoCache.m_reportedResistance < m_actualResistance ) m_actualResistance--;
            else {
              glbLastResStep = nowTime;
              break;
            }
          }
        }
      }
      static uint8_t oldc = 0xFF;
      bool bSchwinnStopped = (int(nowTime - m_crankInfoCache.m_curSchwinnPowerIncomeTm) > 2000);
      if (oldc != m_crankInfoCache.m_cranks[1] || bSchwinnStopped) do { //если 8 раз подать одно значение, zwift нам весь каденс занулит
        oldc = m_crankInfoCache.m_cranks[1];
        //время может прийти совсем нехорошее, а потом восстановиться - не будем-ка доверять ему, да еще и усредним
        const int avg_count = 5; //в секундах
        static double cranks[avg_count] = {};
        static unsigned long times[avg_count], last_crank_out = 0, last_time = 0;
        unsigned long tick = nowTime * 1024 / 1000;
        static uint8_t idx = 0;
        if (!bSchwinnStopped) {
          double cur_crank = m_crankInfoCache.m_cranks[0] / 256.0 + m_crankInfoCache.m_cranks[1] + m_crankInfoCache.m_cranks[2] * 256;
          cranks[idx % avg_count] = cur_crank;
          times[idx % avg_count] = tick;
          idx++;
          if (idx < avg_count) break;
          //debugPrint("no breaked cadence");
          double total_cranks = cur_crank - cranks[idx % avg_count];
          unsigned long total_ticks = tick - times[idx % avg_count];
          if (total_cranks != 0) {
            unsigned long dt = (unsigned long)(total_ticks / total_cranks + 0.5);
            m_avgCadence2x = 120000 / dt;
            last_time += dt;
            last_crank_out++;
          }
        } else {
          m_avgCadence2x = 0;
          //зануляемся, выдавая тот же crank и время
          debugPrint("cadence bSchwinnStopped: m=%d, mc=%d, in=%d", (int)millis(), (int)nowTime, (int)m_crankInfoCache.m_curSchwinnPowerIncomeTm);
        }
        //crank revolution #
        cad[1] = (byte)last_crank_out; cad[2] = (byte)(last_crank_out >> 8);
        //time
        cad[3] = (byte)last_time; cad[4] = (byte)(last_time >> 8);
#ifdef EXCLUSIVE_CADENCE
        m_spCadChar.setValue(cad, sizeof(cad));
        m_spCadChar.notify();
#endif
      } while(0);
      static int glbLastDevTime = 0; //в 1024-х долях секунды
      double power = 0; //schwinn stopped
      if (!bSchwinnStopped) {
        power =  m_lastPower;
        static unsigned long glbLastSchwinnPowerIncome = nowTime;
        static double oldCranks = -1.0;
        double newCranks = m_crankInfoCache.m_cranks[0] / 256.0 + m_crankInfoCache.m_cranks[1] + m_crankInfoCache.m_cranks[2] * 256.0;
        while (newCranks < oldCranks) newCranks += 65536.0;
        if (oldCranks < 0) {
            oldCranks = newCranks; 
        }
        double dCranks = newCranks - oldCranks;
        if (dCranks > 1.0) {
            oldCranks = newCranks;
            double powerByTicks = -10000.0, powerByDeviceTime = -10000.0;
            unsigned long dTicks = m_crankInfoCache.m_curSchwinnPowerIncomeTm - glbLastSchwinnPowerIncome;
            glbLastSchwinnPowerIncome = m_crankInfoCache.m_curSchwinnPowerIncomeTm;
            if (dTicks > 0 && dTicks < 4000) {
                double cadenceByTicks = dCranks * 60000.0 / dTicks;
                powerByTicks = CalcPowerNewAlgo(cadenceByTicks, m_actualResistance);
            }
            int dTim = m_crankInfoCache.m_lastDeviceTime - glbLastDevTime;
            glbLastDevTime = m_crankInfoCache.m_lastDeviceTime;
            if (dTim > 0 && dTim < 4000) {
                double cadenceByDev = dCranks * 61440.0 / dTicks;
                powerByDeviceTime = CalcPowerNewAlgo(cadenceByDev, m_actualResistance);
            }
            double deltaPowerByTicks = fabs(powerByTicks - m_lastPower), deltaPowerByDev = fabs(powerByDeviceTime - m_lastPower);
            power = (deltaPowerByDev < deltaPowerByTicks) ? powerByDeviceTime : powerByTicks;
            resistMng.m_curPower = (int)power;
        }
      }
      if (m_lastPower == -1.0 || fabs(m_lastPower - power) < 100.0)
          m_lastPower = power;
      else
          m_lastPower += (power - m_lastPower) / 2.0;
      if (m_lastPower < 0)
          m_lastPower = 0;
      int pwr = (int)(m_lastPower + 0.5);
      pow[2] = (byte)(pwr & 0xFF);
      pow[3] = (byte)((pwr >> 8) & 0xFF);
#ifdef EXCLUSIVE_POWER
        m_powChar.setValue(pow, sizeof(pow));
        m_powChar.notify();
#endif

      String rr(m_actualResistance);
      String spwr(pwr);
      const char *s[CHUNKS] = {}; s[3] = rr.c_str(); s[5] = spwr.c_str();
      display.update(s);
    }
#endif //SIMULATE_SCHWINN
  }

  int m_avgCadence2x;
  uint8_t m_resistanceSet;
  int16_t m_powerSet, m_grade;
  void doFTMS(unsigned long nowTime) {
    resistMng.tick();
    static unsigned long lastTx = nowTime;
    if (nowTime - lastTx > 200) {
      lastTx = nowTime;
      int16_t powerOut = (int16_t)m_lastPower;
      if (powerOut < 0) powerOut = 0;
      uint8_t indoorBikeDataCharacteristicData[] = {        // values for setup - little endian order
        (uint8_t)(m_indoorBikeDataCharacteristicDef & 0xff),  
        (uint8_t)(m_indoorBikeDataCharacteristicDef >> 8), 
        (uint8_t)(m_avgCadence2x & 0xff),
        (uint8_t)(m_avgCadence2x >> 8), 
        (uint8_t)(powerOut & 0xff), 
        (uint8_t)(powerOut >> 8), 
        m_heartRateCache,
      };
#ifdef EXCLUSIVE_FTMS
      m_indoorBikeDataCharacteristic.setValue(indoorBikeDataCharacteristicData, sizeof(indoorBikeDataCharacteristicData));
      m_indoorBikeDataCharacteristic.notify();
      //debugPrint("FTMS PowerOut: %d", powerOut);
#endif
    }
    FtmsCtrlRequest ctrl;
    if (xQueueReceive(m_ctrlQueue, &ctrl, 0)) {
      uint8_t value[] = { 0x80, 0, 0x02 }; // confirmation data, default - 0x02 - Op "Code not supported", no app cares
      if (ctrl.m_size == 0) { debugPrint("Empty control request"); } else {
        value[1] = (uint8_t)ctrl.m_data8[0];
        switch (value[1]) {
        default:
          debugPrint(ctrl.m_size, ctrl.m_data8, "Control request");
          break;
        case 0x00:
          debugPrint("request control");
          value[2] = 0x01; //ok
          break;
        case 0x01:
          debugPrint("request reset");
          value[2] = 0x01;
          break;
        case 0x03:
          m_grade = (ctrl.m_data8[2] << 8) + ctrl.m_data8[1];
          m_grade *= 10;
          value[2] = 0x01;
          resistMng.GoalGrade(m_grade);
          debugPrint("inclination level: %d", m_grade);
          break;
        case 0x04:
          m_resistanceSet = ctrl.m_data8[1];
          resistMng.GoalResistance(m_resistanceSet);
          debugPrint("resistance level: %d", m_resistanceSet);
          value[2] = 0x01;
          break;
        case 0x07:
          debugPrint("start the training");
          value[2] = 0x01;
          break;
        case 0x05:
          m_powerSet = (ctrl.m_data8[2] << 8) + ctrl.m_data8[1];
          resistMng.GoalPower(m_powerSet);
          debugPrint("power level: %d", m_powerSet);
          value[2] = 0x01;
          break;
        case 0x11: {
            m_grade = (ctrl.m_data8[4] << 8) + ctrl.m_data8[3];
            resistMng.GoalGrade(m_grade);
            debugPrint("status update: grade=%d", m_grade);
            uint8_t statusValue[7] = { 0x12, (uint8_t)ctrl.m_data8[1], (uint8_t)ctrl.m_data8[2], (uint8_t)ctrl.m_data8[3], (uint8_t)ctrl.m_data8[4], (uint8_t)ctrl.m_data8[5], (uint8_t)ctrl.m_data8[6] };
            m_fitnessMachineStatusCharacteristic.setValue(statusValue, 7); // Fitness Machine Status updated, no app cares
            m_fitnessMachineStatusCharacteristic.notify();    
            value[2] = 0x01;
            break;
          }
        }
      }
      m_fitnessMachineControlPointCharacteristic.setValue(value, sizeof(value));
      m_fitnessMachineControlPointCharacteristic.indicate();
    }
  }
  void main() {
    doSteering(millis());
    doHR(millis());
    doPowerMeter(millis());
    doFTMS(millis());
  }
  void loop() {
    if (m_connectedToConsumer)
      main();
    if (!m_connectedToConsumer && m_prevConnectedToConsumer) {
      debugPrint("Nothing connected, start advertising (delay 300)");
      delay(300); // give the bluetooth stack the chance to get things ready
      BLEDevice::startAdvertising(); // [re]start advertising
      m_prevConnectedToConsumer = false;
      const char *s[CHUNKS] = {}; s[0] = "ReconGC...";
      display.update(s);
    }
    if (m_connectedToConsumer && !m_prevConnectedToConsumer) {
      m_prevConnectedToConsumer = true;
      debugPrint("Ready to GATT client");
    }
  }
  QueueHandle_t m_ctrlQueue;
  void safeQueueNewCtrl(const FtmsCtrlRequest &controlRequest) {
    xQueueSend(m_ctrlQueue, &controlRequest, 1);
  }
  SafeHolder<uint8_t> m_heartRate;
  void safeSetNewHeartRate(uint8_t hr) {
    m_heartRate = hr;
  }
  SafeHolder<CrankInfo> m_crankInfo;
  void safeSetNewCrank(unsigned long tm, uint8_t *pData) {
    CrankInfo ci;
    ci.m_curSchwinnPowerIncomeTm = tm;
    ci.m_reportedResistance = pData[16];
    ci.m_lastDeviceTime = pData[8] | (pData[9] << 8);
    ci.m_cranks[0] = pData[3];
    ci.m_cranks[1] = pData[4];
    ci.m_cranks[2] = pData[5];
    m_crankInfo = ci;
  }
} smartBike;

void EspSmartBike::ServerCallbacks::onConnect(BLEServer *pServer) {
  smartBike.m_connectedToConsumer = true;
  BLEDevice::startAdvertising();
  debugPrint("EspSmartBike::onConnect");
}
void EspSmartBike::ServerCallbacks::onDisconnect(BLEServer *pServer) { 
  smartBike.m_connectedToConsumer = false;
  smartBike.m_steeringAuthenticated = false;
  debugPrint("EspSmartBike::onDisconnect");
}

void setup() {
  debugPrint.begin();
  debugPrint("Enter Setup");

  display.begin();
  const char *s[CHUNKS] = {"Starting...", "R:"};
  display.update(s);

  resistUpBtn.begin(); resistDnBtn.begin();
  joystick.begin();
  smartBike.begin();

  s[0] = "WaitGCli...";
  display.update(s);
  debugPrint("Exit Setup");
}
void loop() {
  smartBike.loop();
}

void schwinnEventRecordNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  if (length == 17 && pData[0] == 17 && pData[1] == 32) {
    glbCurResist = pData[16];
    //debugPrint("schwinnEventRecordNotifyCallback17 r=%d", (int)glbCurResist);
    smartBike.safeSetNewCrank(millis(), pData);
  }
}

void schwinnSdr0NotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  //debugPrint("schwinnSdr0NotifyCallback");
  if (length == 20 && pData[15] == 0x5a) {
    smartBike.safeSetNewHeartRate(pData[16]);
  }
}

void EspSmartBike::ftmsCtrlPointCallback::onWrite(BLECharacteristic *pCharacteristic) {
  //debugPrint("ftmsCtrlPointCallback.w");
  smartBike.safeQueueNewCtrl(EspSmartBike::FtmsCtrlRequest(pCharacteristic->getValue()));
}
