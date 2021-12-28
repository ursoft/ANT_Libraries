#include "defines.h"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>
#include <cmath>
#include <sysinfoapi.h>

const char *SCHWINN_SERV = "3bf58980-3a2f-11e6-9011-0002a5d5c51b";
const char *SCHWINN_CHAR = "5c7d82a0-9803-11e3-8a6c-0002a5d5c51b";
const char *SCHWINN_SHORT_CHAR = "0x5c7d82a0";

HMODULE org = LoadLibraryA("BleWin10Lib.org.dll");
typedef bool (*fptr_bool_ptr)(void *);
typedef bool (*fptr_bool_ptr3)(void *, void *, void *);
typedef void (*fptr_void_ptr)(void *);
typedef void (*fptr_void_void)();

std::unique_ptr<std::thread> glbSteeringThread;
HANDLE glbWakeSteeringThread = INVALID_HANDLE_VALUE;
float glbSteeringTask = 0.0, glbSteeringCurrent = 0.0;
bool glbTerminate = false;

char *new_char_zwift(int sz) {
    HANDLE heap = GetProcessHeap();
    char *ret = (char *)HeapAlloc(heap, HEAP_ZERO_MEMORY, sz);
    return ret;
}
typedef void* (* malloc_fn)(size_t);
char* new_char_dll(int sz) {
    static HMODULE heapLib = LoadLibrary("api-ms-win-crt-heap-l1-1-0.dll");
    static malloc_fn f = (malloc_fn)GetProcAddress(heapLib, "malloc");
    char* ret = (char *)f(sz);
    return ret;
}
class BleResponsePool {
    BleResponsePool(const BleResponsePool &);
    BleResponsePool & operator=(const BleResponsePool &);

    std::vector< char *> data;
public:
    BleResponsePool() {}
    char* Allocate(int sz) {
        char* ret = (char *)malloc(sz);
        data.emplace_back(ret);
        return ret;
    }
    ~BleResponsePool() {
        for (size_t i = 0; i < data.size(); i++)
            free(data[i]);
    }
};
void BleResponsePoolFreeAll() {
}
bool equalUuids(const char* ext, const char* known) {
    if (ext == NULL || *ext == 0) return false;
    if (*ext == '{') ext++;
    return _memicmp(ext, known, 36) == 0;
}

bool WINAPI DllMain(HINSTANCE hDll, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)     {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_PROCESS_DETACH:
        glbTerminate = true;
        ::SetEvent(glbWakeSteeringThread);
        if(glbSteeringThread.get())
            glbSteeringThread->join();
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    return true;
}
extern "C" {
    fptr_void_ptr orgProcessBLEResponse = nullptr;
    void* newCadenceData(const byte *cdata, BleResponsePool &pool) {
        char* data = pool.Allocate(0x68);
        memset(data, 0, 0x68);
        data[0] = 2;
        memcpy(data + 8, "12345678", 8);
        data[0x18] = 8; data[0x20] = 15;
        memcpy(data + 0x28, "Schwinn x70c", 13);
        data[0x38] = 13; data[0x40] = 15;
        char* serv = pool.Allocate(0x38);
        memset(serv, 0, 0x38);
        *(void**)(data + 0x50) = serv;
        *(void**)(data + 0x58) = serv + 0x38;
        *(void**)(data + 0x60) = serv + 0x38;
        memcpy(serv, "0x1816", 6);
        serv[0x10] = 6; serv[0x18] = 15;
        int numchars = 1;
        char* chars = pool.Allocate(0x30 * numchars);
        memset(chars, 0, 0x30 * numchars);
        *(void**)(serv + 0x20) = chars;
        *(void**)(serv + 0x28) = chars + 0x30 * numchars;
        *(void**)(serv + 0x30) = chars + 0x30 * numchars;
        memcpy(chars, "0x2a5b", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char *char_value = pool.Allocate(5);
        char_value[0] = 2; //flags
        //crank revolution #
        char_value[1] = cdata ? cdata[4] : 0; char_value[2] = cdata ? cdata[5] : 0;
        //time
        char_value[3] = cdata ? cdata[8] : 0; char_value[4] = cdata ? cdata[9] : 0;

        char tmp[100];
        sprintf(tmp, "crank revolution #=%u, time=%d\n", (unsigned)char_value[1] + (unsigned)char_value[2] * 256, ((unsigned)char_value[3] + (unsigned)char_value[4] * 256)/1024);
        OutputDebugString(tmp);
        
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 5;
        /*chars += 0x30;
        memcpy(chars, "0x2a5c", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char_value = pool.Allocate(2);
        char_value[0] = 2; char_value[1] = 0;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 2;
        chars += 0x30;
        memcpy(chars, "0x2a38", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char_value = pool.Allocate(1);
        char_value[0] = 12;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 1;*/
        return data;
    }
    void doCadence(const byte* cdata);
    void DumpBLEResponse(void* data) {
        char* byteData = (char*)data;
        char buf[512] = {}, * bptr = buf;
        uint64_t firstQword = *(uint64_t*)data;
        bptr += sprintf(bptr, "%016llX.%s %s\n", firstQword, byteData + 8, byteData + 0x28);
        char* service = *(char**)(byteData + 0x50), * end_service = *(char**)(byteData + 0x58);
        while (service < end_service) {
            char len = service[16];
            if (len > 16) {
                char* longServ = *(char**)service;
                if (equalUuids(longServ, SCHWINN_SERV)) {
                    memcpy(longServ, "0x1818", 7);
                    ///!!!service[16] = 6;
                    //byteData[0] = 2;
                }
                bptr += sprintf(bptr, "serv: %s\n", longServ);
            }
            else {
                bptr += sprintf(bptr, "serv: %s\n", service);
            }
            char* charact = *(char**)(service + 0x20), * end_charact = *(char**)(service + 0x28);
            while (charact < end_charact) {
                len = charact[16];
                bool needConversion = false;
                if (len > 16) {
                    char* longChar = *(char**)charact;
                    if (equalUuids(longChar, SCHWINN_CHAR)) {
                        memcpy(longChar, "0x2a63", 7);
                        ///!!!charact[16] = 6;
                        char *real_charact_val = *(char**)(charact + 0x20);
                        if (!real_charact_val) {
                            char* pushing_const_val = new_char_dll(4);
                            pushing_const_val[0] = pushing_const_val[1] = pushing_const_val[3] = 0;
                            pushing_const_val[2] = 6; // чтобы отличить
                            *(void**)(charact + 0x20) = pushing_const_val;
                            charact[0x28] = 4;
                        } else {
                            needConversion = true;
                        }
                    }
                    bptr += sprintf(bptr, "charact: %s ", longChar);
                }
                else {
                    bptr += sprintf(bptr, "charact: %s ", charact);
                    if (_memicmp(charact, SCHWINN_SHORT_CHAR, 10) == 0) {
                        memcpy(charact, "0x2a63", 7);
                        len = 6;
                        charact[16] = len;
                        needConversion = true;
                    }
                }
                char* charact_val = *(char**)(charact + 0x20);
                int charact_len = charact[0x28];
                for (int i = 0; i < charact_len; i++)
                    bptr += sprintf(bptr, "%02X ", (int)(unsigned char)charact_val[i]);
                if (needConversion) {
                    if (charact_len == 0x11 && charact_val[0] == 0x11 && charact_val[1] == 0x20) { //11 20 00 80 10 00 00 80 9C 21 F7 40 05 00 00 00 04 
                        bptr += sprintf(bptr, "-> ");
                        charact_len = 4;
                        charact[0x28] = charact_len;
#if 0
                        charact_val[0] = charact_val[1] = charact_val[3] = 0;
                        charact_val[2] = 7; // чтобы отличить
#else
                        static int64_t mLastCalories = 0;
                        static int mLastTime = 0; //в 1024-х долях секунды
                        static double mLastPower = -1.0;
                        byte* cdata = (byte*)charact_val;
                        static byte oldc = 0xFF;
                        if (oldc != cdata[4]) { //иначе zwift нам весь каденс занулит
                            doCadence(cdata);
                            oldc = cdata[4];
                        }
                        int64_t calories = cdata[10] | (cdata[11] << 8) | (cdata[12] << 16) | (cdata[13] << 24) | ((int64_t)cdata[14] << 32) | (int64_t(cdata[15] & 0x7F) << 40);
                        int tim = cdata[8] | (cdata[9] << 8);
                        if (mLastCalories == 0 || tim == mLastTime) {
                            mLastCalories = calories;
                            mLastTime = tim;
                        }
                        else {
                            int64_t dcalories = calories - mLastCalories;
                            mLastCalories = calories;
                            int dtime = tim - mLastTime;
                            mLastTime = tim;
                            if (dtime < 0) dtime += 65536;

                            int em_idx = (int)cdata[16] - 1;
                            if (em_idx < 0) em_idx = 0;
                            if (em_idx > 24) em_idx = 24;
                            //коэффициент нелинейности нагрузки 1-25 Schwinn
                            static const double extra_mult[] = { 0.60, 0.60, 0.60, 0.60, 0.60, 0.60, 0.60, 0.60, 0.60, 0.60, //1-10
                                0.60, 0.60, 0.60, //11-13
                                0.75, 0.91, 1.07, //14-16
                                1.23, 1.39, 1.55, //17-19
                                1.72, 1.88, 2.04, //20-22
                                2.20, 2.36, 2.52  //23-25
                            };
                            double mult = 0.42 * extra_mult[em_idx];
                            double power = (double)dcalories / (double)dtime * mult;
                            if (mLastPower == -1.0 || fabs(mLastPower - power) < 100.0)
                                mLastPower = power;
                            else
                                mLastPower += (power - mLastPower) / 2.0;
                            if (mLastPower < 0)
                                mLastPower = 1;
                            // flags
                            // 00000001 - 1   - 0x001 - Pedal Power Balance Present
                            // 00000010 - 2   - 0x002 - Pedal Power Balance Reference
                            // 00000100 - 4   - 0x004 - Accumulated Torque Present
                            // 00001000 - 8   - 0x008 - Accumulated Torque Source
                            // 00010000 - 16  - 0x010 - Wheel Revolution Data Present
                            // 00100000 - 32  - 0x020 - Crank Revolution Data Present
                            // 01000000 - 64  - 0x040 - Extreme Force Magnitudes Present
                            // 10000000 - 128 - 0x080 - Extreme Torque Magnitudes Present
                            int pwr = (int)(mLastPower + 0.5);
                            charact_val[0] = charact_val[1] = 0;
                            charact_val[2] = (byte)(pwr & 0xFF);
                            charact_val[3] = (byte)((pwr >> 8) & 0xFF);
                            char tmp[100];
                            sprintf(tmp, "mLastPower=%f\n", mLastPower);
                            OutputDebugString(tmp);
                            //Debug.WriteLine($"Time: {(int)(tim / 1024)}s, {(int)mLastPower}W, {calories >> 8}c, mult={mult}, {currentCadence()}rpm");
#endif
                        }

                        for (int i = 0; i < charact_len; i++)
                            bptr += sprintf(bptr, " %02X ", (int)(unsigned char)charact_val[i]);
                    }
                    else {
                        doCadence(NULL);
                    }
                }
                charact += 0x30;
            }
            service += 0x38;
        }
        bptr += sprintf(bptr, "\n");
        //OutputDebugStringA(buf);
    }
    void doCadence(const byte* cdata) {
        BleResponsePool pool;
        void* data = newCadenceData(cdata, pool);
        DumpBLEResponse(data);
        orgProcessBLEResponse(data);
    }
    float CalcNewSteer() {
        if (glbSteeringTask == 0.0) {
            if (glbSteeringCurrent > 0.0)
                glbSteeringCurrent -= min(10, glbSteeringCurrent);
            else
                glbSteeringCurrent += min(10, -glbSteeringCurrent);
        } else {
            glbSteeringCurrent = glbSteeringTask;
            glbSteeringTask = 0;
        }
        return glbSteeringCurrent;
    }
    void* newSteeringData(BleResponsePool &pool) {
        char* data = pool.Allocate(0x68);
        memset(data, 0, 0x68);
        data[0] = 2;
        memcpy(data + 8, "87654321", 8);
        data[0x18] = 8; data[0x20] = 15;
        memcpy(data + 0x28, "Edge Steer", 10);
        data[0x38] = 10; data[0x40] = 15;
        char* serv = pool.Allocate(0x38);
        memset(serv, 0, 0x38);
        *(void**)(data + 0x50) = serv;
        *(void**)(data + 0x58) = serv + 0x38;
        *(void**)(data + 0x60) = serv + 0x38;
        char* servLong = pool.Allocate(37);
        *(void**)(serv) = servLong;
        memcpy(servLong, "347b0001-7635-408b-8918-8ff3949ce592", 37);
        serv[0x10] = 36; serv[0x18] = 36;
        int numchars = 2;
        char* chars = pool.Allocate(0x30 * numchars);
        memset(chars, 0, 0x30 * numchars);
        *(void**)(serv + 0x20) = chars;
        *(void**)(serv + 0x28) = chars + 0x30 * numchars;
        *(void**)(serv + 0x30) = chars + 0x30 * numchars;
        char* charLong = pool.Allocate(37);
        *(void**)(chars) = charLong;
        memcpy(charLong, "347b0030-7635-408b-8918-8ff3949ce592", 37); //angle
        chars[0x10] = 36; chars[0x18] = 36;
        char *char_value = pool.Allocate(4);
        float steer = CalcNewSteer();
        memcpy(char_value, &steer, 4);
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 4;
        /*chars += 0x30;
        charLong = pool.Allocate(37);
        *(void**)(chars) = charLong;
        memcpy(charLong, "347b0031-7635-408b-8918-8ff3949ce592", 37); //rx - нужен ли?
        chars[0x10] = 36; chars[0x18] = 36;
        char_value = pool.Allocate(3);
        char_value[0] = 3; char_value[1] = 0x11; char_value[2] = -1;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 3;*/
        chars += 0x30;
        charLong = pool.Allocate(37);
        *(void**)(chars) = charLong;
        memcpy(charLong, "347b0032-7635-408b-8918-8ff3949ce592", 37); //tx
        chars[0x10] = 36; chars[0x18] = 36;
        //static int cnt = 0;
        //cnt++;
        /*if(cnt <= 3) {
            char_value = pool.Allocate(1);
            char_value[0] = -1;
            *(void**)(chars + 0x20) = char_value;
            chars[0x28] = 1;
        } else if (cnt <= 5) { //challenge
            char_value = pool.Allocate(4);
            char_value[0] = 3; char_value[1] = 0x10; char_value[2] = -1; char_value[3] = -1;
            *(void**)(chars + 0x20) = char_value;
            chars[0x28] = 4;
        } else {*/ //success
            char_value = pool.Allocate(3);
            char_value[0] = -1; char_value[1] = 0x13; char_value[2] = -1;
            *(void**)(chars + 0x20) = char_value;
            chars[0x28] = 3;
            //if (cnt > 10)
            //    cnt = 0;
        //}
        return data;
    }
    void ProcessBLEResponse(void* data) {
        if (orgProcessBLEResponse) {
            DumpBLEResponse(data);
            orgProcessBLEResponse(data);
            BleResponsePoolFreeAll();
        }
    }
    void ExecuteSteeringThread() {
        int counter = 0, counterMax = 20;
        OutputDebugString("Enter ExecuteSteeringThread");
        do {
            switch (WaitForSingleObject(glbWakeSteeringThread, 50)) {
            case WAIT_TIMEOUT: { //ничего не происходит
                    bool left = (GetKeyState(VK_LEFT) & 0xF000), right = (GetKeyState(VK_RIGHT) & 0xF000);
                    ++counter;
                    if (counter > counterMax || left || right) {
                        if (left && glbSteeringTask > -100) glbSteeringTask -= 10;
                        if (right && glbSteeringTask < 100) glbSteeringTask += 10;
                        counter = 0;
                    }
                    else {
                        continue;
                    }
                }
                break;
            default: 
                OutputDebugString("ExecuteSteeringThread. Failed wait and exit");
                return; //сбой в системе, уходим
            case WAIT_OBJECT_0: //разбудили - для выхода или работы?
                if (glbTerminate) {
                    OutputDebugString("ExecuteSteeringThread. Wake for exit");
                    return;
                }
                break;
                counterMax = 2;
            }
            if (orgProcessBLEResponse) {
                BleResponsePool pool;
                void* data = newSteeringData(pool);
                if (!glbTerminate) DumpBLEResponse(data);
                if (!glbTerminate) orgProcessBLEResponse(data);
                if (glbSteeringTask == 0.0 && glbSteeringCurrent == 0.0)
                    counterMax = 20;
            }
        } while (!glbTerminate);
        OutputDebugString("Exit ExecuteSteeringThread");
    }

    /*fptr_void_ptr orgOnPairCB = nullptr;
    void OnPairCB(void* data) {
        if (orgOnPairCB) {
            OutputDebugString("OnPairCB\n");
            orgOnPairCB(data);
        }
    }*/

    __declspec(dllexport) bool BLEPairToDevice(void *a1) {
        static fptr_bool_ptr real = (fptr_bool_ptr)GetProcAddress(org, "BLEPairToDevice");
        return real(a1);
    }
    __declspec(dllexport) void BLEInitBLEFlags() {
        static fptr_void_void real = (fptr_void_void)GetProcAddress(org, "BLEInitBLEFlags");
        real();
#ifdef ENABLE_EDGE_REMOTE
        const char* edge_id_str = getenv("ZWIFT_EDGE_REMOTE");
        if (edge_id_str && *edge_id_str) {
            glbWakeSteeringThread = ::CreateEvent(NULL, false, false, NULL);
            glbSteeringThread.reset(new std::thread(ExecuteSteeringThread));
        }
#endif
    }
    __declspec(dllexport) void BLEPurgeDeviceList() {
        static fptr_void_void real = (fptr_void_void)GetProcAddress(org, "BLEPurgeDeviceList");
        return real();
    }
    __declspec(dllexport) void BLESetAssertFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetAssertFunc");
        return real(a1);
    }
    __declspec(dllexport) void BLESetConnectionStatusCBFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetConnectionStatusCBFunc");
        return real(a1);
    }
    __declspec(dllexport) void BLESetEnableBLEIsAvailableFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetEnableBLEIsAvailableFunc");
        return real(a1);
    }
    __declspec(dllexport) void BLESetEnableSupportsBLEFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetEnableSupportsBLEFunc");
        return real(a1);
    }
    __declspec(dllexport) void BLESetLogFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetLogFunc");
        return real(a1);
    }
    __declspec(dllexport) void BLESetOnPairCB(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetOnPairCB");
        //orgOnPairCB = (fptr_void_ptr)a1;
        //a1 = OnPairCB;
        return real(a1);
    }
    __declspec(dllexport) void BLESetPeripheralDiscoveryFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetPeripheralDiscoveryFunc");
        return real(a1);
    }
    __declspec(dllexport) void BLESetProcessBLEResponse(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetProcessBLEResponse");
        orgProcessBLEResponse = (fptr_void_ptr)a1;
        a1 = ProcessBLEResponse;
        return real(a1);
    }
    __declspec(dllexport) void BLEStartScanning(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLEStartScanning");
        char* from = (char*)*(void**)a1;
        char* to = (char*)*((char**)a1+1);
        while (from < to) {
            char serv_length = from[0x10];
            if (serv_length <= 15 && memcmp(from, "0x1814", 7) == 0) { // беговую дорожку заменим на Schwinn
                char* schwinn_serv = new_char_zwift(37);
                memcpy(schwinn_serv, SCHWINN_SERV, 37);
                memcpy(from, &schwinn_serv, 8);
                from[0x10] = 36; from[0x18] = 36;
                char* schwinn_char = new_char_zwift(37);
                memcpy(schwinn_char, SCHWINN_CHAR, 37);
                from = (char*)*((char**)from + 4);
                memcpy(from, &schwinn_char, 8);
                from[0x10] = 36; from[0x18] = 36;
                break;
            }
            from += 0x38;
        }
        return real(a1);
    }
    __declspec(dllexport) void BLEStopScanning() {
        static fptr_void_void real = (fptr_void_void)GetProcAddress(org, "BLEStopScanning");
        return real();
    }
    __declspec(dllexport) bool BLEUnpairFromDevice(void *a1) {
        static fptr_bool_ptr real = (fptr_bool_ptr)GetProcAddress(org, "BLEUnpairFromDevice");
        return real(a1);
    }
    __declspec(dllexport) bool BLEWriteToDevice(void *a1, void *a2, void *a3) {
        static fptr_bool_ptr3 real = (fptr_bool_ptr3)GetProcAddress(org, "BLEWriteToDevice");
        return real(a1, a2, a3);
    }
}
