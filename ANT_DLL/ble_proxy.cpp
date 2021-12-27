#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <thread>
#include <cmath>
#include "defines.h"

HMODULE org = LoadLibraryA("BleWin10Lib.org.dll");
typedef bool (*fptr_bool_ptr)(void *);
typedef bool (*fptr_bool_ptr3)(void *, void *, void *);
typedef void (*fptr_void_ptr)(void *);
typedef void (*fptr_void_void)();

std::unique_ptr<std::thread> glbSteeringThread;
HANDLE glbWakeSteeringThread = INVALID_HANDLE_VALUE;
float glbSteeringTask = 0.0, glbSteeringCurrent = 0.0;
bool glbTerminate = false;

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
    void* newCadenceData() {
        char* data = new char[0x68];
        memset(data, 0, 0x68);
        data[0] = 2;
        memcpy(data + 8, "12345678", 8);
        data[0x18] = 8; data[0x20] = 15;
        memcpy(data + 0x28, "Schwinn x70c", 13);
        data[0x38] = 13; data[0x40] = 15;
        char* serv = new char[0x38];
        memset(serv, 0, 0x38);
        *(void**)(data + 0x50) = serv;
        *(void**)(data + 0x58) = serv + 0x38;
        *(void**)(data + 0x60) = serv + 0x38;
        memcpy(serv, "0x1816", 6);
        serv[0x10] = 6; serv[0x18] = 15;
        int numchars = 1;
        char* chars = new char[0x30 * numchars];
        memset(chars, 0, 0x30 * numchars);
        *(void**)(serv + 0x20) = chars;
        *(void**)(serv + 0x28) = chars + 0x30 * numchars;
        *(void**)(serv + 0x30) = chars + 0x30 * numchars;
        memcpy(chars, "0x2a5b", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char *char_value = new char[5];
        char_value[0] = 2; 
        char_value[1] = (GetTickCount()/3) % 0xff;
        char_value[2] = (GetTickCount() / 3 / 256) % 0xff;
        char_value[3] = GetTickCount() % 0xff;
        char_value[4] = (GetTickCount() / 256) % 0xff;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 5;
        /*chars += 0x30;
        memcpy(chars, "0x2a5c", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char_value = new char[2];
        char_value[0] = 2; char_value[1] = 0;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 2;
        chars += 0x30;
        memcpy(chars, "0x2a38", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char_value = new char[1];
        char_value[0] = 12;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 1;*/
        return data;
    }
    void* newPowerData() {
        char* data = new char[0x68];
        memset(data, 0, 0x68);
        data[0] = 2;
        memcpy(data + 8, "12345678", 8);
        data[0x18] = 8; data[0x20] = 15;
        memcpy(data + 0x28, "Schwinn x70p", 13);
        data[0x38] = 13; data[0x40] = 15;
        char* serv = new char[0x38];
        memset(serv, 0, 0x38);
        *(void**)(data + 0x50) = serv;
        *(void**)(data + 0x58) = serv + 0x38;
        *(void**)(data + 0x60) = serv + 0x38;
        memcpy(serv, "0x1818", 6);
        serv[0x10] = 6; serv[0x18] = 15;
        int numchars = 1;
        char* chars = new char[0x30 * numchars];
        memset(chars, 0, 0x30 * numchars);
        *(void**)(serv + 0x20) = chars;
        *(void**)(serv + 0x28) = chars + 0x30 * numchars;
        *(void**)(serv + 0x30) = chars + 0x30 * numchars;
        memcpy(chars, "0x2a63", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char *char_value = new char[4];
        char_value[0] = char_value[1] = char_value[3] = 0;
        char_value[2] = char(byte(200 + GetTickCount() % 50));
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 4;
        /*chars += 0x30;
        memcpy(chars, "0x2a65", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char_value = new char[4];
        char_value[0] = char_value[1] = char_value[3] = 0;
        char_value[2] = 1;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 4;
        chars += 0x30;
        memcpy(chars, "0x2a38", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char_value = new char[1];
        char_value[0] = 12;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 1;*/
        return data;
    }
    void DumpBLEResponse(void* data) {
        char* byteData = (char*)data;
        char buf[512] = {}, * bptr = buf;
        uint64_t firstQword = *(uint64_t*)data;
        bptr += sprintf(bptr, "%016llX.%s %s\n", firstQword, byteData + 8, byteData + 0x28);
        char* service = *(char**)(byteData + 0x50), * end_service = *(char**)(byteData + 0x58);
        while (service < end_service) {
            char len = service[16];
            if (len > 16) {
                bptr += sprintf(bptr, "serv: %s\n", *(char**)service);
            }
            else {
                bptr += sprintf(bptr, "serv: %s\n", service);
            }
            char* charact = *(char**)(service + 0x20), * end_charact = *(char**)(service + 0x28);
            while (charact < end_charact) {
                len = charact[16];
                if (len > 16) {
                    bptr += sprintf(bptr, "charact: %s ", *(char**)charact);
                }
                else {
                    bptr += sprintf(bptr, "charact: %s ", charact);
                }
                char* charact_val = *(char**)(charact + 0x20);
                int charact_len = charact[0x28];
                for (int i = 0; i < charact_len; i++)
                    bptr += sprintf(bptr, "%02X ", (int)(unsigned char)charact_val[i]);
                charact += 0x30;
            }
            service += 0x38;
        }
        bptr += sprintf(bptr, "\n");
        OutputDebugStringA(buf);
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
    void* newSteeringData() {
        char* data = new char[0x68];
        memset(data, 0, 0x68);
        data[0] = 2;
        memcpy(data + 8, "87654321", 8);
        data[0x18] = 8; data[0x20] = 15;
        memcpy(data + 0x28, "Edge Steer", 10);
        data[0x38] = 10; data[0x40] = 15;
        char* serv = new char[0x38];
        memset(serv, 0, 0x38);
        *(void**)(data + 0x50) = serv;
        *(void**)(data + 0x58) = serv + 0x38;
        *(void**)(data + 0x60) = serv + 0x38;
        char* servLong = new char[37];
        *(void**)(serv) = servLong;
        memcpy(servLong, "347b0001-7635-408b-8918-8ff3949ce592", 37);
        serv[0x10] = 36; serv[0x18] = 36;
        int numchars = 2;
        char* chars = new char[0x30 * numchars];
        memset(chars, 0, 0x30 * numchars);
        *(void**)(serv + 0x20) = chars;
        *(void**)(serv + 0x28) = chars + 0x30 * numchars;
        *(void**)(serv + 0x30) = chars + 0x30 * numchars;
        char* charLong = new char[37];
        *(void**)(chars) = charLong;
        memcpy(charLong, "347b0030-7635-408b-8918-8ff3949ce592", 37); //angle
        chars[0x10] = 36; chars[0x18] = 36;
        char *char_value = new char[4];
        float steer = CalcNewSteer();
        memcpy(char_value, &steer, 4);
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 4;
        /*chars += 0x30;
        charLong = new char[37];
        *(void**)(chars) = charLong;
        memcpy(charLong, "347b0031-7635-408b-8918-8ff3949ce592", 37); //rx - нужен ли?
        chars[0x10] = 36; chars[0x18] = 36;
        char_value = new char[3];
        char_value[0] = 3; char_value[1] = 0x11; char_value[2] = -1;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 3;*/
        chars += 0x30;
        charLong = new char[37];
        *(void**)(chars) = charLong;
        memcpy(charLong, "347b0032-7635-408b-8918-8ff3949ce592", 37); //tx
        chars[0x10] = 36; chars[0x18] = 36;
        //static int cnt = 0;
        //cnt++;
        /*if(cnt <= 3) {
            char_value = new char[1];
            char_value[0] = -1;
            *(void**)(chars + 0x20) = char_value;
            chars[0x28] = 1;
        } else if (cnt <= 5) { //challenge
            char_value = new char[4];
            char_value[0] = 3; char_value[1] = 0x10; char_value[2] = -1; char_value[3] = -1;
            *(void**)(chars + 0x20) = char_value;
            chars[0x28] = 4;
        } else {*/ //success
            char_value = new char[3];
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
            orgProcessBLEResponse(data);
            DumpBLEResponse(data);
            data = newPowerData();
            DumpBLEResponse(data);
            orgProcessBLEResponse(data);
            data = newCadenceData();
            DumpBLEResponse(data);
            orgProcessBLEResponse(data);
            //data = newSteeringData();
            //DumpBLEResponse(data);
            //orgProcessBLEResponse(data);
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
                void* data = newSteeringData();
                if (!glbTerminate) DumpBLEResponse(data);
                if (!glbTerminate) orgProcessBLEResponse(data);
                if (glbSteeringTask == 0.0 && glbSteeringCurrent == 0.0)
                    counterMax = 20;
            }
        } while (!glbTerminate);
        OutputDebugString("Exit ExecuteSteeringThread");
    }

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
