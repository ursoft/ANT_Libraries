#include <Windows.h>
#include <cstdint>
#include <cstdio>

HMODULE org = LoadLibraryA("BleWin10Lib.org.dll");
typedef bool (*fptr_bool_ptr)(void *);
typedef bool (*fptr_bool_ptr3)(void *, void *, void *);
typedef void (*fptr_void_ptr)(void *);
typedef void (*fptr_void_void)();

extern "C" {
    fptr_void_ptr orgProcessBLEResponse = nullptr;
    void* newPowerData() {
        char* data = new char[0x68];
        memset(data, 0, 0x68);
        data[0] = 2;
        memcpy(data + 8, "12345678", 8);
        data[0x18] = 8; data[0x20] = 15;
        memcpy(data + 0x28, "SCHWINN 170/270", 15);
        data[0x38] = 15; data[0x40] = 15;
        char* serv = new char[0x38];
        memset(serv, 0, 0x38);
        *(void**)(data + 0x50) = serv;
        *(void**)(data + 0x58) = serv + 0x38;
        *(void**)(data + 0x60) = serv + 0x38;
        memcpy(serv, "0x1818", 6);
        serv[0x10] = 6; serv[0x18] = 15;
        char* chars = new char[0x30 * 3];
        memset(chars, 0, 0x30 * 3);
        *(void**)(serv + 0x20) = chars;
        *(void**)(serv + 0x28) = chars + 0x30 * 3;
        *(void**)(serv + 0x30) = chars + 0x30 * 3;
        memcpy(chars, "0x2a63", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char *char_value = new char[4];
        char_value[0] = char_value[1] = char_value[3] = 0;
        char_value[2] = 127;
        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 4;
        chars += 0x30;
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
        chars[0x28] = 1;
        return data;
    }
    void DumpBLEResponse(void* data) {
        char* byteData = (char*)data;
        char buf[256] = {}, * bptr = buf;
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
                    bptr += sprintf(bptr, "%02X ", charact_val[i]);
                //if (charact_len == 2) charact_val[1] -= 50; //patch HR:OK
                charact += 0x30;
            }
            service += 0x38;
        }
        bptr += sprintf(bptr, "\n");
        OutputDebugStringA(buf);
    }
    void ProcessBLEResponse(void* data) {
        if (orgProcessBLEResponse) {
            orgProcessBLEResponse(data);
            DumpBLEResponse(data);
            data = newPowerData();
            DumpBLEResponse(data);
            orgProcessBLEResponse(data);
        }
    }

    __declspec(dllexport) bool BLEPairToDevice(void *a1) {
        static fptr_bool_ptr real = (fptr_bool_ptr)GetProcAddress(org, "BLEPairToDevice");
        return real(a1);
    }
    __declspec(dllexport) void BLEInitBLEFlags() {
        static fptr_void_void real = (fptr_void_void)GetProcAddress(org, "BLEInitBLEFlags");
        return real();
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
