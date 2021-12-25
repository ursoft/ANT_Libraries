#include <Windows.h>

HMODULE org = LoadLibraryA("BleWin10Lib.org.dll");
typedef char (__fastcall *fptr_bool_ptr)(void *);
typedef char (__fastcall *fptr_bool_ptr3)(void *, void *, void *);
typedef void *(*fptr_ptr_void)();
typedef void (__fastcall *fptr_void_ptr)(void *);
typedef void *(__fastcall *fptr_ptr_ptr)(void *);
typedef void (*fptr_void_void)();
typedef void *(__fastcall *fptr_ptr_bool2)(bool, bool);

extern "C" {
    __declspec(dllexport) char __fastcall BLEPairToDevice(void *a1) {
        static fptr_bool_ptr real = (fptr_bool_ptr)GetProcAddress(org, "BLEPairToDevice");
        return real(a1);
    }
    __declspec(dllexport) void *BLEInitBLEFlags(void *a1) {
        static fptr_ptr_ptr real = (fptr_ptr_ptr)GetProcAddress(org, "BLEInitBLEFlags");
        return real(a1);
    }
    __declspec(dllexport) void * __fastcall BLEPurgeDeviceList(bool a1, bool a2) {
        static fptr_ptr_bool2 real = (fptr_ptr_bool2)GetProcAddress(org, "BLEPurgeDeviceList");
        return real(a1, a2);
    }
    __declspec(dllexport) void BLESetAssertFunc(void* a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetAssertFunc");
        return real(a1);
    }
    __declspec(dllexport) void __fastcall BLESetConnectionStatusCBFunc(void* a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetConnectionStatusCBFunc");
        return real(a1);
    }
    __declspec(dllexport) void __fastcall BLESetEnableBLEIsAvailableFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetEnableBLEIsAvailableFunc");
        return real(a1);
    }
    __declspec(dllexport) void __fastcall BLESetEnableSupportsBLEFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetEnableSupportsBLEFunc");
        return real(a1);
    }
    __declspec(dllexport) void __fastcall BLESetLogFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetLogFunc");
        return real(a1);
    }
    __declspec(dllexport) void __fastcall BLESetOnPairCB(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetOnPairCB");
        return real(a1);
    }
    __declspec(dllexport) void __fastcall BLESetPeripheralDiscoveryFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetPeripheralDiscoveryFunc");
        return real(a1);
    }
    __declspec(dllexport) void __fastcall BLESetProcessBLEResponse(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetProcessBLEResponse");
        return real(a1);
    }
    __declspec(dllexport) void *__fastcall BLEStartScanning(void *a1) {
        static fptr_ptr_ptr real = (fptr_ptr_ptr)GetProcAddress(org, "BLEStartScanning");
        return real(a1);
    }
    __declspec(dllexport) void *BLEStopScanning() {
        static fptr_ptr_void real = (fptr_ptr_void)GetProcAddress(org, "BLEStopScanning");
        return real();
    }
    __declspec(dllexport) char __fastcall BLEUnpairFromDevice(void *a1) {
        static fptr_bool_ptr real = (fptr_bool_ptr)GetProcAddress(org, "BLEUnpairFromDevice");
        return real(a1);
    }
    __declspec(dllexport) char __fastcall BLEWriteToDevice(void *a1, void *a2, void *a3) {
        static fptr_bool_ptr3 real = (fptr_bool_ptr3)GetProcAddress(org, "BLEWriteToDevice");
        return real(a1, a2, a3);
    }
}
