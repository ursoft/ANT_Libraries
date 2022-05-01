#include "defines.h"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>
#include <list>
#include <cmath>
#include <sysinfoapi.h>
#include <bluetoothleapis.h>
#include <BthLEDef.h>
#include <SetupAPI.h>

bool glbNoSchwinn = true;
const char *SCHWINN_SERV = "3bf58980-3a2f-11e6-9011-0002a5d5c51b";
GUID GUID_BLUETOOTHLE_SCHWINN_SERV = { 0x3bf58980, 0x3a2f, 0x11e6, {0x90, 0x11, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b} };
const char *SCHWINN_CHAR_EVENT = "5c7d82a0-9803-11e3-8a6c-0002a5d5c51b";
const char *SCHWINN_SHORT_CHAR_EVENT = "0x5c7d82a0";
const char *SCHWINN_CHAR_SRD0 = "6be8f580-9803-11e3-ab03-0002a5d5c51b";
const char *SCHWINN_SHORT_CHAR_SRD0 = "0x6be8f580";

HMODULE org = LoadLibraryA("BleWin10Lib.org.dll");
typedef bool (*fptr_bool_ptr)(void *);
typedef bool (*fptr_bool_ptr3)(void *, void *, void *);
typedef void (*fptr_void_ptr)(void *);
typedef void (*fptr_void_void)();

std::unique_ptr<std::thread> glbSteeringThread;
HANDLE glbWakeSteeringThread = INVALID_HANDLE_VALUE;

bool glbTerminate = false;
FILE* glbSchwinnDumpFile = NULL;

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

const int KEY_MULT = 5; // многократность нажатия клавиши, которую мы умеем распознавать
volatile float glbSteeringTask = 0.0,         // текущее задание
      glbSteeringCurrent = 0.0,      // выданное только что значение (после чего задание зануляется и включается алгоритм поэтапного уведения в 0)
      glbSteeringAutoNullStep = 5,   // на сколько за один раз приблизить к нулю (не более)
      glbSteeringRange = 50,         // ограничение диапазона
      glbSteeringDelta[2][KEY_MULT] = // массивы шагов по кратности - сперва для remote, потом для keyboard
        { { 15, 20, 15, 20, 30 }, { 0.25, 1, 5, 10, 20 } };
DWORD glbLastSteeringKey = -1, // какую кнопку нажали в прошлый раз
      glbKeyRepeat = 0;        // номер повтора этой кнопки
volatile LONGLONG glbSteeringLastTime = 0, // время последнего нажатия на кнопку
         glbMaxBetweenKeys[2] = { 1000, 500 }; // максимальные времена между считающимися серийными нажатиями (тоже зависит от способностей клавиатуры)
void OnSteeringKeyPress(DWORD key, bool bFastKeyboard) {
    static LONGLONG start = GetTickCount64();
    LONGLONG now = GetTickCount64(), msFromLastKeyPress = now - glbSteeringLastTime;
    glbSteeringLastTime = now;
    if (key == glbLastSteeringKey) {
        if (msFromLastKeyPress <= glbMaxBetweenKeys[bFastKeyboard ? 1 : 0]) {
            if (glbKeyRepeat < KEY_MULT - 1) glbKeyRepeat++;
        } else {
            glbKeyRepeat = 0;
        }
    } else {
        glbLastSteeringKey = key;
        glbKeyRepeat = 0;
        if (glbSteeringTask != 0 || glbSteeringCurrent != 0) {
            glbSteeringCurrent = glbSteeringTask = 0;
            return;
        }
    }
    glbSteeringTask += glbSteeringDelta[bFastKeyboard ? 1 : 0][glbKeyRepeat] * ((key == VK_LEFT) ? -1 : 1);
    if (glbSteeringTask >= glbSteeringRange) glbSteeringTask = glbSteeringRange;
    else if (glbSteeringTask <= -glbSteeringRange) glbSteeringTask = -glbSteeringRange;

    /*char buf[1000];
    sprintf(buf, "STEER: t=%05d dt=%05d %s.%d S=%f\n", (int)(now - start), (int)msFromLastKeyPress, (key == VK_LEFT) ? "LEFT" : "RIGHT", glbKeyRepeat, glbSteeringTask);
    OutputDebugStringA(buf);*/
}
float CalcNewSteer() {
    if (glbSteeringTask == 0.0) {
        if (glbSteeringCurrent > 0.0)
            glbSteeringCurrent -= min(glbSteeringAutoNullStep, glbSteeringCurrent);
        else
            glbSteeringCurrent += min(glbSteeringAutoNullStep, -glbSteeringCurrent);
    } else {
        glbSteeringCurrent = glbSteeringTask;
        glbSteeringTask = 0;
    }
    return glbSteeringCurrent;
}

void PatchMainModule(const char* name, // "Trial.01",
    size_t nBytes, //10,
    const char* from, //"\x04\x0\x0\xf\xb6\x17\x84\xd2\x78\x07",
    const char* to, //"\x04\x0\x0\xf\xb6\x17\xb2\x02\x78\x07"
    size_t add = 0
) {
    char buf[1024];
    HMODULE hMain = GetModuleHandle(NULL);
    const void *base = (const void *)GetProcAddress(hMain, "AkTlsSetValue");
    DWORD oldProtect;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (0 == VirtualQuery(base, &mbi, sizeof(mbi))) {
        sprintf(buf, "ZWIFT_PATCH.%s VirtualQuery failed\n", name);
        OutputDebugString(buf);
        ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
        return;
    }
    MEMORY_BASIC_INFORMATION mbi2 = {};
    while(VirtualQuery((const uint8_t *)mbi.BaseAddress - 1, &mbi2, sizeof(mbi2))) {
        if (mbi2.RegionSize < mbi.RegionSize)
            break;
        else
            mbi = mbi2;
    }
    SIZE_T textLength = mbi.RegionSize + add;
    uint8_t *foundAt = NULL, *curPtr = (uint8_t*)mbi.BaseAddress;
    for (size_t i = 0; i < textLength - nBytes; i++) {
        if (0 == memcmp(curPtr + i, from, nBytes)) {
            if (foundAt == NULL) {
                foundAt = curPtr + i;
            } else {
                sprintf(buf, "ZWIFT_PATCH.%s multifound in [%p, %d]\n", name, mbi.BaseAddress, (int)textLength);
                OutputDebugString(buf);
                ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
                return;
            }
        }
    }
    if (foundAt) {
        sprintf(buf, "ZWIFT_PATCH.%s found in [%p, %d] at %p\n", name, mbi.BaseAddress, (int)textLength, foundAt);
        OutputDebugString(buf);
        VirtualProtectEx(GetCurrentProcess(), mbi.BaseAddress, textLength, PAGE_EXECUTE_READWRITE, &oldProtect);
        BOOL ok = WriteProcessMemory(GetCurrentProcess(), foundAt, to, nBytes, NULL);
        VirtualProtectEx(GetCurrentProcess(), mbi.BaseAddress, textLength, oldProtect, &oldProtect);
        sprintf(buf, "ZWIFT_PATCH.%s WriteProcessMemory=%d\n", name, ok);
        OutputDebugString(buf);
        if(!ok) ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
    } else {
        sprintf(buf, "ZWIFT_PATCH.%s not found in [%p, %d]\n", name, mbi.BaseAddress, (int)textLength);
        OutputDebugString(buf);
        ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
    }
}

int ReplaceFloat(float *fptr, DWORD *offset, bool back, float repl)
{
    float* new_fptr = fptr;
    DWORD new_offset = *offset;
    for (int i = 0; i < 16384; i++) {
        if (back) {
            new_fptr--;
            new_offset -= 4;
        }
        else {
            new_fptr++;
            new_offset += 4;
        }
        if (IsBadReadPtr(new_fptr, 4))
            break;
        if (fabs(*new_fptr - repl) <= 1) {
            char buf[1024];
            sprintf(buf, "ZWIFT_PATCH: [%p]: %x -> %x (%f -> %f aeq %f)\n", offset, *offset, new_offset, *fptr, *new_fptr, repl);
            OutputDebugString(buf);
            *offset = new_offset;
            return 1;
        }
    }
    return 0;
}

int TryFloatPatch(uint8_t* foundAt, float search1, float repl1, float search2, float repl2) {
    int ret = 0;
    for (int back = 4; back < 32 && ret < 2; back++) {
        uint8_t* tryFrom = foundAt - back;
        if (tryFrom[0] == 0xF3 && tryFrom[1] == 0x0f) {
            DWORD* offset = (DWORD *)tryFrom + 1;
            float *fptr = (float *)(tryFrom + 8 + *offset);
            if (tryFrom[2] == 0x10 && tryFrom[3] == 0x1d) {
                if (IsBadReadPtr(fptr, 4)) continue;
                if (search1 == *fptr) {
                    ret += ReplaceFloat(fptr, offset, search1 > repl1, repl1);
                }
            }
            else if (tryFrom[2] == 0x59 && tryFrom[3] == 0x05) {
                if (IsBadReadPtr(fptr, 4)) continue;
                if (search2 == *(float*)fptr) {
                    ret += ReplaceFloat(fptr, offset, search2 > repl2, repl2);
                }
            }
        }
    }
    return ret;
}

void PatchMainModuleNeo(const char* name,
    float search1, float repl1, //100->80
    float search2, float repl2) { //20->60
    char buf[1024];
    HMODULE hMain = GetModuleHandle(NULL);
    const void *base = (const void *)GetProcAddress(hMain, "AkTlsSetValue");
    DWORD oldProtect;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (0 == VirtualQuery(base, &mbi, sizeof(mbi))) {
        sprintf(buf, "ZWIFT_PATCH.%s VirtualQuery failed\n", name);
        OutputDebugString(buf);
        ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
        return;
    }
    MEMORY_BASIC_INFORMATION mbi2 = {};
    while(VirtualQuery((const uint8_t *)mbi.BaseAddress - 1, &mbi2, sizeof(mbi2))) {
        if (mbi2.RegionSize < mbi.RegionSize)
            break;
        else
            mbi = mbi2;
    }
    SIZE_T textLength = mbi.RegionSize;
    const byte marker[] = { 
        0x0f, 0x28, 0xf3,       // movaps xmm6, xmm3
        0xf3, 0x0f, 0x5c, 0xf0, // subss xmm6, xmm0
        0xf3, 0x0f, 0x10, 0x05  // movss xmm0, ?
    };
    uint8_t *curPtr = (uint8_t*)mbi.BaseAddress, *foundAt2 = NULL, *foundAt1 = NULL;
    for (size_t i = 48; i < textLength; i++) {
        if (0 == memcmp(curPtr + i, marker, sizeof(marker))) {
            sprintf(buf, "ZWIFT_PATCH.%s marker found at offset 0x%X\n", name, 
                unsigned(curPtr + i + 0x400 - (uint8_t*)mbi.BaseAddress));
            OutputDebugString(buf);
            switch (*(curPtr + i - 0x15)) {
            case 0xF3:
                if (foundAt2 == NULL) {
                    foundAt2 = curPtr + i;
                    break;
                }
                sprintf(buf, "ZWIFT_PATCH.%s 0xF3 multifound in [%p, %d]\n", name, mbi.BaseAddress, (int)textLength);
                OutputDebugString(buf);
                ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
                return;
            case 0x41:
                if (foundAt1 == NULL) {
                    foundAt1 = curPtr + i;
                    break;
                }
                sprintf(buf, "ZWIFT_PATCH.%s 0x41 multifound in [%p, %d]\n", name, mbi.BaseAddress, (int)textLength);
                OutputDebugString(buf);
                ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
                return;
            default:
                sprintf(buf, "ZWIFT_PATCH.%s unkfound in [%p, %d]\n", name, mbi.BaseAddress, (int)textLength);
                OutputDebugString(buf);
                ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
                return;
            }
        }
    }
    if (foundAt1 && foundAt2) {
        sprintf(buf, "ZWIFT_PATCH.%s found in [%p, %d] at %p && %p\n", name, mbi.BaseAddress, (int)textLength, foundAt1, foundAt2);
        OutputDebugString(buf);
        VirtualProtectEx(GetCurrentProcess(), mbi.BaseAddress, textLength, PAGE_EXECUTE_READWRITE, &oldProtect);
        int fp_cnt = TryFloatPatch(foundAt1, search1, repl1, search2, repl2);
        sprintf(buf, "ZWIFT_PATCH.%s TryFloatPatch1=%d/2\n", name, fp_cnt);
        OutputDebugString(buf);
        if(fp_cnt != 2) ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
        fp_cnt = TryFloatPatch(foundAt2, search1, repl1, search2, repl2);
        sprintf(buf, "ZWIFT_PATCH.%s TryFloatPatch2=%d/2\n", name, fp_cnt);
        OutputDebugString(buf);
        if (fp_cnt != 2) ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
        VirtualProtectEx(GetCurrentProcess(), mbi.BaseAddress, textLength, oldProtect, &oldProtect);
    } else {
        sprintf(buf, "ZWIFT_PATCH.%s both not found in [%p, %d]\n", name, mbi.BaseAddress, (int)textLength);
        OutputDebugString(buf);
        ::MessageBoxA(NULL, buf, "Zwift", MB_ICONERROR);
    }
}
INT_PTR OnAttach() {
    const char* patches_str = getenv("ZWIFT_PATCHES");
    if (patches_str == NULL || *patches_str == '1') {
        PatchMainModule(
            "Trial.01", 10,
            "\x04\x0\x0\xf\xb6\x17\x84\xd2\x78\x07",
            "\x04\x0\x0\xf\xb6\x17\xb2\x02\x78\x07"
        );
        PatchMainModule(
            "Trial.02", 8,
            "\x48\x8d\x4d\x9f\x41\x83\xcf\x08",
            "\x48\x8d\x4d\x9f\x90\x90\x90\x90"
        );
        PatchMainModule(
            "Trial.03", 8,
            "\x48\x8d\x4d\x9f\x41\x83\xcf\x10",
            "\x48\x8d\x4d\x9f\x90\x90\x90\x90"
        );
        PatchMainModule(
            "Trial.04", 10,
            "\xbb\x01\x0\x0\x0\xe9\x80\x0\x0\x0",
            "\xbb\x03\x0\x0\x0\xe9\x80\x0\x0\x0"
        );
        PatchMainModuleNeo("Neo80,100->20,80", 100.0, 80.0, 20.0, 60.0);
        PatchMainModule(
            "OldHome", 24,
            "game_1_17_noesis_enabled",
            "game_1_17_noesis!enabled",
            4096 * 1132 /* rdata segment length */
        );
    }
    return 0;
}
FARPROC ptr = OnAttach;
bool WINAPI DllMain(HINSTANCE hDll, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)     {
    case DLL_PROCESS_ATTACH:
        if(ptr) (*ptr)();
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
double CalcPowerNewAlgo(double cad, int resist) {
    static const double coeffs[25][3] = { //from Excel trends
        {-0.0014, 0.7920, -13.985 },
        {-0.0004, 0.9991, -19.051 },
        { 0.0015, 0.9179, -13.745 },
        { 0.0040, 0.9857, -13.095 },
        { 0.0027, 1.3958, -22.741 },
        { 0.0057, 1.1586, -15.126 },
        {-0.0013, 2.4666, -49.052 },
        { 0.0002, 2.6349, -52.390 },
        { 0.0034, 2.6240, -48.072 },
        { 0.0147, 1.6372, -19.653 },
        { 0.0062, 2.5851, -43.254 },
        { 0.0064, 3.2864, -59.336 },
        { 0.0048, 3.6734, -69.245 },
        { 0.0184, 2.1842, -28.936 },
        { 0.0052, 4.3939, -78.603 },
        { 0.0094, 3.8871, -65.982 },
        { 0.0165, 3.3074, -49.906 },
        { 0.0251, 3.2956, -44.436 },
        { 0.0281, 2.9107, -38.767 },
        { 0.0311, 2.9435, -35.851 },
        { 0.0141, 5.5646, -88.686 },
        { 0.0517, 1.8361, -13.777 },
        { 0.0467, 2.9273, -35.908 },
        { 0.0429, 4.1821, -50.141 },
        { 0.0652, 3.6670, -46.863 }
    };
    int idx = (resist - 1) % 25;
    double ret = coeffs[idx][0] * cad * cad + coeffs[idx][1] * cad + coeffs[idx][2];
    return ret > 0.0 ? ret : 0.0;
}

extern "C" {
    fptr_void_ptr orgProcessBLEResponse = nullptr;
    void* newCadenceData(const byte *cdata, BleResponsePool &pool) {
        //время может прийти совсем нехорошее, а потом восстановиться - не будем-ка доверять ему, да еще и усредним
        //char_value[3] = cdata ? cdata[8] : 0; char_value[4] = cdata ? cdata[9] : 0;
        const int avg_count = 5; //в секундах
        static double cranks[avg_count] = {};
        static DWORD times[avg_count], last_crank_out = 0, last_time = 0;
        DWORD tick = DWORD((uint64_t)GetTickCount() * 1024 / 1000);
        static DWORD idx = 0;
        if (cdata != NULL) {
            double cur_crank = cdata[3] / 256.0 + cdata[4] + cdata[5] * 256;
            cranks[idx % avg_count] = cur_crank;
            times[idx % avg_count] = tick;
            idx++;
            if (idx < avg_count) return nullptr;
            double total_cranks = cur_crank - cranks[idx % avg_count];
            DWORD total_ticks = tick - times[idx % avg_count];
            if (total_cranks != 0) {
                last_time += DWORD(total_ticks / total_cranks + 0.5);
                last_crank_out++;
            }
        } else {
            //зануляемся, выдавая тот же crank и время
        }
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
        char_value[1] = (char)(byte)last_crank_out; char_value[2] = (char)(byte)(last_crank_out >> 8);
        //time
        char_value[3] = (char)(byte)last_time; char_value[4] = (char)(byte)(last_time >> 8);

        /*if (cdata) {
            char tmp[100];
            sprintf(tmp, "crank revolution #=%.3f, time=%.1f\n", cdata[3]/256.0 + (byte)char_value[1] + (byte)char_value[2] * 256, ((byte)char_value[3] + (byte)char_value[4] * 256) / 1024.0);
            OutputDebugString(tmp);
        }
        else {
            OutputDebugString("crank revolution reset\n");
        }*/
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
    static char glbLastSchwinnPseudoHandle[32] = {};
    static double mLastPower = -1.0;
    static DWORD glbLastSchwinnPowerTick = GetTickCount(), glbStartTicks = glbLastSchwinnPowerTick, glbLastSchwinnPowerIncome = glbLastSchwinnPowerTick;
    bool ParseBLEResponse(void* data) {
        int useful_chunks = 0;
        char* byteData = (char*)data;
        char buf[512] = {}, * bptr = buf;
        uint64_t firstQword = *(uint64_t*)data;
        bptr += sprintf(bptr, "%016llX.%s %s\n", firstQword, byteData + 8, byteData + 0x28);
        bool bJoystick = !memcmp(byteData + 0x28, "Schwinn", 7);
        char* service = *(char**)(byteData + 0x50), * end_service = *(char**)(byteData + 0x58);
        while (service < end_service) {
            char len = service[16];
            char *longServ = NULL;
            if (len > 16) {
                longServ = *(char**)service;
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
            static volatile int subst_power = 7; //можно поменять в отладчике
            while (charact < end_charact) {
                len = charact[16];
                bool needPowerConversion = false, needHrConversion = false;
                if (len > 16) {
                    char* longChar = *(char**)charact;
                    bptr += sprintf(bptr, "charact: %s ", longChar);
                    if (equalUuids(longChar, SCHWINN_CHAR_EVENT)) {
                        memcpy(longChar, "0x2a63", 7);
                        ///!!!charact[16] = 6;
                        char* real_charact_val = *(char**)(charact + 0x20);
                        if (!real_charact_val) {
                            char* pushing_const_val = new_char_dll(4);
                            pushing_const_val[0] = pushing_const_val[1] = 0;
                            pushing_const_val[3] = subst_power / 256;
                            pushing_const_val[2] = subst_power % 256; // чтобы отличить
                            *(void**)(charact + 0x20) = pushing_const_val;
                            charact[0x28] = 4;
                            doCadence(NULL);
                        }
                        else {
                            needPowerConversion = true;
                        }
                    } else if (longServ && equalUuids(longChar, SCHWINN_CHAR_SRD0)) {
                        memcpy(longServ, "0x180D", 7); //Heart Rate Service
                        memcpy(longChar, "0x2A37", 7); //Heart Rate Measurement
                        ///!!!charact[16] = 6;
                        char* real_charact_val = *(char**)(charact + 0x20);
                        if (!real_charact_val) {
                            char* pushing_const_val = new_char_dll(2);
                            pushing_const_val[0] = 0;   //flags
                            pushing_const_val[1] = 123; //чтобы отличить
                            *(void**)(charact + 0x20) = pushing_const_val;
                            charact[0x28] = 2;
                            strncpy_s(glbLastSchwinnPseudoHandle, sizeof(glbLastSchwinnPseudoHandle), byteData + 8, sizeof(glbLastSchwinnPseudoHandle));
                        }
                        else {
                            needHrConversion = true;
                        }
                    }
                }
                else {
                    bptr += sprintf(bptr, "charact: %s ", charact);
                    if (_memicmp(charact, SCHWINN_SHORT_CHAR_EVENT, 10) == 0) {
                        memcpy(charact, "0x2a63", 7);
                        len = 6;
                        charact[16] = len;
                        needPowerConversion = true;
                    } else if (_memicmp(charact, SCHWINN_SHORT_CHAR_SRD0, 10) == 0) {
                        memcpy(charact, "0x2A37", 7); //heart rate service
                        len = 6;
                        charact[16] = len;
                        needHrConversion = true;
                    }
                }
                char* charact_val = *(char**)(charact + 0x20);
                int charact_len = charact[0x28];
                for (int i = 0; i < charact_len; i++)
                    bptr += sprintf(bptr, "%02X ", (int)(unsigned char)charact_val[i]);
                if (bJoystick && !glbNoSchwinn && (GetKeyState(VK_CAPITAL) & 0x0001) == 0 /*caps to turn off drifting mouse*/ &&
                    charact_len == 4 && charact[0]=='0' && charact[1]=='x' && charact[2]=='3' && charact[3]=='4' && charact[4]=='7') { //Steering:347...
                    float steerValue;
                    memcpy(&steerValue, charact_val, 4);
                    if (steerValue == 0.001) {
                        mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                    } else {
                        useful_chunks++;
                        if (fabs(steerValue) * 100 - float(int(fabs(steerValue) * 100)) > 0.09) { //тысячная - нажатие кнопки
                            mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        }
                        // квадрант4: 35.35 -> xVal=35, yVal=0.35
                        // квадрант3: -35 + 0.35 = -34.65 -> xVal=-34, yVal=-0.65
                        // квадрант2: -35 - 0.35 = -35.35 -> xVal=-35, yVal=-0.35
                        // квадрант1: 35 - 0.35 = 34.65 -> xVal=34, yVal=0.65
                        float xVal = truncf(steerValue), yVal = (steerValue - xVal) * 100;
                        if (yVal > 50) { // Q1
                            steerValue = xVal + 1.0f;
                            yVal = yVal - 100.0f;
                        } else if (yVal > 0) { //Q4
                            steerValue = xVal;
                        } else if (yVal < -50) { //Q3
                            steerValue = xVal - 1.0f;
                            yVal = 100.0f + yVal;
                        } else if (yVal < 0) { //Q2
                            steerValue = xVal;
                        }
                        memcpy(charact_val, &steerValue, 4);
                        static int16_t lastDx = 0, lastDy = 0;
                        if (steerValue != 0.0 || yVal != 0.0) {
                            int16_t dx = (int16_t)steerValue, dy = (int16_t)yVal;
                            if (dx * lastDx <= 0) dx /= 2; else if (dx / lastDx > 2) dx = 2 * lastDx;
                            if (dy * lastDy <= 0) dy /= 2; else if (dy / lastDy > 2) dy = 2 * lastDy;
                            //sprintf(bptr, "dx=%d, dy=%d, lastDx=%d, lastDy=%d\n", (int)dx, (int)dy, (int)lastDx, (int)lastDy);
                            //OutputDebugStringA(buf);
                            mouse_event(MOUSEEVENTF_MOVE, (DWORD)dx, (DWORD)dy, 0, 0);
                            lastDx = dx; lastDy = dy;
                        } else {
                            lastDx = 0; lastDy = 0;
                        }
                    }
                }
                else if (bJoystick) {
                    useful_chunks++; //to show steering device
                }
                if (needHrConversion) {
                    strncpy_s(glbLastSchwinnPseudoHandle, sizeof(glbLastSchwinnPseudoHandle), byteData + 8, sizeof(glbLastSchwinnPseudoHandle));
                    if (charact_len == 20) {
                        static DWORD last_tick = 0;
                        DWORD now_tick = GetTickCount();
                        if (now_tick - last_tick > 1000) {
                            last_tick = now_tick;
                            byte new_hr = charact_val[16];
                            charact_len = 2;
                            charact_val[0] = 0;
                            charact_val[1] = new_hr;
                            charact[0x28] = charact_len;
                            useful_chunks++;
                        }
                    }
                } else if (needPowerConversion) {
                    useful_chunks++;
                    if (charact_len == 0x11 && charact_val[0] == 0x11 && charact_val[1] == 0x20) { //11 20 00 80 10 00 00 80 9C 21 F7 40 05 00 00 00 04 
                        bptr += sprintf(bptr, "-> ");
                        charact_len = 4;
                        charact[0x28] = charact_len;
#if 1
                        byte* cdata = (byte*)charact_val;
                        static byte oldc = 0xFF;
                        if (oldc != cdata[4]) { //если 8 раз подать одно значение, zwift нам весь каденс занулит
                            doCadence(cdata);
                            oldc = cdata[4];
                        }
                        int tim = cdata[8] | (cdata[9] << 8);
                        static int mLastTime = 0; //в 1024-х долях секунды
#ifdef OLD_ALGO
                        static int64_t mLastCalories = 0;
                        int64_t calories = cdata[10] | (cdata[11] << 8) | (cdata[12] << 16) | (cdata[13] << 24) | ((int64_t)cdata[14] << 32) | (int64_t(cdata[15] & 0x7F) << 40);
                        if (mLastCalories == 0 || tim == mLastTime) {
                            mLastCalories = calories;
                            mLastTime = tim;
                            charact_val[0] = charact_val[1] = charact_val[2] = charact_val[3] = 0;
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
#else //new algo: f(cadence, res)
                        {
                            double power = mLastPower;
                            DWORD nowTicks = GetTickCount(), dTicks = nowTicks - glbLastSchwinnPowerTick;
                            glbLastSchwinnPowerIncome = nowTicks;
                            static double oldCranks = -1.0;
                            double newCranks = cdata[3] / 256.0 + cdata[4] + cdata[5] * 256.0;
                            while (newCranks < oldCranks) newCranks += 65536.0;
                            if (oldCranks < 0) {
                                oldCranks = newCranks; 
                            }
                            double dCranks = newCranks - oldCranks;
                            if (dCranks > 1.0) {
                                int reportedResistance = cdata[16];
                                static int glbActualResistance = 0;
                                if (reportedResistance) {
                                    const int averageStepDur = 750; //750ms даём на шаг сопротивления
                                    if (glbActualResistance == 0) glbActualResistance = reportedResistance;
                                    else if (reportedResistance != glbActualResistance) {
                                        static unsigned long glbLastResStep = 0;
                                        while (nowTicks - glbLastResStep > averageStepDur) {
                                            glbLastResStep += averageStepDur;
                                            if (reportedResistance > glbActualResistance) glbActualResistance++;
                                            else if (reportedResistance < glbActualResistance) glbActualResistance--;
                                            else {
                                                glbLastResStep = nowTicks;
                                                break;
                                            }
                                        }
                                    }
                                }

                                oldCranks = newCranks;
                                double powerByTicks = -10000.0, powerByDeviceTime = -10000.0;
                                glbLastSchwinnPowerTick = nowTicks;
                                if (dTicks > 0 && dTicks < 4000) {
                                    double cadenceByTicks = dCranks * 60000.0 / dTicks;
                                    powerByTicks = CalcPowerNewAlgo(cadenceByTicks, glbActualResistance);
                                }
                                int dTim = tim - mLastTime;
                                mLastTime = tim;
                                if (dTim > 0 && dTim < 4000) {
                                    double cadenceByDev = dCranks * 61440.0 / dTicks;
                                    powerByDeviceTime = CalcPowerNewAlgo(cadenceByDev, glbActualResistance);
                                }
                                double deltaPowerByTicks = fabs(powerByTicks - mLastPower), deltaPowerByDev = fabs(powerByDeviceTime - mLastPower);
                                power = (deltaPowerByDev < deltaPowerByTicks) ? powerByDeviceTime : powerByTicks;
                                if (glbSchwinnDumpFile) {
                                    fprintf(glbSchwinnDumpFile, "%d,%d,%d,%f,%f,%f,%d,%d\n", nowTicks - glbStartTicks,
                                        dTicks, dTim, powerByTicks, powerByDeviceTime, power, reportedResistance, glbActualResistance);
                                    fflush(glbSchwinnDumpFile);
                                }
                            }

#endif //OLD_ALGO
                            if (mLastPower == -1.0 || fabs(mLastPower - power) < 100.0)
                                mLastPower = power;
                            else
                                mLastPower += (power - mLastPower) / 2.0;
                            if (mLastPower < 0)
                                mLastPower = 0;
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
#else
                            int pwr = subst_power; { // полезно при тестировании, чтобы на педалях не потеть
#endif
                            charact_val[0] = charact_val[1] = 0;
                            charact_val[2] = (byte)(pwr & 0xFF);
                            charact_val[3] = (byte)((pwr >> 8) & 0xFF);
                            //char tmp[100];
                            //sprintf(tmp, "mLastPower=%f\n", mLastPower);
                            //OutputDebugString(tmp);
                            //Debug.WriteLine($"Time: {(int)(tim / 1024)}s, {(int)mLastPower}W, {calories >> 8}c, mult={mult}, {currentCadence()}rpm");
                        }

                        for (int i = 0; i < charact_len; i++)
                            bptr += sprintf(bptr, " %02X ", (int)(unsigned char)charact_val[i]);
                    }
                } else useful_chunks++;
                charact += 0x30;
            }
            service += 0x38;
        }
        bptr += sprintf(bptr, " uch=%d\n", useful_chunks);
        //OutputDebugStringA(buf);
        return useful_chunks > 0;
    }
    void doCadence(const byte* cdata) {
        BleResponsePool pool;
        void* data = newCadenceData(cdata, pool);
        if (data) {
            ParseBLEResponse(data);
            orgProcessBLEResponse(data);
        }
    }

    void* nullPowerData(BleResponsePool &pool) { //only power yet
        mLastPower = -1.0;
        char* data = pool.Allocate(0x68);
        memset(data, 0, 0x68);
        data[0] = 2;
        memcpy(data + 8, glbLastSchwinnPseudoHandle, 15);
        data[0x18] = (char)strlen(data + 8); data[0x20] = 15;
        memcpy(data + 0x28, "SCHWINN 170/270", 15);
        data[0x38] = 15; data[0x40] = 15;
        char* serv = pool.Allocate(0x38);
        memset(serv, 0, 0x38);
        *(void**)(data + 0x50) = serv;
        *(void**)(data + 0x58) = serv + 0x38;
        *(void**)(data + 0x60) = serv + 0x38;
        memcpy(serv, "0x1818", 6);
        serv[0x10] = 6; serv[0x18] = 15;
        int numchars = 1;
        char* chars = pool.Allocate(0x30 * numchars);
        memset(chars, 0, 0x30 * numchars);
        *(void**)(serv + 0x20) = chars;
        *(void**)(serv + 0x28) = chars + 0x30 * numchars;
        *(void**)(serv + 0x30) = chars + 0x30 * numchars;
        memcpy(chars, "0x2a63", 6);
        chars[0x10] = 6; chars[0x18] = 15;
        char *char_value = pool.Allocate(4);
        char_value[0] = 0; char_value[1] = 0; char_value[2] = 0; char_value[3] = 0;

        *(void**)(chars + 0x20) = char_value;
        chars[0x28] = 4;
        return data;
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

        //char buf[100];
        //sprintf(buf, "STEER_OUT=%f\n", steer);
        //OutputDebugStringA(buf);

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
            if(ParseBLEResponse(data))
                orgProcessBLEResponse(data);
            BleResponsePoolFreeAll();
        }
    }
    void ExecuteSteeringThread() {
        int counter = 0, counterMax = 20;
        OutputDebugString("Enter ExecuteSteeringThread\n");
        do {
            bool bSchwinnStopped = (int(GetTickCount() - glbLastSchwinnPowerIncome) > 1500 && glbStartTicks != glbLastSchwinnPowerIncome && mLastPower > 0);
            switch (WaitForSingleObject(glbWakeSteeringThread, 50)) {
            case WAIT_TIMEOUT: { //ничего не происходит
                    bool left = (GetKeyState(VK_LEFT) & 0xF000), right = (GetKeyState(VK_RIGHT) & 0xF000);
                    ++counter;
                    if (counter > counterMax || left || right || bSchwinnStopped) {
                        if (left || right) {
                            OnSteeringKeyPress(left ? VK_LEFT : VK_RIGHT, true);
                            counterMax = 2;
                        }
                        counter = 0;
                    }
                    else {
                        continue;
                    }
                }
                break;
            default: 
                OutputDebugString("ExecuteSteeringThread. Failed wait and exit\n");
                return; //сбой в системе, уходим
            case WAIT_OBJECT_0: //разбудили - для выхода или работы?
                if (glbTerminate) {
                    OutputDebugString("ExecuteSteeringThread. Wake for exit\n");
                    return;
                }
                counterMax = 2;
                break;
            }
            if (orgProcessBLEResponse) {
                BleResponsePool pool;
                void* data = newSteeringData(pool);
                if (!glbTerminate) ParseBLEResponse(data);
                if (!glbTerminate) orgProcessBLEResponse(data);
                if (!glbTerminate && bSchwinnStopped) {
                    OutputDebugString("ExecuteSteeringThread bSchwinnStopped\n");
                    data = nullPowerData(pool);
                    if (!glbTerminate) ParseBLEResponse(data);
                    if (!glbTerminate) orgProcessBLEResponse(data);
                    data = newCadenceData(NULL, pool);
                    if (!glbTerminate) ParseBLEResponse(data);
                    if (!glbTerminate) orgProcessBLEResponse(data);
                }
                if (glbSteeringTask == 0.0 && glbSteeringCurrent == 0.0)
                    counterMax = 20;
            }
        } while (!glbTerminate);
        OutputDebugString("Exit ExecuteSteeringThread\n");
    }

    /*fptr_void_ptr orgOnPairCB = nullptr;
    void OnPairCB(void* data) {
        if (orgOnPairCB) {
            OutputDebugString("OnPairCB\n");
            orgOnPairCB(data);
        }
    }*/

    void pushSchwinnToGetHeartRate() {
        HANDLE deviceHandle = INVALID_HANDLE_VALUE;
        char buf[1024];
        HDEVINFO handle_ = INVALID_HANDLE_VALUE;
        GUID BluetoothClassGUID = GUID_BLUETOOTHLE_SCHWINN_SERV;
        HDEVINFO result = SetupDiGetClassDevs(
            &BluetoothClassGUID,
            NULL,
            NULL,
            //DIGCF_PRESENT | DIGCF_ALLCLASSES | DIGCF_DEVICEINTERFACE));
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (result == INVALID_HANDLE_VALUE) {
            sprintf(buf, "SetupDiGetClassDevs:INVALID_HANDLE_VALUE, le=%x\n", GetLastError());
            OutputDebugStringA(buf);
            return;
        }

        if (handle_ != INVALID_HANDLE_VALUE) {
            SetupDiDestroyDeviceInfoList(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
        handle_ = result;

        GUID BluetoothInterfaceGUID = GUID_BLUETOOTHLE_SCHWINN_SERV;

        //std::vector<SP_DEVICE_INTERFACE_DATA> ble_interfaces;
        std::vector<std::string> ble_paths;

        // Enumerate device of LE_DEVICE interface class
        for (int i = 0;; i++) {
            std::string error;
            SP_DEVICE_INTERFACE_DATA device_interface_data = { 0 };
            device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
            BOOL success = SetupDiEnumDeviceInterfaces(
                handle_,
                NULL,
                (LPGUID)&BluetoothInterfaceGUID,
                (DWORD)i,
                &device_interface_data);
            if (!success) {
                DWORD last_error = GetLastError();
                if (last_error == ERROR_NO_MORE_ITEMS) {
                    //Enum devices finished.
                    break;
                }
                else {
                    sprintf(buf, "Error enumerating device interfaces, le=%x\n", last_error);
                    OutputDebugStringA(buf);
                    return;
                }
            }

            // Retrieve required # of bytes for interface details
            ULONG required_length = 0;
            success = SetupDiGetDeviceInterfaceDetail(
                handle_,
                &device_interface_data,
                NULL,
                0,
                &required_length,
                NULL);
            std::unique_ptr<UINT8> interface_data(new UINT8[required_length]);
            RtlZeroMemory(interface_data.get(), required_length);

            PSP_DEVICE_INTERFACE_DETAIL_DATA device_interface_detail_data = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(interface_data.get());
            device_interface_detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            SP_DEVINFO_DATA device_info_data = { 0 };
            device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

            ULONG actual_length = required_length;
            success = SetupDiGetDeviceInterfaceDetail(
                handle_,
                &device_interface_data,
                device_interface_detail_data,
                actual_length,
                &required_length,
                &device_info_data);

            //Store data
            std::string strpath = std::string(device_interface_detail_data->DevicePath);
            /*OLECHAR* bstrGuid;
            StringFromCLSID(device_interface_data.InterfaceClassGuid, &bstrGuid);*/
            ble_paths.push_back(strpath);
            //ble_interfaces.push_back(device_interface_data);
            //sprintf(buf, "ble_paths+=%s\n", strpath.c_str());
            //OutputDebugStringA(buf);
        }

        //Select device to open
        std::string path = ble_paths[0];
        USHORT required_count = 0;
        // Get GATT Services Count
        deviceHandle = CreateFile(path.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (deviceHandle == INVALID_HANDLE_VALUE) {
            sprintf(buf, "CreateFile:INVALID_HANDLE_VALUE, le=%x, path=%s\n", GetLastError(), path.c_str());
            OutputDebugStringA(buf);
            return;
        }

        HRESULT hr = BluetoothGATTGetServices(deviceHandle, 0, NULL, &required_count, BLUETOOTH_GATT_FLAG_NONE);
        if (required_count == 0) {
            sprintf(buf, "BluetoothGATTGetServices:%x, %d le=%x\n", hr, required_count, GetLastError());
            OutputDebugStringA(buf);
            return;
        }
        //Get GATT Services
        std::string error;
        std::unique_ptr<BTH_LE_GATT_SERVICE> services(new BTH_LE_GATT_SERVICE[required_count]);
        USHORT actual_count = required_count;
        hr = BluetoothGATTGetServices(deviceHandle, actual_count, services.get(), &required_count, BLUETOOTH_GATT_FLAG_NONE);
        if (hr) {
            sprintf(buf, "BluetoothGATTGetServices:%x le=%x\n", hr, GetLastError());
            OutputDebugStringA(buf);
            CloseHandle(deviceHandle);
            return;
        }

        for (int i = 0; i < actual_count; i++) {
            BTH_LE_GATT_SERVICE& service(services.get()[i]);

            //Get GATT Characteristics
            USHORT required_count;
            HRESULT hr = BluetoothGATTGetCharacteristics(deviceHandle, &service, 0, NULL, &required_count, BLUETOOTH_GATT_FLAG_NONE);

            if (hr != HRESULT_FROM_WIN32(ERROR_MORE_DATA)) {
                sprintf(buf, "BluetoothGATTGetCharacteristics:%x, %d le=%x\n", hr, required_count, GetLastError());
                OutputDebugStringA(buf);
                CloseHandle(deviceHandle);
                return;
            }

            std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC> gatt_characteristics(new BTH_LE_GATT_CHARACTERISTIC[required_count]);
            USHORT actual_count = required_count;
            hr = BluetoothGATTGetCharacteristics(deviceHandle, &service, actual_count, gatt_characteristics.get(), &required_count, BLUETOOTH_GATT_FLAG_NONE);
            if (hr) {
                sprintf(buf, "BluetoothGATTGetCharacteristics:%x le=%x\n", hr, GetLastError());
                OutputDebugStringA(buf);
                CloseHandle(deviceHandle);
                return;
            }

            //Enum Characteristics
            for (int i = 0; i < actual_count; i++) {
                BTH_LE_GATT_CHARACTERISTIC& gatt_characteristic(gatt_characteristics.get()[i]);
                if (gatt_characteristic.CharacteristicUuid.Value.LongUuid.Data1 != 0x1717b3c0)
                    continue;
                UCHAR valueData[] = { 5,0,0,0, 5,3,0xd9,0,0x1f };
                BTH_LE_GATT_CHARACTERISTIC_VALUE* charact_v = (PBTH_LE_GATT_CHARACTERISTIC_VALUE)valueData;

                hr = BluetoothGATTSetCharacteristicValue(
                    deviceHandle, //HANDLE                             hDevice,
                    &gatt_characteristic,    //PBTH_LE_GATT_CHARACTERISTIC        Characteristic,
                    charact_v,   //PBTH_LE_GATT_CHARACTERISTIC_VALUE  CharacteristicValue,
                    NULL,        //BTH_LE_GATT_RELIABLE_WRITE_CONTEXT ReliableWriteContext,
                    BLUETOOTH_GATT_FLAG_NONE
                );
                sprintf(buf, "BluetoothGATTSetCharacteristicValue:%x\n", hr);
                OutputDebugStringA(buf);
            }
        }
        CloseHandle(deviceHandle);
    }
    __declspec(dllexport) bool BLEPairToDevice(void *a1) {
        if(strtoull(glbLastSchwinnPseudoHandle, 0, 10) == reinterpret_cast<uint64_t>(a1))
            pushSchwinnToGetHeartRate();
        static fptr_bool_ptr real = (fptr_bool_ptr)GetProcAddress(org, "BLEPairToDevice");
        return real(a1);
    }
    __declspec(dllexport) void BLEInitBLEFlags() {
        static fptr_void_void real = (fptr_void_void)GetProcAddress(org, "BLEInitBLEFlags");
        real();
#ifdef ENABLE_EDGE_REMOTE
        //const char* edge_id_str = getenv("ZWIFT_EDGE_REMOTE");
        //if (edge_id_str && *edge_id_str) {
            glbWakeSteeringThread = ::CreateEvent(NULL, false, false, NULL);
            glbSteeringThread.reset(new std::thread(ExecuteSteeringThread)); //ему еще слушать клавиатуру и занулять мощность, если Schwinn прекратил крутить
        //}
#endif
        const char* dump_file = getenv("ZWIFT_DUMP_SCHWINN");
        if (dump_file && *dump_file && *dump_file != '-' && NULL == glbSchwinnDumpFile) {
            glbSchwinnDumpFile = fopen(dump_file, "wt");
            if (glbSchwinnDumpFile) {
                fprintf(glbSchwinnDumpFile, "ticks,dticks,ddev_t1024,pwr_t,pwr_d,power,rep_resist,act_resist\n");
                fflush(glbSchwinnDumpFile);
            }
        }
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
    __declspec(dllexport) void BLESetBLEErrorFunc(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetBLEErrorFunc");
        if(real) real(a1);
    }
    __declspec(dllexport) void BLESetProcessBLEResponse(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLESetProcessBLEResponse");
        orgProcessBLEResponse = (fptr_void_ptr)a1;
        a1 = ProcessBLEResponse;
        return real(a1);
    }
    __declspec(dllexport) void BLEStartScanning(void *a1) {
        static fptr_void_ptr real = (fptr_void_ptr)GetProcAddress(org, "BLEStartScanning");
        const char *use_schwinn = getenv("ZWIFT_DUMP_SCHWINN");
        if (use_schwinn == nullptr || *use_schwinn != '-') {
            glbNoSchwinn = false;
            char* from = (char*)*(void**)a1;
            char* to = (char*)*((char**)a1 + 1);
            while (from < to) {
                char serv_length = from[0x10];
                /*if (serv_length < 1000) {
                    char buf[1024] = { 0 };
                    memcpy(buf, serv_length <= 15 ? from : *(char **)from, serv_length);
                    buf[serv_length] = '\n';
                    OutputDebugStringA(buf);
                }*/

                /*if (serv_length <= 15 && memcmp(from, "0x1814", 7) == 0) { // беговую дорожку заменим на Schwinn
                    char* schwinn_serv = new_char_zwift(37);
                    memcpy(schwinn_serv, SCHWINN_SERV, 37);
                    memcpy(from, &schwinn_serv, 8);
                    from[0x10] = 36; from[0x18] = 36;
                    char* schwinn_char = new_char_zwift(37);
                    memcpy(schwinn_char, SCHWINN_CHAR_EVENT, 37);
                    char *ptr = (char*)*((char**)from + 4);
                    memcpy(ptr, &schwinn_char, 8);
                    ptr[0x10] = 36; ptr[0x18] = 36;
                } else*/ if (serv_length == 36 && memcmp(*(char**)from, "A026EE07-0A7D-4AB3-97FA-F1500F9FEB8B", 36) == 0) { // жертвуем Wahoo
                    memcpy(*(char**)from, SCHWINN_SERV, 36);
                    char** ptr = (char**)*((char**)from + 4);
                    //OutputDebugStringA(*ptr);
                    memcpy(*ptr, SCHWINN_CHAR_EVENT, 36);
                    ptr += 6;
                    memcpy(*ptr, SCHWINN_CHAR_SRD0, 36);
                }
                from += 0x38;
            }
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
