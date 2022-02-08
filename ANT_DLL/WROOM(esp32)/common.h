#include <BLEDevice.h>
#include <BLE2902.h>
#include "SSD1306Wire.h"
#include "defines.h"
const int MAX_ADC_VAL = 4095; // ESP32 ADC is 12bit

struct DebugPrint {
  SemaphoreHandle_t m_sema = NULL;  
  void begin() {
#ifdef DEBUG
    Serial.begin(115200);
    m_sema = xSemaphoreCreateMutex();
#endif
  }
  template <typename ...Args>
  void operator()(const char *frm, Args ...args) {
#ifdef DEBUG
    xSemaphoreTake(m_sema, portMAX_DELAY);
    char b[256], *bp = b, *be = b + sizeof(b);
    bp += snprintf(bp, be - bp, "[c%d] ", xPortGetCoreID());
    bp += snprintf(bp, be - bp, frm, args...);
    Serial.println(b);
    xSemaphoreGive(m_sema);
#endif
  }
  void operator()(int aBytes, const void *apBytes, const char *msg) {
#ifdef DEBUG
    xSemaphoreTake(m_sema, portMAX_DELAY);
    char b[256], *bp = b, *be = b + sizeof(b);
    bp += snprintf(bp, be - bp, "[c%d] %s: ", xPortGetCoreID(), msg);
    for(int i = 0; i < aBytes && be > bp + 1; i++) {
      bp += snprintf(bp, be - bp, "%02X ", (int)((byte *)apBytes)[i]);
    }
    Serial.println(b);
    xSemaphoreGive(m_sema);
#endif
  }
} debugPrint;

template<typename T> class SafeHolder {
  SemaphoreHandle_t m_sema;
  T m_val;
  uint8_t m_rev;
public:
  SafeHolder() : m_rev(0) { m_sema = xSemaphoreCreateMutex(); }
  SafeHolder &operator =(const T &val) {
    xSemaphoreTake(m_sema, portMAX_DELAY);
    m_val = val;
    m_rev++;
    xSemaphoreGive(m_sema);
    return *this;
  }
  bool HasNew(uint8_t *ioRev, T *dest) {
    xSemaphoreTake(m_sema, portMAX_DELAY);
    bool ret = false;
    if(*ioRev != m_rev) {
      *ioRev = m_rev;
      *dest = m_val;
      ret = true;
    }
    xSemaphoreGive(m_sema);
    return ret;
  }
};
