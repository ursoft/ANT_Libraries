//по мотивам https://raw.githubusercontent.com/matandoocorpo/Zwift-Steer/main/steerer.ino
//Сборка в Platformio: не работает ADC, читает MAX (конфликт с BLE)
#include <BLEDevice.h>
#include <BLE2902.h>
#include "SSD1306Wire.h"

//#define DEBUG
struct DebugPrint {
  void begin() {
#ifdef DEBUG
    Serial.begin(115200);
#endif
  }
  void operator()(const char *ln) {
#ifdef DEBUG
    Serial.println(ln);
#endif
  }
} debugPrint;

enum Chunks { Line1, Line2, Line3, Line2a, Line2b, Line3a, CHUNKS };
class Display {
  SSD1306Wire m_display;
  String m_lines[CHUNKS];
  const int m_x[CHUNKS] = {0,  0,  0, 30, 70, 80};
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
      if(newLine[i] && String(newLine[i]) != m_lines[i]) {
        ch = true;
        m_lines[i] = newLine[i];
      }
    }
    if(ch) {
      m_display.clear();
      for(int i = 0; i < CHUNKS; i++) {
        m_display.drawString(m_x[i], m_y[i], m_lines[i]);
      }
      m_display.display();
    }
  }
} display;

const int MaxAdcVal = 4095; // ESP32 ADC is 12bit

class SchwinnResistButton { //resistance control - up and down
  const int m_inPin, m_outPin; // с какого пина на какой транслировать меандр выборки
  const char *m_name;
  bool m_pressed, m_pressInProgress;
public:
  SchwinnResistButton(const char *name, int inPin, int outPin) : m_inPin(inPin), m_outPin(outPin), m_name(name), m_pressed(false), m_pressInProgress(false) {}
  void begin() {
    pinMode(m_inPin, INPUT); 
    pinMode(m_outPin, INPUT);
  }
  void tick() {
    unsigned long nowTime = millis();
    if(nowTime == 0) ++nowTime;
    if(analogRead(m_inPin) >= MaxAdcVal/2 && m_pressed) {
      pinMode(m_outPin, OUTPUT);
      digitalWrite(m_outPin, HIGH);
      m_pressInProgress = true;
    } else {
      if(m_pressInProgress) {
        m_pressed = false;
        m_pressInProgress = false;
        digitalWrite(m_outPin, LOW);
        pinMode(m_outPin, INPUT);
      }
    }
  }
  void OnJoyButton(bool needChange, SchwinnResistButton *otherBtn) { //вызовется, если пользователь нажал кнопку при отклонении (needChange: отклонение было до упора)
    if(needChange) {
      /*if(otherBtn->m_pressed) {
        otherBtn->m_pressed = false;
        otherBtn->m_pressStartTime = 0;
        otherBtn->tick();
      }*/
      m_pressed = true;
      tick();
      const char *s[CHUNKS] = {}; s[4] = m_name;
      display.update(s); //сохранить последнее на экране
    }
  }
} resistUpBtn("UP", 15, 2), resistDnBtn("DN", 39, 16);

void smartDelay(int ms) {
  for(int i = 0; i < ms; i++) {
    resistUpBtn.tick(); resistDnBtn.tick(); //нельзя проспать меандр, он всего 10мс каждые 50 мс
    ::delay(1);
  }
}

class Joystick { //steering, manual resistance and PC mouse control
  enum Setup { ZeroBounce = 40 /* дребезг около начального значения */, MaxSteerAngle = 35 };
  int m_adcNullX, m_adcNullY; //Joystick Calibration
  int m_buttonPressCount;
public:
  enum Pins { Yaxis = 12, Button = 13, Xaxis = 14 };
  Joystick() : m_adcNullX(0), m_adcNullY(0), m_buttonPressCount(0) {}
  void begin() {
    pinMode(Pins::Xaxis, INPUT); pinMode(Pins::Button, INPUT); pinMode(Pins::Xaxis, INPUT);
    m_adcNullX = analogRead(Pins::Xaxis);
    if (m_adcNullX <= 0 || m_adcNullX == MaxAdcVal) {
      m_adcNullX = (MaxAdcVal + 1) / 2;
      debugPrint("Fixed adcNullX");
    }
    m_adcNullY = analogRead(Pins::Yaxis);
    if (m_adcNullY <= 0 || m_adcNullY == MaxAdcVal) {
      m_adcNullY = (MaxAdcVal + 1) / 2;
      debugPrint("Fixed adcNullY");
    }
  }
  float readAngle(Pins pin) { //Joystick read angle from axis
    int adcVal = analogRead(pin);
    int adcNull = (pin == Yaxis) ? m_adcNullY : m_adcNullX;
    if(adcVal == MaxAdcVal) {
      ::delay(5);
      adcVal = analogRead(pin);
    }
    int potVal = adcVal - adcNull;
    float angle = 0.0;
    if (potVal > ZeroBounce || potVal < -ZeroBounce) {
      if(potVal < 0)
        angle = float(potVal) / adcNull * MaxSteerAngle;
      else
        angle = float(potVal) / (MaxAdcVal - adcNull) * MaxSteerAngle;
    }
    return angle;
  }
  float packedAngle() { //лишнего поля в протоколе steering нет, поэтому хитро пакуем 2 угла и кнопку в одно число
    float angle = 0.0;
    float angleX = int(readAngle(Pins::Xaxis));
    float angleY = readAngle(Pins::Yaxis);
    bool buttonJoy = (analogRead(Pins::Button) == 0);
    if(angleX > 0) {
      angle = angleX + angleY / 100.0;
    } else if(angleX < 0 || angleY != 0) {
      angle = angleX + angleY / 100.0;
      if(buttonJoy && ++m_buttonPressCount > 10) {
        m_buttonPressCount = 0;
        resistUpBtn.OnJoyButton(angleY <= -MaxSteerAngle + 1, &resistDnBtn);
        resistDnBtn.OnJoyButton(angleY >= MaxSteerAngle - 1, &resistUpBtn);
      }
      if(!buttonJoy)
        m_buttonPressCount = 0;
    } else if(buttonJoy) {
      smartDelay(10);
      if(++m_buttonPressCount < 10) return angle; //anti-bounce
      m_buttonPressCount = 0;
      angle = 100.0; //button pressed
    } else { // по нулям
      //smartDelay(25);
      m_buttonPressCount = 0;
    }
    return angle;
  }
} joystick;

void schwinnEventRecordNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);
void schwinnSdr0NotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);

struct EspSmartBike {
  EspSmartBike() : m_pBleServer(NULL), m_pSteerAngleChar(NULL), m_pSteerTxChar(NULL), 
    m_hrmChar(BLEUUID((uint16_t)0x2A37), BLECharacteristic::PROPERTY_NOTIFY),
    m_spCadChar(BLEUUID((uint16_t)0x2A5B), BLECharacteristic::PROPERTY_NOTIFY),
    m_powChar(BLEUUID((uint16_t)0x2A63), BLECharacteristic::PROPERTY_NOTIFY),
    m_schwinnEventRecordChar(NULL), m_schwinnSdr0Char(NULL),
    m_schwinnAddr("84:71:27:27:4A:44"), m_heartRate(0), m_bHasNewHeartRate(false),
    m_reportedResistance(0), m_actualResistance(0),
    m_connectedToConsumer(false), m_prevConnectedToConsumer(false), m_steeringAuthenticated(false),
    m_lastPower(-1.0), m_lastDeviceTime(0), m_curSchwinnPowerIncome(0) { m_cranks[0] = m_cranks[1] = m_cranks[2]; }
  BLEServer *m_pBleServer;
  BLECharacteristic *m_pSteerAngleChar;
  BLECharacteristic *m_pSteerTxChar;
  BLECharacteristic m_hrmChar, m_spCadChar, m_powChar;
  BLERemoteCharacteristic *m_schwinnEventRecordChar, *m_schwinnSdr0Char;
  BLEAddress m_schwinnAddr;
  uint8_t m_heartRate; bool m_bHasNewHeartRate; 
  uint8_t m_reportedResistance, m_actualResistance;
  bool m_connectedToConsumer, m_prevConnectedToConsumer, m_steeringAuthenticated;
  double m_lastPower;
  int m_lastDeviceTime;
  unsigned long m_curSchwinnPowerIncome;
  uint8_t m_cranks[3];
  class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer);
    void onDisconnect(BLEServer *pServer);
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
    s[2] = "Schwinn OK";
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
    BLEDevice::init("Schwinn");
    debugPrint("Connect to Schwinn...");
    connectToSchwinn();

    m_pBleServer = BLEDevice::createServer();
    m_pBleServer->setCallbacks(new ServerCallbacks());
  
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

    #define heartRateService BLEUUID((uint16_t)0x180D)
    BLEService *pHeartService = m_pBleServer->createService(heartRateService);
    pHeartService->addCharacteristic(&m_hrmChar);
    static BLEDescriptor heartRateDescriptor(BLEUUID((uint16_t)0x2901));
    heartRateDescriptor.setValue("Heart Rate from Schwinn 570U");
    m_hrmChar.addDescriptor(&heartRateDescriptor);
    m_hrmChar.addDescriptor(new BLE2902());
    /*static BLECharacteristic sensorPositionCharacteristic(BLEUUID((uint16_t)0x2A38), BLECharacteristic::PROPERTY_READ);
    pHeartService->addCharacteristic(&sensorPositionCharacteristic);
    sensorPositionDescriptor.setValue("Position: chest or Schwinn handles");
    sensorPositionCharacteristic.addDescriptor(&sensorPositionDescriptor);*/
    pHeartService->start();

    #define speedCadService BLEUUID((uint16_t)0x1816)
    BLEService *pSpeedCadenceService = m_pBleServer->createService(speedCadService);
    pSpeedCadenceService->addCharacteristic(&m_spCadChar);
    m_spCadChar.addDescriptor(new BLE2902());
    pSpeedCadenceService->start();

    #define powerService BLEUUID((uint16_t)0x1818)
    BLEService *pPowerService = m_pBleServer->createService(powerService);
    pPowerService->addCharacteristic(&m_powChar);
    m_powChar.addDescriptor(new BLE2902());
    pPowerService->start();

    debugPrint("Define the advertiser...");
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->addServiceUUID(STEERING_SERVICE_UUID);
    pAdvertising->addServiceUUID(heartRateService);
    pAdvertising->addServiceUUID(speedCadService);
    pAdvertising->addServiceUUID(powerService);
    pAdvertising->setMinPreferred(0x06); // set value to 0x00 to not advertise this parameter
    debugPrint("Starting advertiser...");
    BLEDevice::startAdvertising();
    debugPrint("Waiting a client connection to notify...");
  }
  void doHR() {
    static uint8_t heart[2] = {0, 0};
    unsigned long nowHr = millis();
    static unsigned long lastHr = nowHr;
    if((nowHr - lastHr > 500 && m_bHasNewHeartRate) || lastHr == nowHr /*first time*/) {
      lastHr = nowHr;
      heart[1] = m_heartRate;
      m_bHasNewHeartRate = false;
      m_hrmChar.setValue(heart, sizeof(heart));
      m_hrmChar.notify();
      String str = String("H:") + String(m_heartRate);
      const char *s[CHUNKS] = {}; s[2] = str.c_str();
      display.update(s);
    }
  }
  void doSteering() {
    float packedAngle = joystick.packedAngle();
    static float lastPackedAngle = -200.0;
    static unsigned long lastPackedAngleTxTime = 0;
    unsigned long nowTime = millis();
    if(packedAngle == lastPackedAngle && nowTime - lastPackedAngleTxTime < 150) {
      smartDelay(10);
      return; // не повторяемся слишком часто
    }
    String sPackedAngle(packedAngle);
    //debugPrint(sPackedAngle.c_str());
    m_pSteerAngleChar->setValue(packedAngle);
    m_pSteerAngleChar->notify();
    const char *s[CHUNKS] = {}; s[0] = sPackedAngle.c_str();
    display.update(s);
    smartDelay(50);
    lastPackedAngle = packedAngle;
    lastPackedAngleTxTime = nowTime;
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
  void doPowerMeter() {
    unsigned long millis_cache = millis();
    if(m_reportedResistance || m_lastPower != 0) {      
      if(m_reportedResistance) {
        const int averageStepDur = 750; //750ms даём на шаг сопротивления
        if(m_actualResistance == 0) m_actualResistance = m_reportedResistance;        
        else if(m_reportedResistance != m_actualResistance) { 
          static unsigned long glbLastResStep = 0;
          while(millis_cache - glbLastResStep > averageStepDur) {
            glbLastResStep += averageStepDur;
            if(m_reportedResistance > m_actualResistance) m_actualResistance++;
            else if(m_reportedResistance < m_actualResistance )m_actualResistance--;
            else {
              glbLastResStep = millis_cache;
              break;
            }
          }
        }
      }
      m_reportedResistance = 0;
      static uint8_t oldc = 0xFF;
      bool bSchwinnStopped = (millis_cache - m_curSchwinnPowerIncome > 2000);
      if (oldc != m_cranks[1] || bSchwinnStopped) do { //если 8 раз подать одно значение, zwift нам весь каденс занулит
        oldc = m_cranks[1];
        static uint8_t cad[5] = {};
        //время может прийти совсем нехорошее, а потом восстановиться - не будем-ка доверять ему, да еще и усредним
        const int avg_count = 5; //в секундах
        static double cranks[avg_count] = {};
        static unsigned long times[avg_count], last_crank_out = 0, last_time = 0;
        unsigned long tick = millis_cache * 1024 / 1000;
        static uint8_t idx = 0;
        if (!bSchwinnStopped) {
            double cur_crank = m_cranks[0] / 256.0 + m_cranks[1] + m_cranks[2] * 256;
            cranks[idx % avg_count] = cur_crank;
            times[idx % avg_count] = tick;
            idx++;
            if (idx < avg_count) break;
            //debugPrint("no breaked cadence");
            double total_cranks = cur_crank - cranks[idx % avg_count];
            unsigned long total_ticks = tick - times[idx % avg_count];
            if (total_cranks != 0) {
                last_time += (unsigned long)(total_ticks / total_cranks + 0.5);
                last_crank_out++;
            }
        } else {
            //зануляемся, выдавая тот же crank и время
            //debugPrint("cadence bSchwinnStopped");
        }
        cad[0] = 2; //flags
        //crank revolution #
        cad[1] = (byte)last_crank_out; cad[2] = (byte)(last_crank_out >> 8);
        //time
        cad[3] = (byte)last_time; cad[4] = (byte)(last_time >> 8);
        m_spCadChar.setValue(cad, sizeof(cad));
        m_spCadChar.notify();
        //Serial.printf("cadence notified: %d\n", (int)last_crank_out);
      } while(0);
      static int glbLastDevTime = 0; //в 1024-х долях секунды
      double power = 0; //schwinn stopped
      if(!bSchwinnStopped) {
        power =  m_lastPower;
        static unsigned long glbLastSchwinnPowerIncome = millis_cache;
        static double oldCranks = -1.0;
        double newCranks = m_cranks[0] / 256.0 + m_cranks[1] + m_cranks[2] * 256.0;
        while (newCranks < oldCranks) newCranks += 65536.0;
        if (oldCranks < 0) {
            oldCranks = newCranks; 
        }
        double dCranks = newCranks - oldCranks;
        if (dCranks > 1.0) {
            oldCranks = newCranks;
            double powerByTicks = -10000.0, powerByDeviceTime = -10000.0;
            unsigned long dTicks = m_curSchwinnPowerIncome - glbLastSchwinnPowerIncome;
            glbLastSchwinnPowerIncome = m_curSchwinnPowerIncome;
            if (dTicks > 0 && dTicks < 4000) {
                double cadenceByTicks = dCranks * 60000.0 / dTicks;
                powerByTicks = CalcPowerNewAlgo(cadenceByTicks, m_actualResistance);
            }
            int dTim = m_lastDeviceTime - glbLastDevTime;
            glbLastDevTime = m_lastDeviceTime;
            if (dTim > 0 && dTim < 4000) {
                double cadenceByDev = dCranks * 61440.0 / dTicks;
                powerByDeviceTime = CalcPowerNewAlgo(cadenceByDev, m_actualResistance);
            }
            double deltaPowerByTicks = fabs(powerByTicks - m_lastPower), deltaPowerByDev = fabs(powerByDeviceTime - m_lastPower);
            power = (deltaPowerByDev < deltaPowerByTicks) ? powerByDeviceTime : powerByTicks;
        }
      }
      static uint8_t pow[4] = {};
      if (m_lastPower == -1.0 || fabs(m_lastPower - power) < 100.0)
          m_lastPower = power;
      else
          m_lastPower += (power - m_lastPower) / 2.0;
      if (m_lastPower < 0)
          m_lastPower = 0;
      int pwr = (int)(m_lastPower + 0.5);
      pow[2] = (byte)(pwr & 0xFF);
      pow[3] = (byte)((pwr >> 8) & 0xFF);
      m_powChar.setValue(pow, sizeof(pow));
      m_powChar.notify();

      String rr(m_actualResistance);
      String spwr(pwr);
      const char *s[CHUNKS] = {}; s[3] = rr.c_str(); s[5] = spwr.c_str();
      display.update(s);
    }
  }
  void main() {
    doSteering();
    doHR();
    doPowerMeter();
    //doFTMS();
  }
  void loop() {
    if(m_connectedToConsumer) {
      if(m_steeringAuthenticated) {
        main();
      } else {
        debugPrint("Steering Auth Challenging");
        static uint8_t authSuccess[3] = {0xff, 0x13, 0xff};
        m_pSteerTxChar->setValue(authSuccess, sizeof(authSuccess));
        m_pSteerTxChar->indicate();
        m_steeringAuthenticated = true;
        smartDelay(250);
      }
    }
    if (!m_connectedToConsumer && m_prevConnectedToConsumer) {
      smartDelay(300); // give the bluetooth stack the chance to get things ready
      m_pBleServer->startAdvertising(); // [re]start advertising
      debugPrint("Nothing connected, start advertising");
      m_prevConnectedToConsumer = false;
      const char *s[CHUNKS] = {}; s[0] = "ReconnGC...";
      display.update(s);
    }
    if (m_connectedToConsumer && !m_prevConnectedToConsumer) {
      m_prevConnectedToConsumer = true;
      debugPrint("Waiting GATT client...");
    }
  }
} smartBike;

void EspSmartBike::ServerCallbacks::onConnect(BLEServer *pServer) {
  smartBike.m_connectedToConsumer = true;
  BLEDevice::startAdvertising();
  debugPrint("onConnect");
}
void EspSmartBike::ServerCallbacks::onDisconnect(BLEServer *pServer) { 
  smartBike.m_connectedToConsumer = false;
  smartBike.m_steeringAuthenticated = false;
  debugPrint("onDisconnect");
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

void schwinnEventRecordNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, 
                                             uint8_t *pData, size_t length, bool isNotify) {
  //debugPrint("schwinnEventRecordNotifyCallback");
  if(length == 17 && pData[0] == 17 && pData[1] == 32) {
    smartBike.m_curSchwinnPowerIncome = millis();
    smartBike.m_reportedResistance = pData[16];
    smartBike.m_lastDeviceTime = pData[8] | (pData[9] << 8);
    smartBike.m_cranks[0] = pData[3];
    smartBike.m_cranks[1] = pData[4];
    smartBike.m_cranks[2] = pData[5];
  }
}

void schwinnSdr0NotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, 
                                     uint8_t *pData, size_t length, bool isNotify) {
  //debugPrint("schwinnSdr0NotifyCallback");
  if(length == 20 && pData[15] == 0x5a) {
    smartBike.m_bHasNewHeartRate = true;
    smartBike.m_heartRate = pData[16];
  }
}
