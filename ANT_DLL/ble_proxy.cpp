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
#include <string>
#include <map>

bool glbNoSchwinn = true;
const char *SCHWINN_SERV = "3bf58980-3a2f-11e6-9011-0002a5d5c51b";
GUID GUID_BLUETOOTHLE_SCHWINN_SERV = { 0x3bf58980, 0x3a2f, 0x11e6, {0x90, 0x11, 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b} };
const char *SCHWINN_CHAR_EVENT = "5c7d82a0-9803-11e3-8a6c-0002a5d5c51b";
const char *SCHWINN_SHORT_CHAR_EVENT = "0x5c7d82a0";
const char *SCHWINN_CHAR_SRD0 = "6be8f580-9803-11e3-ab03-0002a5d5c51b";
const char *SCHWINN_SHORT_CHAR_SRD0 = "0x6be8f580";

HMODULE org = LoadLibraryA("BleWin10Lib_V2.org.dll");
typedef bool (*fptr_bool_ptr)(void *);
typedef bool (*fptr_bool_ptr3)(void *, void *, void *);
typedef void (*fptr_void_ptr)(void *);
typedef void (*fptr_void_void)();
typedef int (*EVP_CipherInit_ex)(void *cip_ctx, void *cip_type, void *eng_impl, uint8_t *key, uint8_t *iv, int enc);
EVP_CipherInit_ex orgEVP_CipherInit_ex;
char *dumpHex(char *dest, const char *name, const uint8_t *data, int data_size) {
    if (data) {
        dest += sprintf(dest, " %s: ", name);
        for (int i = 0; i < data_size; i++) {
            dest += sprintf(dest, "%02X", data[i]);
        }
    } else dest += sprintf(dest, " %s: (null)", name);
    return dest;
}
int newEVP_CipherInit_ex(void* cip_ctx, void* cip_type, void* eng_impl, uint8_t* key, uint8_t* iv, int enc) {
    char buf[1024];
    char* pBuf = buf + sprintf(buf, enc ? "zwEncryptInit_ex %p" : "zwDecryptInit_ex %p", cip_ctx);
    pBuf = dumpHex(pBuf, "key", key, 16);
    pBuf = dumpHex(pBuf, "iv", iv, 12);
    pBuf += sprintf(pBuf, "\n");
    OutputDebugStringA(buf);
    if (orgEVP_CipherInit_ex)
        return orgEVP_CipherInit_ex(cip_ctx, cip_type, eng_impl, key, iv, enc);
    return 0;
}
int newEVP_DecryptInit_ex(void *cip_ctx, void *cip_type, void *eng_impl, uint8_t *key, uint8_t *iv) {
    return newEVP_CipherInit_ex(cip_ctx, cip_type, eng_impl, key, iv, 0);
}
int newEVP_EncryptInit_ex(void *cip_ctx, void *cip_type, void *eng_impl, uint8_t *key, uint8_t *iv) {
    return newEVP_CipherInit_ex(cip_ctx, cip_type, eng_impl, key, iv, 1);
}

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

const uint8_t *PatchMainModule(const char* name, // "Trial.01",
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
        return NULL;
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
                return NULL;
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
    return foundAt;
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
enum CHARSET_IDS : uint8_t {
    CID_LAT = 0x0,
    CID_JAPAN = 0x1,
    CID_KOREAN = 0x2,
    CID_CHINESE = 0x3,
    CID_CNT = 0x4
};
struct CFont2D_fileHdrV3 {
    uint16_t m_version;
    char field_2;
    char field_3;
    int m_tex[10];
    float m_kern[4];
    char field_3C[26];
    uint16_t m_charsCnt;
    char     field_58[4];   //10,0,94,-1 (data/Fonts/ZwiftFondoBlack105ptW_EFIGS_K.bin); = (data/Fonts/ZwiftFondoMedium54ptW_EFIGS_K.bin)
    char     m_family[16];  //"Zwift Fondo ZGA"
    int      m_points;      //105; 75? why not 54
    char     m_gap[124];    //0's
    int      m_realKerns;   //0 until Ursoft kern fix
};
struct tViewport_u16 {
    uint16_t m_left;
    uint16_t m_top;
    uint16_t m_width;
    uint16_t m_height;
};
struct CFont2D_info {
    CHARSET_IDS m_langId;
    char m_fileHdrV1[0x9C]; //__declspec(align(4)) CFont2D_fileHdrV1
    CFont2D_fileHdrV3 m_fileHdrV3;
    tViewport_u16 m_v1gls[256];
    uint16_t m_glyphIndexes[65536];
};
struct CFont2D_glyph {
    uint16_t m_codePoint;
    char m_kernIdx;
    uint8_t m_cnt;
    float m_left;
    float m_top;
    float m_width;
    float m_height;
};
using UChar = wchar_t;
std::map<std::pair<UChar, UChar>, float> glbEmptyMap, glb105Map, glb54Map;
struct CFont2D;
struct UrsoftKerner {
    const CFont2D &m_font;
    UrsoftKerner(const CFont2D &font);
    std::map<std::pair<UChar, UChar>, float> *m_realKerning;
    std::pair<UChar, UChar> m_realKernKey;
    float operator ()(UChar ch);
    static UrsoftKerner &stSelect(CFont2D *t);
    void cleanup() {
        m_realKernKey.second = 0;
    }
};
struct CFont2D {
    char *m_fileName[4] /*std::string*/;
    char m_lidKernIdx[0x60] /*std::ContainerBase[4]*/;
    float m_kern[4];
    CFont2D_info m_info;
    CFont2D_glyph *m_glyphs;
    void *m_RGBAv1;
    int m_lineHeight;
    CHARSET_IDS m_texSuffix;
    char field_20A35;
    char field_20A36;
    char field_20A37;
    int m_tex;
    float m_field_20A3C;
    float m_scale;
    float m_kerning;
    float m_baseLine;
    float m_headLine;
    float m_verticalOffset;
    float m_spaceScale;
    char m_loadedV1;
    char m_loadedV3;
    char field_20A5A;
    char field_20A5B;
    char field_20A5C;
    char field_20A5D;
    char field_20A5E;
    char field_20A5F;
    void *m_cache /*CFont2D_cache * */;
    int m_cacheCnt;
    int m_cacheCntUsed;
    int m_curCache;
    char field_20A74;
    char field_20A75;
    char field_20A76;
    char field_20A77;
    char m_hbStack[0x28] /*CFont2D_HeadBase_deque*/;
    bool Is105() const { return m_loadedV3 && m_info.m_fileHdrV3.m_points == 105; }
    bool Is54() const { return m_loadedV3 && m_info.m_fileHdrV3.m_points == 75; }
    float GetHeight() {
        auto ret = m_lineHeight * m_field_20A3C * m_scale;
        if (ret > 0.0f)
            return ret;
        else
            return 1.0f;
    }
    int GetParagraphLineCountW(float w, const UChar *str, float scale, float marg, bool unlim);
    static float StringWidthW_ulen(CFont2D *t, UChar *uText, uint32_t textLen) {
        if (!t->m_loadedV3 || !uText || !*uText || !textLen)
            return 0.0f;
        float       ret = 0.0f;
        const UChar *uEnd = uText + textLen;
        UrsoftKerner uk(*t);
        do {
            auto ch = *uText;
            auto gidx = t->m_info.m_glyphIndexes[ch];
            if (gidx != 0xFFFF) {
                float mult = (ch == 32) ? t->m_spaceScale : 1.0f;
                auto  kidx = t->m_glyphs[gidx].m_kernIdx;
                ret += t->m_kern[kidx] * t->m_kerning * t->m_info.m_fileHdrV3.m_kern[kidx] * (t->m_glyphs[gidx].m_width + uk(ch)) * mult;
            }
        } while (++uText != uEnd);
        return ret * t->m_scale;
    }
    float CharWidthW(UChar ch, float add = 0.0f) {
        if (!m_loadedV3)
            return 0.0f;
        auto gi = m_info.m_glyphIndexes[ch];
        if (gi == 0xFFFF)
            return 0.0f;
        auto kernIdx = m_glyphs[gi].m_kernIdx;
        auto ret = m_scale * m_kern[kernIdx] * m_kerning * m_info.m_fileHdrV3.m_kern[kernIdx] 
            * (m_glyphs[gi].m_width + add);
        if (ch == 32)
            ret *= m_spaceScale;
        return ret;
    }
};
UrsoftKerner::UrsoftKerner(const CFont2D &font) : m_font(font) {
    if (font.Is105())
        m_realKerning = &glb105Map;
    else if (font.Is54())
        m_realKerning = &glb54Map;
    else
        m_realKerning = &glbEmptyMap;
}
float UrsoftKerner::operator ()(UChar ch) {
    if (GetKeyState(VK_SCROLL) & 0x0001) {
        return 0; //when scroll is on, no Ursoft kerning
    }
    m_realKernKey.first = m_realKernKey.second;
    m_realKernKey.second = ch;
    if (ch != 32 && m_realKernKey.first && !m_realKerning->empty()) {
        auto fnd = m_realKerning->find(m_realKernKey);
        if (fnd != m_realKerning->end())
            return fnd->second;
    }
    return 0;
}
struct UrsoftFontPatcher : public UrsoftKerner {
    UrsoftFontPatcher(CFont2D &f) : UrsoftKerner(f) {
        //все цифры одинаковой ширины и посередине
        int width = 100 + 3, *pShift;
        if (f.Is105()) {
            static int sh105[]{ -1,-2,0,-4,-1,-2,-2,-4,-2,0 };
            pShift = sh105;
        } else if (f.Is54()) {
            width = 70 + 1;
            static int sh54[]{ 1,0,1,-1,-1,0,0,-1,0,0 };
            pShift = sh54;
        } else return;
        for (auto ch = L'0'; ch <= L'9'; ch++) {
            auto gidx = f.m_info.m_glyphIndexes[ch];
            if (gidx != 0xFFFF) {
                f.m_glyphs[gidx].m_width = width / 2048.0f;
                f.m_glyphs[gidx].m_left += pShift[ch - L'0'] / 2048.0f;
            }
        }
    }
};
UrsoftKerner &UrsoftKerner::stSelect(CFont2D *t) {
    if (t->Is105()) {
        static UrsoftFontPatcher _105(*t);
        return _105;
    } else if (t->Is54()) {
        static UrsoftFontPatcher _54(*t);
        return _54;
    }
    static UrsoftKerner others(*t);
    return others;
}

float patched_CFont2D_StringWidthW_ulen_loop(CFont2D *t, UChar *uText, uint32_t textLen) {
    static_assert(0x20AA0 == sizeof(CFont2D));
    static_assert(0x20990 == sizeof(CFont2D_info));
    static_assert(0xF0 == sizeof(CFont2D_fileHdrV3));
    static_assert(0x14 == sizeof(CFont2D_glyph));
    //::MessageBoxA(NULL, "patched_CFont2D_StringWidthW_ulen_loop", "Zwift.AntDll", 0);
    register float ret = 0.0f;
    UrsoftKerner uk(*t);
    do {
        auto ch = *uText;
        register auto gidx = t->m_info.m_glyphIndexes[ch];
        if (gidx != 0xFFFF) {
            register auto kidx = t->m_glyphs[gidx].m_kernIdx;
            ret += t->m_kern[kidx] * t->m_kerning * t->m_info.m_fileHdrV3.m_kern[kidx] * (t->m_glyphs[gidx].m_width + uk(ch))
                * ((*uText == 32) ? t->m_spaceScale : 1.0f);
        }
        ++uText;
        --textLen;
    } while (textLen);
    return ret * t->m_scale;
}
void patchCFont2D__StringWidthW_ulen() {
/*.text:00007FF74C3CDB3C                                   ; float __fastcall CFont2D::StringWidthW_ulen(CFont2D *this, _WORD *uText, unsigned int textLen)
.text:00007FF74C3CDB3C                                   CFont2D__StringWidthW_ulen proc near    ; CODE XREF: CFont2D__StringWidthW_u+2B↑j
.text:00007FF74C3CDB3C                                                                           ; GUI_EditBox__CalculateCursorOffsetForAlignment+1B↓p ...
.text:00007FF74C3CDB3C 000 33 C0                                         xor     eax, eax        ; Logical Exclusive OR
.text:00007FF74C3CDB3E 000 38 81 59 0A 02 00                             cmp     [rcx+20A59h], al ; Compare Two Operands
.text:00007FF74C3CDB44 000 0F 84 9F 00 00 00                             jz      loc_7FF74C3CDBE9 ; Jump if Zero (ZF=1)
.text:00007FF74C3CDB4A 000 48 85 D2                                      test    rdx, rdx        ; Logical Compare
.text:00007FF74C3CDB4D 000 0F 84 96 00 00 00                             jz      loc_7FF74C3CDBE9 ; Jump if Zero (ZF=1)
.text:00007FF74C3CDB53 000 66 39 02                                      cmp     [rdx], ax       ; Compare Two Operands
.text:00007FF74C3CDB56 000 0F 84 8D 00 00 00                             jz      loc_7FF74C3CDBE9 ; Jump if Zero (ZF=1)
.text:00007FF74C3CDB5C 000 0F 57 C0                                      xorps   xmm0, xmm0      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF74C3CDB5F 000 45 85 C0                                      test    r8d, r8d        ; Logical Compare
.text:00007FF74C3CDB62 000 74 7C                                         jz      short loc_7FF74C3CDBE0 ; Jump if Zero (ZF=1)
.text:00007FF74C3CDB64 000 45 8B C8                                      mov     r9d, r8d        ; _textLen = textLen
.text:00007FF74C3CDB67
.text:00007FF74C3CDB67                                   do_loop:                                ; CODE XREF: CFont2D__StringWidthW_ulen+A2↓j*/
    char jump[]{ "\x48\x83\xEC\x30\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xD0\x48\x83\xC4\x30\xC3" }; //sub rsp, 0x30; mov rax, addr; call rax; add rsp, 0x30; ret
    *(void **)(jump + 6) = (void *)&CFont2D::StringWidthW_ulen;
    PatchMainModule(
        "CFont2D.StringWidthW_ulen", 21,
        "\x33\xC0\x38\x81\x59\x0A\x02\x00\x0F\x84\x9F\x00\x00\x00\x48\x85\xD2\x0F\x84\x96\x00",
        jump
    );
    /*
.text:00007FF74C3CDB67 000 0F B7 02                                      movzx   eax, word ptr [rdx] ; gidx = this->m_info.m_glyphIndexes[*uText]
.text:00007FF74C3CDB6A 000 44 0F B7 84 41 20 0A 00 00                    movzx   r8d, word ptr [rcx+rax*2+0A20h] ; Move with Zero-Extend
.text:00007FF74C3CDB73 000 B8 FF FF 00 00                                mov     eax, 0FFFFh
.text:00007FF74C3CDB78 000 66 44 3B C0                                   cmp     r8w, ax         ; if ( gidx != 0xFFFF )
.text:00007FF74C3CDB7C 000 73 58                                         jnb     short skip      ; Jump if Not Below (CF=0)
.text:00007FF74C3CDB7E 000 66 83 3A 20                                   cmp     word ptr [rdx], 20h ; ' ' ; if ( *uText == 32 )
.text:00007FF74C3CDB82 000 4F 8D 04 80                                   lea     r8, [r8+r8*4]   ; Load Effective Address
.text:00007FF74C3CDB86 000 48 8B 81 20 0A 02 00                          mov     rax, [rcx+20A20h]
.text:00007FF74C3CDB8D 000 46 0F B6 54 80 02                             movzx   r10d, byte ptr [rax+r8*4+2] ; Move with Zero-Extend
.text:00007FF74C3CDB93 000 F3 42 0F 10 9C 91 5C 01 00 00                 movss   xmm3, dword ptr [rcx+r10*4+15Ch] ; Move Scalar Single-FP
.text:00007FF74C3CDB9D 000 75 0A                                         jnz     short no_space_branch ; Jump if Not Zero (ZF=0)
.text:00007FF74C3CDB9F 000 F3 0F 10 91 54 0A 02 00                       movss   xmm2, dword ptr [rcx+20A54h] ; _spaceScale = this->m_spaceScale;
.text:00007FF74C3CDBA7 000 EB 08                                         jmp     short line28    ; Jump
.text:00007FF74C3CDBA9                                   ; ---------------------------------------------------------------------------
.text:00007FF74C3CDBA9
.text:00007FF74C3CDBA9                                   no_space_branch:                        ; CODE XREF: CFont2D__StringWidthW_ulen+61↑j
.text:00007FF74C3CDBA9 000 F3 0F 10 15 4B D0 A7 00                       movss   xmm2, cs:units  ; _spaceScale = 1.0;
.text:00007FF74C3CDBB1
.text:00007FF74C3CDBB1                                   line28:                                 ; CODE XREF: CFont2D__StringWidthW_ulen+6B↑j
.text:00007FF74C3CDBB1 000 F3 42 0F 10 8C 91 80 00 00 00                 movss   xmm1, dword ptr [rcx+r10*4+80h] ; Move Scalar Single-FP
.text:00007FF74C3CDBBB 000 F3 0F 59 89 44 0A 02 00                       mulss   xmm1, dword ptr [rcx+20A44h] ; Scalar Single-FP Multiply
.text:00007FF74C3CDBC3 000 F3 42 0F 59 5C 80 0C                          mulss   xmm3, dword ptr [rax+r8*4+0Ch] ; Scalar Single-FP Multiply
.text:00007FF74C3CDBCA 000 F3 0F 59 CB                                   mulss   xmm1, xmm3      ; Scalar Single-FP Multiply
.text:00007FF74C3CDBCE 000 F3 0F 59 CA                                   mulss   xmm1, xmm2      ; Scalar Single-FP Multiply
.text:00007FF74C3CDBD2 000 F3 0F 58 C1                                   addss   xmm0, xmm1      ; Scalar Single-FP Add
.text:00007FF74C3CDBD6
.text:00007FF74C3CDBD6                                   skip:                                   ; CODE XREF: CFont2D__StringWidthW_ulen+40↑j
.text:00007FF74C3CDBD6 000 48 83 C2 02                                   add     rdx, 2          ; ++uText
.text:00007FF74C3CDBDA 000 49 83 E9 01                                   sub     r9, 1           ; --_textLen
.text:00007FF74C3CDBDE 000 75 87                                         jnz     short do_loop   ; Jump if Not Zero (ZF=0)
.text:00007FF74C3CDBE0
.text:00007FF74C3CDBE0                                   loc_7FF74C3CDBE0:                       ; CODE XREF: CFont2D__StringWidthW_ulen+26↑j
.text:00007FF74C3CDBE0 000 F3 0F 59 81 40 0A 02 00                       mulss   xmm0, dword ptr [rcx+20A40h] ; return ret * this->m_scale
.text:00007FF74C3CDBE8 000 C3                                            retn                    ; Return Near from Procedure
.text:00007FF74C3CDBE9                                   ; ---------------------------------------------------------------------------
.text:00007FF74C3CDBE9
.text:00007FF74C3CDBE9                                   loc_7FF74C3CDBE9:                       ; CODE XREF: CFont2D__StringWidthW_ulen+8↑j
.text:00007FF74C3CDBE9                                                                           ; CFont2D__StringWidthW_ulen+11↑j ...
.text:00007FF74C3CDBE9 000 0F 57 C0                                      xorps   xmm0, xmm0      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF74C3CDBEC 000 C3                                            retn                    ; Return Near from Procedure
.text:00007FF74C3CDBEC                                   CFont2D__StringWidthW_ulen endp */
}
int patched_CFont2D_FitCharsToWidthW(CFont2D *t, const UChar *str, int w) {
    //::MessageBoxA(NULL, "patched_CFont2D_FitCharsToWidthW", "Zwift.AntDll", 0);
    if (!str || !t->m_loadedV3)
        return 0;
    int  len =  (int)wcslen(str);
    auto v8 = 0.0f;
    if (!len)
        return len;
    int i = 0;
    UrsoftKerner uk(*t);
    while (true) {
        auto ch = str[i];
        auto v11 = t->m_info.m_glyphIndexes[ch];
        if (v11 != 0xFFFF) {
            auto kernIdx = t->m_glyphs[v11].m_kernIdx;
            v8 += t->m_kern[kernIdx] * t->m_kerning * t->m_info.m_fileHdrV3.m_kern[kernIdx] *
                (t->m_glyphs[v11].m_width + uk(ch)) * t->m_scale * (ch == 32 ? t->m_spaceScale : 1.0f);
        }
        if (v8 >= w)
            break;
        if (++i >= len)
            return len;
    }
    return i;
}
void patchCFont2D__FitCharsToWidthW() {
    char jump[]{ "\x48\x83\xEC\x30\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xD0\x48\x83\xC4\x30\xC3\x00" }; //sub rsp, 0x30; mov rax, addr; call rax; add rsp, 0x30; ret
    *(void **)(jump + 6) = (void *)((char *)&patched_CFont2D_FitCharsToWidthW);
    PatchMainModule(
        "CFont2D.FitCharsToWidthW", 22,
        "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x33\xDB",
        jump
    );
    /*.text:00007FF74C3C9830                                   ; int __fastcall CFont2D::FitCharsToWidthW(CFont2D *this, UChar *str, int w)
.text:00007FF74C3C9830                                   CFont2D__FitCharsToWidthW proc near     ; CODE XREF: CFont2D__FitWordsToWidthW+2F9↓p
.text:00007FF74C3C9830                                                                           ; GUI_EditBox__Render+9AD↓p ...
.text:00007FF74C3C9830
.text:00007FF74C3C9830                                   arg_0           = qword ptr  8
.text:00007FF74C3C9830                                   arg_8           = qword ptr  10h
.text:00007FF74C3C9830                                   arg_10          = qword ptr  18h
.text:00007FF74C3C9830
.text:00007FF74C3C9830 000 48 89 5C 24 08                                mov     [rsp+arg_0], rbx
.text:00007FF74C3C9835 000 48 89 6C 24 10                                mov     [rsp+arg_8], rbp
.text:00007FF74C3C983A 000 48 89 74 24 18                                mov     [rsp+arg_10], rsi
.text:00007FF74C3C983F 000 57                                            push    rdi
.text:00007FF74C3C9840 008 48 83 EC 20                                   sub     rsp, 20h        ; Integer Subtraction
.text:00007FF74C3C9844 028 33 DB                                         xor     ebx, ebx        ; Logical Exclusive OR
.text:00007FF74C3C9846 028 41 8B E8                                      mov     ebp, r8d
.text:00007FF74C3C9849 028 48 8B F2                                      mov     rsi, rdx
.text:00007FF74C3C984C 028 48 8B F9                                      mov     rdi, rcx
.text:00007FF74C3C984F 028 48 85 D2                                      test    rdx, rdx        ; Logical Compare
.text:00007FF74C3C9852 028 0F 84 BE 00 00 00                             jz      loc_7FF74C3C9916 ; Jump if Zero (ZF=1)
.text:00007FF74C3C9858 028 38 99 59 0A 02 00                             cmp     [rcx+20A59h], bl ; Compare Two Operands
.text:00007FF74C3C985E 028 0F 84 B2 00 00 00                             jz      loc_7FF74C3C9916 ; Jump if Zero (ZF=1)
.text:00007FF74C3C9864 028 48 8B CA                                      mov     rcx, rdx
.text:00007FF74C3C9867 028 E8 A4 32 E3 FF                                call    u_strlen        ; Call Procedure
.text:00007FF74C3C986C 028 8B C8                                         mov     ecx, eax
.text:00007FF74C3C986E 028 0F 57 C9                                      xorps   xmm1, xmm1      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF74C3C9871 028 85 C0                                         test    eax, eax        ; Logical Compare
.text:00007FF74C3C9873 028 0F 84 95 00 00 00                             jz      loc_7FF74C3C990E ; Jump if Zero (ZF=1)
.text:00007FF74C3C9879 028 66 0F 6E E5                                   movd    xmm4, ebp       ; Move 32 bits
.text:00007FF74C3C987D 028 0F 5B E4                                      cvtdq2ps xmm4, xmm4     ; Convert Packed Doubleword Integers to Packed Double-Precision Floating-Point Values
.text:00007FF74C3C9880
.text:00007FF74C3C9880                                   loc_7FF74C3C9880:                       ; CODE XREF: CFont2D__FitCharsToWidthW+D8↓j
.text:00007FF74C3C9880 028 8B C3                                         mov     eax, ebx
.text:00007FF74C3C9882 028 0F 57 C0                                      xorps   xmm0, xmm0      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF74C3C9885 028 0F B7 14 46                                   movzx   edx, word ptr [rsi+rax*2] ; Move with Zero-Extend
.text:00007FF74C3C9889 028 B8 FF FF 00 00                                mov     eax, 0FFFFh
.text:00007FF74C3C988E 028 44 0F B7 84 57 20 0A 00 00                    movzx   r8d, word ptr [rdi+rdx*2+0A20h] ; Move with Zero-Extend
.text:00007FF74C3C9897 028 66 44 3B C0                                   cmp     r8w, ax         ; Compare Two Operands
.text:00007FF74C3C989B 028 73 5B                                         jnb     short loc_7FF74C3C98F8 ; Jump if Not Below (CF=0)
.text:00007FF74C3C989D 028 48 8B 87 20 0A 02 00                          mov     rax, [rdi+20A20h]
.text:00007FF74C3C98A4 028 4F 8D 04 80                                   lea     r8, [r8+r8*4]   ; Load Effective Address
.text:00007FF74C3C98A8 028 46 0F B6 4C 80 02                             movzx   r9d, byte ptr [rax+r8*4+2] ; Move with Zero-Extend
.text:00007FF74C3C98AE 028 F3 42 0F 10 9C 8F 5C 01 00 00                 movss   xmm3, dword ptr [rdi+r9*4+15Ch] ; Move Scalar Single-FP
.text:00007FF74C3C98B8 028 83 FA 20                                      cmp     edx, 20h ; ' '  ; Compare Two Operands
.text:00007FF74C3C98BB 028 75 0A                                         jnz     short loc_7FF74C3C98C7 ; Jump if Not Zero (ZF=0)
.text:00007FF74C3C98BD 028 F3 0F 10 97 54 0A 02 00                       movss   xmm2, dword ptr [rdi+20A54h] ; Move Scalar Single-FP
.text:00007FF74C3C98C5 028 EB 08                                         jmp     short loc_7FF74C3C98CF ; Jump
.text:00007FF74C3C98C7                                   ; ---------------------------------------------------------------------------
.text:00007FF74C3C98C7
.text:00007FF74C3C98C7                                   loc_7FF74C3C98C7:                       ; CODE XREF: CFont2D__FitCharsToWidthW+8B↑j
.text:00007FF74C3C98C7 028 F3 0F 10 15 2D 13 A8 00                       movss   xmm2, cs:units  ; Move Scalar Single-FP
.text:00007FF74C3C98CF
.text:00007FF74C3C98CF                                   loc_7FF74C3C98CF:                       ; CODE XREF: CFont2D__FitCharsToWidthW+95↑j
.text:00007FF74C3C98CF 028 F3 42 0F 10 84 8F 80 00 00 00                 movss   xmm0, dword ptr [rdi+r9*4+80h] ; Move Scalar Single-FP
.text:00007FF74C3C98D9 028 F3 0F 59 87 44 0A 02 00                       mulss   xmm0, dword ptr [rdi+20A44h] ; Scalar Single-FP Multiply
.text:00007FF74C3C98E1 028 F3 42 0F 59 5C 80 0C                          mulss   xmm3, dword ptr [rax+r8*4+0Ch] ; Scalar Single-FP Multiply
.text:00007FF74C3C98E8 028 F3 0F 59 C3                                   mulss   xmm0, xmm3      ; Scalar Single-FP Multiply
.text:00007FF74C3C98EC 028 F3 0F 59 87 40 0A 02 00                       mulss   xmm0, dword ptr [rdi+20A40h] ; Scalar Single-FP Multiply
.text:00007FF74C3C98F4 028 F3 0F 59 C2                                   mulss   xmm0, xmm2      ; Scalar Single-FP Multiply
.text:00007FF74C3C98F8
.text:00007FF74C3C98F8                                   loc_7FF74C3C98F8:                       ; CODE XREF: CFont2D__FitCharsToWidthW+6B↑j
.text:00007FF74C3C98F8 028 F3 0F 58 C1                                   addss   xmm0, xmm1      ; Scalar Single-FP Add
.text:00007FF74C3C98FC 028 0F 2F C4                                      comiss  xmm0, xmm4      ; Scalar Ordered Single-FP Compare and Set EFLAGS
.text:00007FF74C3C98FF 028 0F 28 C8                                      movaps  xmm1, xmm0      ; Move Aligned Four Packed Single-FP
.text:00007FF74C3C9902 028 73 0E                                         jnb     short loc_7FF74C3C9912 ; Jump if Not Below (CF=0)
.text:00007FF74C3C9904 028 FF C3                                         inc     ebx             ; Increment by 1
.text:00007FF74C3C9906 028 3B D9                                         cmp     ebx, ecx        ; Compare Two Operands
.text:00007FF74C3C9908 028 0F 82 72 FF FF FF                             jb      loc_7FF74C3C9880 ; Jump if Below (CF=1)
.text:00007FF74C3C990E
.text:00007FF74C3C990E                                   loc_7FF74C3C990E:                       ; CODE XREF: CFont2D__FitCharsToWidthW+43↑j
.text:00007FF74C3C990E 028 8B C1                                         mov     eax, ecx
.text:00007FF74C3C9910 028 EB 06                                         jmp     short loc_7FF74C3C9918 ; Jump
.text:00007FF74C3C9912                                   ; ---------------------------------------------------------------------------
.text:00007FF74C3C9912
.text:00007FF74C3C9912                                   loc_7FF74C3C9912:                       ; CODE XREF: CFont2D__FitCharsToWidthW+D2↑j
.text:00007FF74C3C9912 028 8B C3                                         mov     eax, ebx
.text:00007FF74C3C9914 028 EB 02                                         jmp     short loc_7FF74C3C9918 ; Jump
.text:00007FF74C3C9916                                   ; ---------------------------------------------------------------------------
.text:00007FF74C3C9916
.text:00007FF74C3C9916                                   loc_7FF74C3C9916:                       ; CODE XREF: CFont2D__FitCharsToWidthW+22↑j
.text:00007FF74C3C9916                                                                           ; CFont2D__FitCharsToWidthW+2E↑j
.text:00007FF74C3C9916 028 33 C0                                         xor     eax, eax        ; Logical Exclusive OR
.text:00007FF74C3C9918
.text:00007FF74C3C9918                                   loc_7FF74C3C9918:                       ; CODE XREF: CFont2D__FitCharsToWidthW+E0↑j
.text:00007FF74C3C9918                                                                           ; CFont2D__FitCharsToWidthW+E4↑j
.text:00007FF74C3C9918 028 48 8B 5C 24 30                                mov     rbx, [rsp+28h+arg_0]
.text:00007FF74C3C991D 028 48 8B 6C 24 38                                mov     rbp, [rsp+28h+arg_8]
.text:00007FF74C3C9922 028 48 8B 74 24 40                                mov     rsi, [rsp+28h+arg_10]
.text:00007FF74C3C9927 028 48 83 C4 20                                   add     rsp, 20h        ; Add
.text:00007FF74C3C992B 008 5F                                            pop     rdi
.text:00007FF74C3C992C 000 C3                                            retn                    ; Return Near from Procedure
.text:00007FF74C3C992C                                   CFont2D__FitCharsToWidthW endp
*/
}
int patched_CFont2D_FitWordsToWidthW(CFont2D *t, const UChar *str, int w, float marg) {
    if (!str || !t->m_loadedV3)
        return 0;
    int  len = (int)wcslen(str);
    int  v9 = 0, v11 = 0, v12 = 0, v14 = 0;
    auto v15 = 0.0f;
    auto goalWidth = w - marg;
    bool v27 = false;
    for (; v11 <= len && !v27; ++v11) {
        auto v16 = 0.0f;
        for (; v11 < len; ++v11) {
            if (str[v11] == 32)
                break;
            if (str[v11] == 10)
                break;
        }
        while (str[v11] == 32)
            ++v11;
        UrsoftKerner uk0(*t);
        for (int i = 0; i < v11; i++) {
            auto ch = str[i];
            auto v21 = t->m_info.m_glyphIndexes[ch];
            if (v21 != 0xFFFF) {
                auto kernIdx = t->m_glyphs[v21].m_kernIdx;
                v16 += t->m_kern[kernIdx] * t->m_kerning * t->m_info.m_fileHdrV3.m_kern[kernIdx] * 
                    (t->m_glyphs[v21].m_width + uk0(ch)) * t->m_scale * (ch == 32 ? t->m_spaceScale : 1.0f);
            }
        }
        ++v14;
        if (str[v11] == 10) {
            if (goalWidth <= v16) {
                if (v14 == 1)
                    v12 = 1;
            } else {
                v9 = v11 + 1;
            }
            v27 = true;
            continue;
        }
        if (goalWidth <= v16) {
            if (v14 == 1) {
                auto v28 = 0;
                v16 = 0.0;
                if (v11) {
                    auto v29 = str;
                    UrsoftKerner uk(*t);
                    while (1) {
                        auto ch = *v29;
                        auto v30 = t->m_info.m_glyphIndexes[ch];
                        if (v30 != 0xFFFF) {
                            auto v33 = t->m_glyphs[v30].m_kernIdx;
                            v16 += t->m_kern[v33] * t->m_kerning * t->m_info.m_fileHdrV3.m_kern[v33] * 
                                (t->m_glyphs[v30].m_width + uk(ch)) * t->m_scale * (ch == 32 ? t->m_spaceScale : 1.0f);
                        }
                        if (v16 > goalWidth)
                            break;
                        ++v28;
                        ++v29;
                        if (v28 >= v11) {
                            v9 = v11;
                            v27 = true;
                            continue;
                        }
                    }
                    v11 = v28;
                }
                v9 = v11;
            }
            v27 = true;
            continue;
        }
        v27 = false;
        v9 = v11;
    }
    if (v12) {
        UrsoftKerner uk(*t);
        for (int v35 = 0; v35 < v11; ++v35) {
            auto v36 = str[v35];
            auto v37 = t->m_info.m_glyphIndexes[v36];
            if (v37 != 0xFFFF) {
                auto v40 = t->m_glyphs[v37].m_kernIdx;
                v15 += t->m_kern[v40] * t->m_kerning * t->m_info.m_fileHdrV3.m_kern[v40] * 
                    (t->m_glyphs[v37].m_width + uk(v36)) * t->m_scale * ((v36 == 32) ? t->m_spaceScale : 1.0f);
            }
            if (v15 > goalWidth) {
                v11 = v35;
                break;
            }
        }
        v9 = v11;
    }
    if (len < v9)
        v9 = len;
    if (v9)
        return v9;
    else
        return patched_CFont2D_FitCharsToWidthW(t, str, (int)goalWidth);
}
void patchCFont2D__FitWordsToWidthW() {
    char jump[]{ "\x48\x83\xEC\x30\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xD0\x48\x83\xC4\x30\xC3\x00\x00\x00\x00\x00\x00\x00\x00" }; //sub rsp, 0x30; mov rax, addr; call rax; add rsp, 0x30; ret
    *(void **)(jump + 6) = (void *)((char *)&patched_CFont2D_FitWordsToWidthW);
    PatchMainModule(
        "CFont2D.FitWordsToWidthW", 29,
        "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x48\x89\x78\x20\x41\x55\x41\x56\x41\x57\x48\x83\xEC\x30",
        jump
    );
    /*.text:00007FF74C3C9A74                                   ; int __fastcall CFont2D::FitWordsToWidthW(CFont2D *this, const UChar *str, int w, float a4)
.text:00007FF74C3C9A74                                   CFont2D__FitWordsToWidthW proc near     ; CODE XREF: UI_AudioControl__HUD_cbDrawAudioControl?+FF↑p
.text:00007FF74C3C9A74                                                                           ; DrawWorkoutList+3CBF↑p ...
.text:00007FF74C3C9A74
.text:00007FF74C3C9A74                                   var_28          = xmmword ptr -28h
.text:00007FF74C3C9A74                                   arg_0           = qword ptr  8
.text:00007FF74C3C9A74                                   arg_8           = qword ptr  10h
.text:00007FF74C3C9A74                                   arg_10          = qword ptr  18h
.text:00007FF74C3C9A74                                   arg_18          = qword ptr  20h
.text:00007FF74C3C9A74
.text:00007FF74C3C9A74 000 48 8B C4                                      mov     rax, rsp
.text:00007FF74C3C9A77 000 48 89 58 08                                   mov     [rax+8], rbx
.text:00007FF74C3C9A7B 000 48 89 68 10                                   mov     [rax+10h], rbp
.text:00007FF74C3C9A7F 000 48 89 70 18                                   mov     [rax+18h], rsi
.text:00007FF74C3C9A83 000 48 89 78 20                                   mov     [rax+20h], rdi
.text:00007FF74C3C9A87 000 41 55                                         push    r13
.text:00007FF74C3C9A89 008 41 56                                         push    r14
.text:00007FF74C3C9A8B 010 41 57                                         push    r15
.text:00007FF74C3C9A8D 018 48 83 EC 30                                   sub     rsp, 30h        ; Integer Subtraction
.text:00007FF74C3C9A91 048 0F 29 70 D8                                   movaps  xmmword ptr [rax-28h], xmm6 ; Move Aligned Four Packed Single-FP
.text:00007FF74C3C9A95 048 0F 28 F3                                      movaps  xmm6, xmm3      ; Move Aligned Four Packed Single-FP
.text:00007FF74C3C9A98 048 41 8B F0                                      mov     esi, r8d
.text:00007FF74C3C9A9B 048 48 8B FA                                      mov     rdi, rdx
.text:00007FF74C3C9A9E 048 48 8B D9                                      mov     rbx, rcx
.text:00007FF74C3C9AA1 048 48 85 D2                                      test    rdx, rdx        ; Logical Compare
.text:00007FF74C3C9AA4 048 0F 84 CF 02 00 00                             jz      loc_7FF74C3C9D79 ; Jump if Zero (ZF=1)
.text:00007FF74C3C9AAA 048 80 B9 59 0A 02 00 00                          cmp     byte ptr [rcx+20A59h], 0 ; Compare Two Operands
.text:00007FF74C3C9AB1 048 0F 84 C2 02 00 00                             jz      loc_7FF74C3C9D79 ; Jump if Zero (ZF=1)
.text:00007FF74C3C9AB7 048 48 8B CA                                      mov     rcx, rdx
.text:00007FF74C3C9ABA 048 E8 51 30 E3 FF                                call    u_strlen        ; Call Procedure
.text:00007FF74C3C9ABF 048 66 0F 6E E6                                   movd    xmm4, esi       ; Move 32 bits
.text:00007FF74C3C9AC3 048 45 33 C0                                      xor     r8d, r8d        ; Logical Exclusive OR
.text:00007FF74C3C9AC6 048 0F 5B E4                                      cvtdq2ps xmm4, xmm4     ; Convert Packed Doubleword Integers to Packed Double-Precision Floating-Point Values
.text:00007FF74C3C9AC9 048 33 D2                                         xor     edx, edx        ; Logical Exclusive OR
.text:00007FF74C3C9ACB 048 40 32 F6                                      xor     sil, sil        ; Logical Exclusive OR
.text:00007FF74C3C9ACE 048 8B E8                                         mov     ebp, eax
.text:00007FF74C3C9AD0 048 45 33 F6                                      xor     r14d, r14d      ; Logical Exclusive OR
.text:00007FF74C3C9AD3 048 45 8D 78 01                                   lea     r15d, [r8+1]    ; Load Effective Address
.text:00007FF74C3C9AD7 048 41 BD FF FF 00 00                             mov     r13d, 0FFFFh
.text:00007FF74C3C9ADD 048 0F 57 ED                                      xorps   xmm5, xmm5      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF74C3C9AE0 048 0F 57 DB                                      xorps   xmm3, xmm3      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF74C3C9AE3 048 F3 0F 5C E6                                   subss   xmm4, xmm6      ; Scalar Single-FP Subtract
.text:00007FF74C3C9AE7 048 F3 0F 10 35 0D 11 A8 00                       movss   xmm6, cs:units  ; Move Scalar Single-FP
.....
*/
}
bool new_CFont2D_NotSpace(const UChar *rdi, CFont2D *t, uint64_t r8_from11, uint64_t forceDrawMalloc) {
    bool ret = false;
    static CONTEXT context{};
    context.ContextFlags = CONTEXT_FLOATING_POINT;
    if (GetThreadContext(GetCurrentThread(), &context)) {
        float *ioCxXMM9 = reinterpret_cast<float *>(&context.Xmm9);
        float *ioCxXMM11 = reinterpret_cast<float *>(&context.Xmm11);
        UrsoftKerner &k = UrsoftKerner::stSelect(t);
        bool ret = false;
        if (*rdi != 32) {
            const auto &g = t->m_glyphs[t->m_info.m_glyphIndexes[*rdi]];
            ioCxXMM9[0] += k(*rdi) * t->m_info.m_fileHdrV3.m_kern[g.m_kernIdx] * ioCxXMM11[0];
            ret = true;
        } else if (t->m_cache && !forceDrawMalloc && t->m_cacheCntUsed) {
            ++t->m_curCache;
            t->m_cacheCntUsed--;
        }

        if (r8_from11 == 1) { //last symbol
            k.cleanup();
        }
        SetThreadContext(GetCurrentThread(), &context); //restore xmm1
    }
    return ret;
}
void patchCFont2D__RenderWString_u() {
    char jump[]{
                 "\x50\x51\x57\x59\x52\x48\x89\xda"          //push rax; push rcx; [arg1: push rdi; pop rcx;] push rdx; [arg2: mov rdx, rbx]
                 "\x41\x50\x4D\x89\xD8"                      //push r8; [arg3 mov r8, r11]
                 "\x41\x51\x4D\x89\xF9"                      //push r9; [arg4 mov r9, r15]
                 "\x41\x52\x41\x53"                          //push r10; push r11
                 "\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\x48\x83\xEC\x30\xFF\xD0\x48\x83\xC4\x30" //mov rax, addr; sub rsp, 0x30; call rax; add rsp, 0x30; 
                 "\x09\xC0"                                  //or eax,eax
                 "\x41\x5B\x41\x5A\x41\x59\x41\x58\x5A\x59\x58" //pop r11; pop r10; pop r9; pop r8; pop rdx; pop rcx; pop rax
                 "\xF3\x44\x0F\x10\xAC\x83\x5C\x01\x00\x00"  //movss   xmm13, dword ptr [rbx+rax*4+15Ch] ; m_kern[g.m_kernIdx]
                 "\xF3\x0F\x10\x4C\x24\x5C"                  //movss   xmm1, dword ptr[rsp + 128h + var_D8 + 0Ch]; Move Scalar Single - FP
                 "\xF3\x0F\x10\x83\x44\x0A\x02\x00"          //movss   xmm0, dword ptr[rbx + 20A44h]; this->m_kerning
                 "\xF3\x0F\x59\x84\x83\x80\x00\x00\x00"      //mulss   xmm0, dword ptr[rbx + rax * 4 + 80h]; Scalar Single - FP Multiply
                 "\xF3\x41\x0F\x59\xCD"                      //mulss   xmm1, xmm13; Scalar Single - FP Multiply
                 "\xF3\x41\x0F\x59\xCC"                      //mulss   xmm1, xmm12; Scalar Single - FP Multiply
                 "\xF3\x0F\x59\xC8"                          //mulss   xmm1, xmm0; Scalar Single - FP Multiply
                 "\xF3\x0F\x59\x8B\x54\x0A\x02\x00"          //mulss   xmm1, dword ptr [rbx+20A54h] ; xmm1 is dx
                 "\x0F\x85\x95\x01\x00\x00"   //jne $+xx
    };
    *(void **)(jump + 24) = (void *)((char *)&new_CFont2D_NotSpace);
    unsigned char find[] {
      0x66, 0x83, 0x3F, 0x20, 0xF3, 0x44, 0x0F, 0x10, 0xAC, 0x83,
      0x5C, 0x01, 0x00, 0x00, 0x75, 0x64, 0xF3, 0x0F, 0x10, 0x4C,
      0x24, 0x5C, 0xF3, 0x0F, 0x10, 0x83, 0x44, 0x0A, 0x02, 0x00,
      0xF3, 0x0F, 0x59, 0x84, 0x83, 0x80, 0x00, 0x00, 0x00, 0xF3,
      0x41, 0x0F, 0x59, 0xCD, 0xF3, 0x41, 0x0F, 0x59, 0xCC, 0xF3,
      0x0F, 0x59, 0xC8, 0xF3, 0x0F, 0x59, 0x8B, 0x54, 0x0A, 0x02,
      0x00, 0x4C, 0x39, 0xAB, 0x60, 0x0A, 0x02, 0x00, 0x0F, 0x84,
      0xBF, 0x01, 0x00, 0x00, 0x45, 0x84, 0xFF, 0x0F, 0x85, 0xB6,
      0x01, 0x00, 0x00, 0x8B, 0x83, 0x6C, 0x0A, 0x02, 0x00, 0x85,
      0xC0, 0x0F, 0x84, 0xA8, 0x01, 0x00, 0x00, 0xFF, 0xC8, 0x01,
      0xB3, 0x70, 0x0A, 0x02, 0x00, 0x89, 0x83, 0x6C, 0x0A, 0x02,
      0x00, 0xE9, 0x95, 0x01, 0x00, 0x00
    };
    static_assert(sizeof(jump) - 1 == sizeof(find));
    PatchMainModule("CFont2D.RenderWString_u", sizeof(jump) - 1, (char *)find, jump);
    /*
74C3CD022 128 66 83 3F 20                   cmp     word ptr [rdi], 20h ; ' ' ; [rdi]=ch
74C3CD026 128 F3 44 0F 10 AC 83 5C 01 00 00 movss   xmm13, dword ptr [rbx+rax*4+15Ch] ; m_kern[g.m_kernIdx]
74C3CD030 128 75 64                         jnz     short not_space ; Jump if Not Zero (ZF=0)
74C3CD032 128 F3 0F 10 4C 24 5C             movss   xmm1, dword ptr [rsp+128h+var_D8+0Ch] ; Move Scalar Single-FP
74C3CD038 128 F3 0F 10 83 44 0A 02 00       movss   xmm0, dword ptr [rbx+20A44h] ; this->m_kerning
74C3CD040 128 F3 0F 59 84 83 80 00 00 00    mulss   xmm0, dword ptr [rbx+rax*4+80h] ; Scalar Single-FP Multiply
74C3CD049 128 F3 41 0F 59 CD                mulss   xmm1, xmm13     ; Scalar Single-FP Multiply
74C3CD04E 128 F3 41 0F 59 CC                mulss   xmm1, xmm12     ; Scalar Single-FP Multiply
74C3CD053 128 F3 0F 59 C8                   mulss   xmm1, xmm0      ; Scalar Single-FP Multiply
74C3CD057 128 F3 0F 59 8B 54 0A 02 00       mulss   xmm1, dword ptr [rbx+20A54h] ; xmm1 is dx
74C3CD05F 128 4C 39 AB 60 0A 02 00          cmp     [rbx+20A60h], r13 ; this->m_cache
74C3CD066 128 0F 84 BF 01 00 00             jz      loc_7FF74C3CD22B ; Jump if Zero (ZF=1)
74C3CD06C 128 45 84 FF                      test    r15b, r15b      ; Logical Compare
74C3CD06F 128 0F 85 B6 01 00 00             jnz     loc_7FF74C3CD22B ; Jump if Not Zero (ZF=0)
74C3CD075 128 8B 83 6C 0A 02 00             mov     eax, [rbx+20A6Ch]
74C3CD07B 128 85 C0                         test    eax, eax        ; Logical Compare
74C3CD07D 128 0F 84 A8 01 00 00             jz      loc_7FF74C3CD22B ; Jump if Zero (ZF=1)
74C3CD083 128 FF C8                         dec     eax             ; Decrement by 1
74C3CD085 128 01 B3 70 0A 02 00             add     [rbx+20A70h], esi ; Add
74C3CD08B 128 89 83 6C 0A 02 00             mov     [rbx+20A6Ch], eax
74C3CD091 128 E9 95 01 00 00                jmp     loc_7FF74C3CD22B ; Jump
74C3CD096                                   ; ---------------------------------------------------------------------------
74C3CD096
74C3CD096                                   not_space:                              ; CODE XREF: CFont2D__RenderWString_u+31C↑j
превращаем в
74C3CD026 128 F3 44 0F 10 AC 83 5C 01 00 00 movss   xmm13, dword ptr [rbx+rax*4+15Ch] ; m_kern[g.m_kernIdx]
74C3CD032 128 F3 0F 10 4C 24 5C             movss   xmm1, dword ptr [rsp+128h+var_D8+0Ch] ; Move Scalar Single-FP
74C3CD038 128 F3 0F 10 83 44 0A 02 00       movss   xmm0, dword ptr [rbx+20A44h] ; this->m_kerning
74C3CD040 128 F3 0F 59 84 83 80 00 00 00    mulss   xmm0, dword ptr [rbx+rax*4+80h] ; Scalar Single-FP Multiply
74C3CD049 128 F3 41 0F 59 CD                mulss   xmm1, xmm13     ; Scalar Single-FP Multiply
74C3CD04E 128 F3 41 0F 59 CC                mulss   xmm1, xmm12     ; Scalar Single-FP Multiply
74C3CD053 128 F3 0F 59 C8                   mulss   xmm1, xmm0      ; Scalar Single-FP Multiply
74C3CD057 128 F3 0F 59 8B 54 0A 02 00       mulss   xmm1, dword ptr [rbx+20A54h] ; xmm1 is dx
это было dx = g.m_view.m_width * v42 * v17 * m_kerning * m_kern[g.m_kernIdx] * m_spaceScale;
теперь у нас есть возможность заменить остаток на 
if (CFont2D::NotSpace(CFont *t=rbx, const wchar_t *rdi, keep float xmm1_4=dx, inout xmm9_4=cx)) {not_space branch}
74C3CD022 128 66 83 3F 20                   cmp     word ptr [rdi], 20h ; ' ' ; [rdi]=ch
74C3CD030 128 75 64                         jnz     short not_space ; Jump if Not Zero (ZF=0)
74C3CD05F 128 4C 39 AB 60 0A 02 00          cmp     [rbx+20A60h], r13 ; this->m_cache
74C3CD066 128 0F 84 BF 01 00 00             jz      loc_7FF74C3CD22B ; Jump if Zero (ZF=1)
74C3CD06C 128 45 84 FF                      test    r15b, r15b      ; Logical Compare
74C3CD06F 128 0F 85 B6 01 00 00             jnz     loc_7FF74C3CD22B ; Jump if Not Zero (ZF=0)
74C3CD075 128 8B 83 6C 0A 02 00             mov     eax, [rbx+20A6Ch]
74C3CD07B 128 85 C0                         test    eax, eax        ; Logical Compare
74C3CD07D 128 0F 84 A8 01 00 00             jz      loc_7FF74C3CD22B ; Jump if Zero (ZF=1)
74C3CD083 128 FF C8                         dec     eax             ; Decrement by 1
74C3CD085 128 01 B3 70 0A 02 00             add     [rbx+20A70h], esi ; Add
74C3CD08B 128 89 83 6C 0A 02 00             mov     [rbx+20A6Ch], eax
74C3CD091 128 E9 95 01 00 00                jmp     loc_7FF74C3CD22B ; Jump
74C3CD096                                   ; ---------------------------------------------------------------------------
74C3CD096
74C3CD096                                   not_space:                              ; CODE XREF: CFont2D__RenderWString_u+31C↑j

*/
    //::MessageBoxA(NULL, "patchCFont2D__RenderWString_u", "Zwift.AntDll", 0);
}
#pragma pack(push, 1)
struct RealKernItem { //5 bytes
    UChar m_prev, m_cur; // for example, AV pair needs V correction to the left
    char m_corr;         // corr amount (pixels)
};
#pragma pack(pop)
void loadKerns(const char *binFileName, std::map<std::pair<UChar, UChar>, float> *pDest) {
    FILE *f = fopen(binFileName, "rb");
    if (f) {
        uint16_t ver;
        fread(&ver, sizeof(ver), 1, f);
        if (ver == 0x100) {
            return;
        } else {
            if (ver == 0x300) {
                //inlined CFont2D::LoadFontV3(name):
                static_assert(sizeof(CFont2D_fileHdrV3) == 0xF0);
                CFont2D_fileHdrV3 h;
                fread(((uint8_t *)&h) + sizeof(ver), sizeof(CFont2D_fileHdrV3) - sizeof(ver), 1, f);
                fseek(f, sizeof(CFont2D_glyph) * h.m_charsCnt, SEEK_CUR);
                static_assert(sizeof(RealKernItem) == 5);
                RealKernItem rki;
                for (int k = 0; k < h.m_realKerns; k++) {
                    fread(&rki, sizeof(RealKernItem), 1, f);
                    bool prevNotDigit = rki.m_prev < '0' || rki.m_prev > '9';
                    bool curNotDigit = rki.m_cur < '0' || rki.m_cur > '9';
                    if (prevNotDigit && curNotDigit)
                        (*pDest)[std::make_pair(rki.m_prev, rki.m_cur)] = rki.m_corr / 2048.0f;
                    else if (!prevNotDigit && (rki.m_cur == '.' || rki.m_cur == ':'))
                        (*pDest)[std::make_pair(rki.m_prev, rki.m_cur)] = 4 / 2048.0f;
                }
            } else {
                //Log("Unknown font version number from %s(%x)\n", name_, ver);
            }
        }
    }
    if (f)
        fclose(f);
}
enum RenderFlags {
    RF_CX_ISCENTER = 0x1,
    RF_CY_ISCENTER = 0x2,
    RF_CX_ISRIGHT = 0x4,
    RF_CY_ISBOTTOM = 0x8,
};
struct VEC2 { float m_data[2]; };
int CFont2D::GetParagraphLineCountW(float w, const UChar *str, float scale, float marg, bool unlim) {
    if (!m_loadedV3 || !str || !*str)
        return 0;
    int len = (int)wcslen(str);
    if (!unlim && len > 0x400)
        return 1;
    int ret = 0, wordsLen;
    for (auto *end = str + len; str < end; str += wordsLen) {
        scale = 1.0f / scale;
        wordsLen = patched_CFont2D_FitWordsToWidthW(this, str, int(scale * w), scale * marg);
        marg = 0.0f;
        if (!wordsLen)
            wordsLen = len;
        ++ret;
    }
    return ret;
}
int patchedCFont2D__GetParagraphIndexByPosition(CFont2D *t, float cx, float cy, float w, float h, UChar *str, RenderFlags flags, float scaleW, float scaleH, float *ox, float *oy) {
    int ret = 0;
    if (t->m_loadedV3 && str) {
        auto _str = str;
        ret = (int)wcslen(str);
        auto oy_ = 0.0f;
        auto v18 = t->GetHeight() * scaleW * t->m_field_20A3C * scaleH, fw = 1.0f / scaleW * w;
        if (flags & RF_CY_ISCENTER)
            oy_ = h * 0.5f - t->GetParagraphLineCountW(w, str, scaleW, 0.0f, false) * v18 * 0.5f;
        if (h > oy_) {
            while (_str < str + ret) {
                auto eatChars = patched_CFont2D_FitWordsToWidthW(t, _str, int(fw), 0.0f);
                auto ox_ = 0.0f;
                *oy = oy_;
                *ox = ox_;
                UrsoftKerner uk(*t);
                for (int v20 = 0; v20 < eatChars; v20++) {
                    auto ch = _str[v20];
                    auto v24 = t->m_info.m_glyphIndexes[ch];
                    if (v24 != 0xFFFF) {
                        auto v25 = ox_;
                        auto v27 = t->m_glyphs[v24].m_kernIdx;
                        ox_ += t->m_kern[v27] * t->m_kerning * (t->m_glyphs[v24].m_width + uk(ch)) * t->m_info.m_fileHdrV3.m_kern[v27] * scaleW * 
                            t->m_scale * ((ch == 32) ? t->m_spaceScale : 1.0f);
                        if (ox_ > cx && cx > v25 && cy > oy_ && (oy_ + v18) > cy) {
                            *ox = v25;
                            *oy = oy_;
                            return int(_str - str) + v20;
                        }
                    }
                }
                auto v29 = oy_ + v18;
                if (cy > oy_ && v29 > cy) {
                    *ox = ox_;
                    *oy = oy_;
                    return int(_str - str) + (eatChars - 1);
                }
                _str += eatChars;
                *ox = ox_;
                *oy = oy_;
                if (h <= v29)
                    return ret;
            }
        }
    }
    return ret;
}
VEC2 *patchedCFont2D__GetParagraphPositionByIndexW(CFont2D *t /*rcx*/, VEC2 *ret /*rdx*/, int index /*r8d*/,
    float w /*xmm3*/, float h, UChar *str, int a7, float mult, float scale) {
    if (t->m_loadedV3 && str != nullptr && *str) {
        auto _str = str;
        auto y = 0.0f, lh = t->GetHeight() * mult * t->m_field_20A3C * scale;
        w *= 1.0f / mult;
        int i = 0;
        do {
            auto v19 = patched_CFont2D_FitWordsToWidthW(t, _str, int(w), 0.0f);
            auto v21 = v19 + i;
            if (v21 >= index)
                break;
            for (int v23 = v19; v23; --v23)
                if (*_str++ == 10)
                    y += lh;
            i = v21;
            if (_str[-1] != 10)
                y += lh;
        } while (*_str);
        auto v24 = mult * t->m_scale * t->m_kerning;
        UrsoftKerner uk(*t);
        while (i < index && *_str) {
            ++i;
            auto v26 = *_str++;
            auto v27 = t->m_info.m_glyphIndexes[v26];
            if (v27 != 0xFFFF) {
                if (v26 == 10) {
                    ret->m_data[0] = 0.0f;
                    y += lh;
                } else {
                    auto v29 = t->m_glyphs[v27].m_kernIdx;
                    ret->m_data[0] += t->m_kern[v29] * (t->m_glyphs[v27].m_width + uk(v26)) * 
                        t->m_info.m_fileHdrV3.m_kern[v29] * v24 * ((v26 == 32) ? t->m_spaceScale : 1.0f);
                }
            }
        }
        ret->m_data[1] = y;
    }
    return ret;
}
void patchCFont2D__GetParagraphIndexByPosition() {
    char jump[]{ "\x48\x83\xEC\x00\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0\x48\x83\xC4\x00\xC3" }; //sub rsp, 0x00; mov rax, addr; jmp rax; add rsp, 0x00; ret
    *(void **)(jump + 6) = (void *)((char *)&patchedCFont2D__GetParagraphIndexByPosition);
    PatchMainModule(
        "CFont2D.GetParagraphIndexByPosition", 32,
        "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x48\x89\x78\x20\x41\x54\x41\x56\x41\x57\x48\x81\xEC\xD0\x00\x00\x00",
        jump
    );
    /* 
.text:00007FF7FD9AA248                                   ; __int64 __fastcall CFont2D::GetParagraphIndexByPosition(CFont2D *this, float _cx, float cy, float w, float h, UChar *str, RenderFlags flags, float scaleW, float scaleH, float *ox, float *oy)
.text:00007FF7FD9AA248                                   CFont2D__GetParagraphIndexByPosition proc near
.text:00007FF7FD9AA248                                                                           ; CODE XREF: GUI_EditBox__OnMouseDown+2A2↓p
.text:00007FF7FD9AA248                                                                           ; GUI_EditBox__OnMouseMove+2AC↓p ...
.text:00007FF7FD9AA248
.text:00007FF7FD9AA248                                   marg            = dword ptr -0C8h
.text:00007FF7FD9AA248                                   unlim           = byte ptr -0C0h
.text:00007FF7FD9AA248                                   var_B8          = xmmword ptr -0B8h
.text:00007FF7FD9AA248                                   var_98          = xmmword ptr -98h
.text:00007FF7FD9AA248                                   var_88          = xmmword ptr -88h
.text:00007FF7FD9AA248                                   var_18          = byte ptr -18h
.text:00007FF7FD9AA248                                   arg_20          = dword ptr  28h
.text:00007FF7FD9AA248                                   str             = qword ptr  30h
.text:00007FF7FD9AA248                                   arg_30          = byte ptr  38h
.text:00007FF7FD9AA248                                   scale           = dword ptr  40h
.text:00007FF7FD9AA248                                   arg_40          = dword ptr  48h
.text:00007FF7FD9AA248                                   arg_48          = qword ptr  50h
.text:00007FF7FD9AA248                                   arg_50          = qword ptr  58h
.text:00007FF7FD9AA248
.text:00007FF7FD9AA248 000 48 8B C4                                      mov     rax, rsp
.text:00007FF7FD9AA24B 000 48 89 58 08                                   mov     [rax+8], rbx
.text:00007FF7FD9AA24F 000 48 89 68 10                                   mov     [rax+10h], rbp
.text:00007FF7FD9AA253 000 48 89 70 18                                   mov     [rax+18h], rsi
.text:00007FF7FD9AA257 000 48 89 78 20                                   mov     [rax+20h], rdi
.text:00007FF7FD9AA25B 000 41 54                                         push    r12
.text:00007FF7FD9AA25D 008 41 56                                         push    r14
.text:00007FF7FD9AA25F 010 41 57                                         push    r15
.text:00007FF7FD9AA261 018 48 81 EC D0 00 00 00                          sub     rsp, 0D0h       ; Integer Subtraction
.text:00007FF7FD9AA268 0E8 80 B9 59 0A 02 00 00                          cmp     byte ptr [rcx+20A59h], 0 ; Compare Two Operands
.text:00007FF7FD9AA26F 0E8 48 8B F9                                      mov     rdi, rcx
.text:00007FF7FD9AA272 0E8 0F 29 70 D8                                   movaps  xmmword ptr [rax-28h], xmm6 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA276 0E8 0F 29 78 C8                                   movaps  xmmword ptr [rax-38h], xmm7 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA27A 0E8 44 0F 29 40 B8                                movaps  xmmword ptr [rax-48h], xmm8 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA27F 0E8 44 0F 29 48 A8                                movaps  xmmword ptr [rax-58h], xmm9 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA284 0E8 44 0F 29 50 98                                movaps  xmmword ptr [rax-68h], xmm10 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA289 0E8 44 0F 29 58 88                                movaps  xmmword ptr [rax-78h], xmm11 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA28E 0E8 44 0F 28 DA                                   movaps  xmm11, xmm2     ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA292 0E8 44 0F 29 6C 24 60                             movaps  [rsp+0E8h+var_88], xmm13 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA298 0E8 44 0F 28 EB                                   movaps  xmm13, xmm3     ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA29C 0E8 44 0F 29 74 24 50                             movaps  [rsp+0E8h+var_98], xmm14 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA2A2 0E8 44 0F 28 F1                                   movaps  xmm14, xmm1     ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA2A6 0E8 0F 84 4A 02 00 00                             jz      loc_7FF7FD9AA4F6 ; Jump if Zero (ZF=1)
.text:00007FF7FD9AA2AC 0E8 48 8B 9C 24 18 01 00 00                       mov     rbx, [rsp+0E8h+str]
.text:00007FF7FD9AA2B4 0E8 48 85 DB                                      test    rbx, rbx        ; Logical Compare
.text:00007FF7FD9AA2B7 0E8 0F 84 39 02 00 00                             jz      loc_7FF7FD9AA4F6 ; Jump if Zero (ZF=1)
.text:00007FF7FD9AA2BD 0E8 F3 0F 10 B4 24 28 01 00 00                    movss   xmm6, [rsp+0E8h+scale] ; Move Scalar Single-FP
.text:00007FF7FD9AA2C6 0E8 48 8B F3                                      mov     rsi, rbx
.text:00007FF7FD9AA2C9 0E8 F3 0F 59 B1 3C 0A 02 00                       mulss   xmm6, dword ptr [rcx+20A3Ch] ; Scalar Single-FP Multiply
.text:00007FF7FD9AA2D1 0E8 48 8B CB                                      mov     rcx, rbx
.text:00007FF7FD9AA2D4 0E8 E8 37 28 E3 FF                                call    u_strlen        ; Call Procedure
.text:00007FF7FD9AA2D9 0E8 48 8B CF                                      mov     rcx, rdi        ; this
    */
}
void patchCFont2D__GetParagraphPositionByIndexW() {
    char jump[]{ "\x48\x83\xEC\x00\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xE0\x48\x83\xC4\x00\xC3\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" }; //sub rsp, 0x00; mov rax, addr; jmp rax; add rsp, 0x00; ret
    *(void **)(jump + 6) = (void *)((char *)&patchedCFont2D__GetParagraphPositionByIndexW);
    PatchMainModule(
        "CFont2D.GetParagraphPositionByIndexW", 37,
        "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x81\xEC\x90\x00\x00\x00\x0F\x29\x70\xC8\x33\xDB",
        jump
    );
    /* 
.text:00007FF7FD9AA6C4                                   ; VEC2 *__fastcall CFont2D::GetParagraphPositionByIndexW(CFont2D *this, VEC2 *ret, int a3, float w, int a5_skipped, UChar *str, int a7_skipped, float mult, float scale)
.text:00007FF7FD9AA6C4                                   CFont2D__GetParagraphPositionByIndexW proc near
.text:00007FF7FD9AA6C4                                                                           ; CODE XREF: GUI_EditBox__OnKey+4A2↓p
.text:00007FF7FD9AA6C4                                                                           ; DATA XREF: .pdata:00007FF7FEA053EC↓o
.text:00007FF7FD9AA6C4
.text:00007FF7FD9AA6C4                                   var_98          = xmmword ptr -98h
.text:00007FF7FD9AA6C4                                   var_48          = xmmword ptr -48h
.text:00007FF7FD9AA6C4                                   var_28          = byte ptr -28h
.text:00007FF7FD9AA6C4                                   a5_skipped      = dword ptr  28h
.text:00007FF7FD9AA6C4                                   str             = qword ptr  30h
.text:00007FF7FD9AA6C4                                   a7_skipped      = dword ptr  38h
.text:00007FF7FD9AA6C4                                   mult            = dword ptr  40h
.text:00007FF7FD9AA6C4                                   scale           = dword ptr  48h
.text:00007FF7FD9AA6C4
.text:00007FF7FD9AA6C4 000 48 8B C4                                      mov     rax, rsp
.text:00007FF7FD9AA6C7 000 48 89 58 08                                   mov     [rax+8], rbx
.text:00007FF7FD9AA6CB 000 48 89 68 10                                   mov     [rax+10h], rbp
.text:00007FF7FD9AA6CF 000 48 89 70 18                                   mov     [rax+18h], rsi
.text:00007FF7FD9AA6D3 000 57                                            push    rdi
.text:00007FF7FD9AA6D4 008 41 54                                         push    r12
.text:00007FF7FD9AA6D6 010 41 55                                         push    r13
.text:00007FF7FD9AA6D8 018 41 56                                         push    r14
.text:00007FF7FD9AA6DA 020 41 57                                         push    r15
.text:00007FF7FD9AA6DC 028 48 81 EC 90 00 00 00                          sub     rsp, 90h        ; Integer Subtraction
.text:00007FF7FD9AA6E3 0B8 0F 29 70 C8                                   movaps  xmmword ptr [rax-38h], xmm6 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA6E7 0B8 33 DB                                         xor     ebx, ebx        ; Logical Exclusive OR
.text:00007FF7FD9AA6E9 0B8 45 8B E8                                      mov     r13d, r8d
.text:00007FF7FD9AA6EC 0B8 0F 29 78 B8                                   movaps  xmmword ptr [rax-48h], xmm7 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA6F0 0B8 48 8B FA                                      mov     rdi, rdx
.text:00007FF7FD9AA6F3 0B8 44 0F 29 40 A8                                movaps  xmmword ptr [rax-58h], xmm8 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA6F8 0B8 44 0F 28 C3                                   movaps  xmm8, xmm3      ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA6FC 0B8 44 0F 29 48 98                                movaps  xmmword ptr [rax-68h], xmm9 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA701 0B8 48 8B E9                                      mov     rbp, rcx
.text:00007FF7FD9AA704 0B8 44 0F 29 50 88                                movaps  xmmword ptr [rax-78h], xmm10 ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA709 0B8 38 99 59 0A 02 00                             cmp     [rcx+20A59h], bl ; Compare Two Operands
.text:00007FF7FD9AA70F 0B8 0F 84 AC 01 00 00                             jz      loc_7FF7FD9AA8C1 ; Jump if Zero (ZF=1)
.text:00007FF7FD9AA715 0B8 48 8B B4 24 E8 00 00 00                       mov     rsi, [rsp+0B8h+str]
.text:00007FF7FD9AA71D 0B8 48 85 F6                                      test    rsi, rsi        ; Logical Compare
.text:00007FF7FD9AA720 0B8 0F 84 9B 01 00 00                             jz      loc_7FF7FD9AA8C1 ; Jump if Zero (ZF=1)
.text:00007FF7FD9AA726 0B8 44 8B F3                                      mov     r14d, ebx
.text:00007FF7FD9AA729 0B8 0F 57 F6                                      xorps   xmm6, xmm6      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF7FD9AA72C 0B8 E8 E7 FA FF FF                                call    CFont2D__GetHeight ; Call Procedure
.text:00007FF7FD9AA731 0B8 F3 44 0F 10 8C 24 F8 00 00 00                 movss   xmm9, [rsp+0B8h+mult] ; Move Scalar Single-FP
.text:00007FF7FD9AA73B 0B8 0F 28 F8                                      movaps  xmm7, xmm0      ; Move Aligned Four Packed Single-FP
.text:00007FF7FD9AA73E 0B8 41 0F 28 C9                                   movaps  xmm1, xmm9      ; Move Aligned Four Packed Single-FP
    */
}
struct RECT2 {
    float m_left;
    float m_top;
    float m_width;
    float m_height;
};
struct GUI_Obj {
    void *m_vptr;
    RECT2 m_pos;
    float m_xvo;
    float m_yvo;
    int m_flag;
    float m_padding;
    float m_bordLeft;
    float m_bordTop;
    float m_bordRight;
    float m_bordBottom;
    char field_38[16];
    char *m_mouseOverSid;
    char *m_toggleOnSid;
    char *m_toggleOffSid;
    char *m_selectSid;
    char field_68[4];
    int m_z;
    char m_field_70;
    char m_field_71;
    char m_visible;
    char m_disabled;
    char m_field_74;
    char m_field_75;
    char m_field_76;
    char field_77;
    CFont2D *m_font;
    GUI_Obj *m_parent;
    char field_88[24];
    int m_borderStyle;
    char field_A4[4];
    void *m_wtext5;
    UChar *m_wtext2;
    int m_wtextLen;
    char m_hasText;
    char field_BD[7];
    int m_field_C4;
    int m_field_C8;
    char field_CC[12];
    int m_field_D8;
    char field_DC[20];
    float GetX() {
        if (m_parent)
            return m_parent->GetX() + m_pos.m_left;
        else
            return m_pos.m_left;
    }
};
struct GUI_EditBox : public GUI_Obj {
    char field_F0[4];
    float m_field_F4;
    char field_F8[16];
    int m_align;
    int m_flags;
    char field_110[96];
    UChar *m_grayString;
    UChar *m_string;
    char field_180[48];
    float m_field_1B0;
    char field_1B4[4];
    float m_cursorPos;
    float field_1BC;
    float m_field_1C0;
    int m_cursorIndex;
    int m_scrolled;
    char field_1CC[24];
    int m_stringLen;
    char field_1E8[80];
    float GetTextScale(UChar *string, unsigned int strLen) {
        float scale = m_field_1B0 * m_field_F4;
        if (m_flags & 0x80000) {
            float w = m_pos.m_width - 8.0f;
            float need_w = 0.0f;
            if (string != m_grayString) {
                need_w = CFont2D::StringWidthW_ulen(m_font, string, strLen) * scale;
            }
            if (need_w > w) {
                return (w / need_w) * scale;
            }
        }
        return scale;
    }
    float CalculateCursorOffsetForAlignment(char align, UChar *str, unsigned int strLen, float scale) {
        if (align & 1)
            return (m_pos.m_width
                - CFont2D::StringWidthW_ulen(m_font, str, strLen) * scale) * 0.5f;
        if (align & 4)
            return m_pos.m_width
                - (CFont2D::StringWidthW_ulen(m_font, str, strLen) * scale + m_field_1C0);
        return 0.0f;
    }
};
int patchedGUI_EditBox__CalculateCursorIndexFromPosition(GUI_EditBox *t, float pos) {
    auto scale = t->GetTextScale(t->m_string, t->m_stringLen);
    auto _offset = t->CalculateCursorOffsetForAlignment(t->m_align, t->m_string, t->m_stringLen, scale);
    auto max_w = (float)(pos - t->GetX()) - _offset;
    auto chr_w = 0.0f;
    auto ch_idx = t->m_scrolled;
    if (ch_idx >= t->m_stringLen)
        return ch_idx;
    UrsoftKerner uk(*t->m_font);
    do {
        auto chr = (t->m_flags & 8) ? '*' : t->m_string[ch_idx]; //8: password field
        chr_w += t->m_font->CharWidthW(chr, uk(chr)) * scale;
        if (chr_w >= max_w)
            break;
        ++ch_idx;
    } while (ch_idx < t->m_stringLen);
    return ch_idx;
}
void patchGUI_EditBox__CalculateCursorIndexFromPosition() {
    char jump[]{ "\x48\x83\xEC\x20\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xD0\x48\x83\xC4\x20\xC3\0\0\0\0\0\0\0\0\0\0\0\0" }; //sub rsp, 0x20; mov rax, addr; call rax; add rsp, 0x20; ret
    *(void **)(jump + 6) = (void *)((char *)&patchedGUI_EditBox__CalculateCursorIndexFromPosition);
    PatchMainModule(
        "GUI_EditBox.CalculateCursorIndexFromPosition", 33,
        "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x70\x10\x57\x48\x83\xEC\x60\x44\x8B\x81\xE4\x01\x00\x00\x48\x8B\xD9\x48\x8B\x91\x78\x01\x00\x00",
        jump
    );
    /* 
.text:00007FF706ECFBA0                                   ; __int64 __fastcall GUI_EditBox::CalculateCursorIndexFromPosition(GUI_EditBox *this, float pos)
.text:00007FF706ECFBA0                                   GUI_EditBox__CalculateCursorIndexFromPosition proc near
.text:00007FF706ECFBA0                                                                           ; CODE XREF: GUI_EditBox__OnMouseDown+335↓p
.text:00007FF706ECFBA0                                                                           ; GUI_EditBox__OnMouseMove+3B7↓p
.text:00007FF706ECFBA0                                                                           ; DATA XREF: ...
.text:00007FF706ECFBA0
.text:00007FF706ECFBA0                                   var_48          = dword ptr -48h
.text:00007FF706ECFBA0                                   var_38          = xmmword ptr -38h
.text:00007FF706ECFBA0                                   var_28          = xmmword ptr -28h
.text:00007FF706ECFBA0                                   var_18          = xmmword ptr -18h
.text:00007FF706ECFBA0                                   arg_0           = qword ptr  8
.text:00007FF706ECFBA0                                   arg_8           = qword ptr  10h
.text:00007FF706ECFBA0
.text:00007FF706ECFBA0 000 48 8B C4                                      mov     rax, rsp
.text:00007FF706ECFBA3 000 48 89 58 08                                   mov     [rax+8], rbx
.text:00007FF706ECFBA7 000 48 89 70 10                                   mov     [rax+10h], rsi
.text:00007FF706ECFBAB 000 57                                            push    rdi
.text:00007FF706ECFBAC 008 48 83 EC 60                                   sub     rsp, 60h        ; Integer Subtraction
.text:00007FF706ECFBB0 068 44 8B 81 E4 01 00 00                          mov     r8d, [rcx+1E4h]
.text:00007FF706ECFBB7 068 48 8B D9                                      mov     rbx, rcx
.text:00007FF706ECFBBA 068 48 8B 91 78 01 00 00                          mov     rdx, [rcx+178h]
.text:00007FF706ECFBC1 068 0F 29 70 E8                                   movaps  xmmword ptr [rax-18h], xmm6 ; Move Aligned Four Packed Single-FP
.text:00007FF706ECFBC5 068 0F 29 78 D8                                   movaps  xmmword ptr [rax-28h], xmm7 ; Move Aligned Four Packed Single-FP
.text:00007FF706ECFBC9 068 0F 28 F9                                      movaps  xmm7, xmm1      ; Move Aligned Four Packed Single-FP
.text:00007FF706ECFBCC 068 44 0F 29 40 C8                                movaps  xmmword ptr [rax-38h], xmm8 ; Move Aligned Four Packed Single-FP
.text:00007FF706ECFBD1 068 E8 FE 05 00 00                                call    GUI_EditBox__GetTextScale ; Call Procedure
.text:00007FF706ECFBD6 068 44 8B 8B E4 01 00 00                          mov     r9d, [rbx+1E4h]
.text:00007FF706ECFBDD 068 44 0F 28 C0                                   movaps  xmm8, xmm0      ; Move Aligned Four Packed Single-FP
.text:00007FF706ECFBE1 068 4C 8B 83 78 01 00 00                          mov     r8, [rbx+178h]
.text:00007FF706ECFBE8 068 48 8B CB                                      mov     rcx, rbx
.text:00007FF706ECFBEB 068 8B 93 08 01 00 00                             mov     edx, [rbx+108h]
.text:00007FF706ECFBF1 068 F3 44 0F 11 44 24 20                          movss   [rsp+68h+var_48], xmm8 ; Move Scalar Single-FP
.text:00007FF706ECFBF8 068 E8 93 00 00 00                                call    GUI_EditBox__CalculateCursorOffsetForAlignment ; Call Procedure
.text:00007FF706ECFBFD 068 48 8B CB                                      mov     rcx, rbx
.text:00007FF706ECFC00 068 0F 28 F0                                      movaps  xmm6, xmm0      ; Move Aligned Four Packed Single-FP
.text:00007FF706ECFC03 068 E8 F8 91 00 00                                call    GUI_Obj__GetX   ; Call Procedure
.text:00007FF706ECFC08 068 48 63 BB C8 01 00 00                          movsxd  rdi, dword ptr [rbx+1C8h] ; Move with Sign-Extend Doubleword
.text:00007FF706ECFC0F 068 F3 0F 5C F8                                   subss   xmm7, xmm0      ; Scalar Single-FP Subtract
.text:00007FF706ECFC13 068 F3 0F 5C FE                                   subss   xmm7, xmm6      ; Scalar Single-FP Subtract
.text:00007FF706ECFC17 068 0F 57 F6                                      xorps   xmm6, xmm6      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF706ECFC1A 068 3B BB E4 01 00 00                             cmp     edi, [rbx+1E4h] ; Compare Two Operands
.text:00007FF706ECFC20 068 7D 4A                                         jge     short loc_7FF706ECFC6C ; Jump if Greater or Equal (SF=OF)
.text:00007FF706ECFC22 068 48 8D 34 3F                                   lea     rsi, [rdi+rdi]  ; Load Effective Address
    */
}
void patchedGUI_EditBox__CalcCursorPositionByIndex(GUI_EditBox *t, float scale) {
    if ((t->m_flags & 0x800) == 0) {
        int chIdxMax = t->m_scrolled, chIdx = t->m_scrolled;
        t->m_cursorPos = 0.0;
        if (t->m_cursorIndex < t->m_scrolled)
            chIdx = t->m_cursorIndex;
        if (t->m_cursorIndex > t->m_scrolled)
            chIdxMax = t->m_cursorIndex;
        UrsoftKerner uk(*t->m_font);
        while (chIdx < chIdxMax) {
            UChar chr = (t->m_flags & 8) ? L'*' : t->m_string[chIdx];
            ++chIdx;
            t->m_cursorPos += t->m_font->CharWidthW(chr, uk(chr)) * scale;
        }
    }
}
void patchGUI_EditBox__CalcCursorPositionByIndex() {
    char jump[]{ "\x48\x83\xEC\x20\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xD0\x48\x83\xC4\x20\xC3\0\0\0\0\0\0\0\0\0\0\0\0" }; //sub rsp, 0x20; mov rax, addr; call rax; add rsp, 0x20; ret
    *(void **)(jump + 6) = (void *)((char *)&patchedGUI_EditBox__CalcCursorPositionByIndex);
    PatchMainModule(
        "GUI_EditBox.CalcCursorPositionByIndex", 33,
        "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x30\xF7\x81\x0C\x01\x00\x00\x00\x08\x00\x00\x48\x8B\xD9\x0F\x29\x74\x24\x20",
        jump
    );
    /* 
.text:00007FF706ECFAF8                                   ; void __fastcall GUI_EditBox::CalcCursorPositionByIndex(GUI_EditBox *this, float scale)
.text:00007FF706ECFAF8                                   GUI_EditBox__CalcCursorPositionByIndex proc near
.text:00007FF706ECFAF8                                                                           ; CODE XREF: GUI_EditBox__Render+B16↓p
.text:00007FF706ECFAF8                                                                           ; DATA XREF: .pdata:00007FF707F06820↓o ...
.text:00007FF706ECFAF8
.text:00007FF706ECFAF8                                   var_18          = xmmword ptr -18h
.text:00007FF706ECFAF8                                   arg_0           = qword ptr  8
.text:00007FF706ECFAF8                                   arg_8           = qword ptr  10h
.text:00007FF706ECFAF8
.text:00007FF706ECFAF8 000 48 89 5C 24 08                                mov     [rsp+arg_0], rbx
.text:00007FF706ECFAFD 000 48 89 74 24 10                                mov     [rsp+arg_8], rsi
.text:00007FF706ECFB02 000 57                                            push    rdi
.text:00007FF706ECFB03 008 48 83 EC 30                                   sub     rsp, 30h        ; Integer Subtraction
.text:00007FF706ECFB07 038 F7 81 0C 01 00 00 00 08 00 00                 test    dword ptr [rcx+10Ch], 800h ; Logical Compare
.text:00007FF706ECFB11 038 48 8B D9                                      mov     rbx, rcx
.text:00007FF706ECFB14 038 0F 29 74 24 20                                movaps  [rsp+38h+var_18], xmm6 ; Move Aligned Four Packed Single-FP
.text:00007FF706ECFB19 038 0F 28 F1                                      movaps  xmm6, xmm1      ; Move Aligned Four Packed Single-FP
.text:00007FF706ECFB1C 038 75 6C                                         jnz     short loc_7FF706ECFB8A ; Jump if Not Zero (ZF=0)
.text:00007FF706ECFB1E 038 44 8B 81 C4 01 00 00                          mov     r8d, [rcx+1C4h]
.text:00007FF706ECFB25 038 8B 91 C8 01 00 00                             mov     edx, [rcx+1C8h]
.text:00007FF706ECFB2B 038 8B C2                                         mov     eax, edx
.text:00007FF706ECFB2D 038 83 A1 B8 01 00 00 00                          and     dword ptr [rcx+1B8h], 0 ; Logical AND
.text:00007FF706ECFB34 038 44 3B C2                                      cmp     r8d, edx        ; Compare Two Operands
.text:00007FF706ECFB37 038 41 0F 4C C0                                   cmovl   eax, r8d        ; Move if Less (SF!=OF)
.text:00007FF706ECFB3B 038 41 0F 4F D0                                   cmovg   edx, r8d        ; Move if Greater (ZF=0 & SF=OF)
.text:00007FF706ECFB3F 038 48 63 F8                                      movsxd  rdi, eax        ; Move with Sign-Extend Doubleword
.text:00007FF706ECFB42 038 48 63 F2                                      movsxd  rsi, edx        ; Move with Sign-Extend Doubleword
.text:00007FF706ECFB45 038 EB 3E                                         jmp     short loc_7FF706ECFB85 ; Jump
    */
}
void patchedGUI_EditBox__OnMouseUpCalcCusorIndex(GUI_EditBox *t) {
    static_assert(sizeof(RECT2) == 16);
    static_assert(sizeof(GUI_Obj) == 0xF0);
    static_assert(sizeof(GUI_EditBox) == 0x238);
    int i = 0;
    auto scale = t->GetTextScale(t->m_string, t->m_stringLen);
    auto chPtr = &t->m_string[t->m_scrolled];
    auto chPtrMax = chPtr + wcslen(chPtr);
    auto chr_w = 0.0f, offset = 0.0f;
    //looks like offset is already counted
    //if (t->m_align == 1)
    //    offset = (t->m_pos.m_width - CFont2D::StringWidthW_ulen(t->m_font, chPtr, uint32_t(chPtrMax - chPtr)) * scale) * 0.5f;
    UrsoftKerner uk(*t->m_font);
    for (; *chPtr && chPtr < chPtrMax; ++chPtr) {
        auto chr = (t->m_flags & 8) ? L'*' : *chPtr;
        chr_w += t->m_font->CharWidthW(chr, uk(chr)) * scale;
        if (chr_w >= t->m_cursorPos - offset)
            break;
        ++i;
    }
    t->m_cursorIndex = i + t->m_scrolled;
}
void patchGUI_EditBox__OnMouseUpCalcCusorIndex() {
    char jump[]{ "\x48\x83\xEC\x30\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xD0\x48\x83\xC4\x30\xC3\0\0\0\0\0\0\0\0\0\0\0\0" }; //sub rsp, 0x30; mov rax, addr; call rax; add rsp, 0x30; ret
    *(void **)(jump + 6) = (void *)((char *)&patchedGUI_EditBox__OnMouseUpCalcCusorIndex);
    PatchMainModule(
        "GUI_EditBox.OnMouseUpCalcCusorIndex", 33,
        "\x48\x8B\xC4\x48\x89\x58\x10\x48\x89\x68\x18\x56\x57\x41\x56\x48\x81\xEC\x90\x08\x00\x00\x0F\x29\x70\xD8\x0F\x29\x78\xC8\x44\x0F\x29\x40",
        jump
    );
    /* 
.text:00007FF706ED17F8                                   GUI_EditBox__OnMouseUpCalcCusorIndex proc near
.text:00007FF706ED17F8                                                                           ; CODE XREF: GUI_EditBox__OnMouseUp+339↑p
.text:00007FF706ED17F8                                                                           ; DATA XREF: .pdata:00007FF707F06904↓o
.text:00007FF706ED17F8
.text:00007FF706ED17F8                                   var_888         = qword ptr -888h
.text:00007FF706ED17F8                                   uText           = word ptr -848h
.text:00007FF706ED17F8                                   var_18          = byte ptr -18h
.text:00007FF706ED17F8
.text:00007FF706ED17F8                                   ; __unwind { // __CxxFrameHandler4
.text:00007FF706ED17F8 000 48 8B C4                                      mov     rax, rsp
.text:00007FF706ED17FB 000 48 89 58 10                                   mov     [rax+10h], rbx
.text:00007FF706ED17FF 000 48 89 68 18                                   mov     [rax+18h], rbp
.text:00007FF706ED1803 000 56                                            push    rsi
.text:00007FF706ED1804 008 57                                            push    rdi
.text:00007FF706ED1805 010 41 56                                         push    r14
.text:00007FF706ED1807 018 48 81 EC 90 08 00 00                          sub     rsp, 890h       ; Integer Subtraction
.text:00007FF706ED180E 8A8 0F 29 70 D8                                   movaps  xmmword ptr [rax-28h], xmm6 ; Move Aligned Four Packed Single-FP
.text:00007FF706ED1812 8A8 0F 29 78 C8                                   movaps  xmmword ptr [rax-38h], xmm7 ; Move Aligned Four Packed Single-FP
.text:00007FF706ED1816 8A8 44 0F 29 40 B8                                movaps  xmmword ptr [rax-48h], xmm8 ; Move Aligned Four Packed Single-FP
.text:00007FF706ED181B 8A8 48 8B F9                                      mov     rdi, rcx
.text:00007FF706ED181E 8A8 48 8D 05 7B E0 F4 00                          lea     rax, g_GiantFontW ; Load Effective Address
.text:00007FF706ED1825 8A8 48 39 41 78                                   cmp     [rcx+78h], rax  ; Compare Two Operands
.text:00007FF706ED1829 8A8 0F 85 C1 00 00 00                             jnz     loc_7FF706ED18F0 ; Jump if Not Zero (ZF=0)
.text:00007FF706ED182F 8A8 44 8B 81 E4 01 00 00                          mov     r8d, [rcx+1E4h] ; strLen
.text:00007FF706ED1836 8A8 48 8B 91 78 01 00 00                          mov     rdx, [rcx+178h] ; string
.text:00007FF706ED183D 8A8 E8 92 E9 FF FF                                call    GUI_EditBox__GetTextScale ; Call Procedure
.text:00007FF706ED1842 8A8 44 0F 28 C0                                   movaps  xmm8, xmm0      ; Move Aligned Four Packed Single-FP
.text:00007FF706ED1846 8A8 48 63 8F C8 01 00 00                          movsxd  rcx, dword ptr [rdi+1C8h] ; Move with Sign-Extend Doubleword
.text:00007FF706ED184D 8A8 48 8B 87 78 01 00 00                          mov     rax, [rdi+178h]
.text:00007FF706ED1854 8A8 48 8D 34 48                                   lea     rsi, [rax+rcx*2] ; Load Effective Address
.text:00007FF706ED1858 8A8 0F 57 FF                                      xorps   xmm7, xmm7      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF706ED185B 8A8 0F 57 F6                                      xorps   xmm6, xmm6      ; Bitwise Logical XOR for Single-FP Data
.text:00007FF706ED185E 8A8 83 BF 08 01 00 00 01                          cmp     dword ptr [rdi+108h], 1 ; Compare Two Operands
.text:00007FF706ED1865 8A8 75 2A                                         jnz     short loc_7FF706ED1891 ; Jump if Not Zero (ZF=0)
.text:00007FF706ED1867 8A8 48 8B D6                                      mov     rdx, rsi        ; uText
.text:00007FF706ED186A 8A8 48 8B 4F 78                                   mov     rcx, [rdi+78h]  ; this
.text:00007FF706ED186E 8A8 E8 99 C2 FD FF                                call    CFont2D__StringWidthW_u ; Call Procedure
.text:00007FF706ED1873 8A8 F3 41 0F 59 C0                                mulss   xmm0, xmm8      ; Scalar Single-FP Multiply
.text:00007FF706ED1878 8A8 F3 0F 10 77 10                                movss   xmm6, dword ptr [rdi+10h] ; Move Scalar Single-FP
.text:00007FF706ED187D 8A8 F3 0F 59 35 C7 90 A5 00                       mulss   xmm6, cs:a3     ; Scalar Single-FP Multiply
.text:00007FF706ED1885 8A8 F3 0F 59 05 BF 90 A5 00                       mulss   xmm0, cs:a3     ; Scalar Single-FP Multiply
.text:00007FF706ED188D 8A8 F3 0F 5C F0                                   subss   xmm6, xmm0      ; Scalar Single-FP Subtract
    */
}
void PatchFontsKerning() {
    loadKerns("data/fonts/ZwiftFondoMedium54ptW_EFIGS_K.bin", &glb54Map);
    loadKerns("data/fonts/ZwiftFondoBlack105ptW_EFIGS_K.bin", &glb105Map);
    patchCFont2D__StringWidthW_ulen();
    patchCFont2D__FitCharsToWidthW();
    patchCFont2D__FitWordsToWidthW();
    patchCFont2D__RenderWString_u();
    patchCFont2D__GetParagraphIndexByPosition();
    patchCFont2D__GetParagraphPositionByIndexW();
    patchGUI_EditBox__CalculateCursorIndexFromPosition();
    patchGUI_EditBox__CalcCursorPositionByIndex();
    patchGUI_EditBox__OnMouseUpCalcCusorIndex();
}
INT_PTR OnAttach() {
    const char* patches_str = getenv("ZWIFT_PATCHES");
    if (patches_str == NULL || *patches_str == '1') {
        /*PatchMainModule(
            "Trial.01", 9,
            "\xda\x04\x0\x0\xf\xb6\x13\x84\xd2",
            "\xda\x04\x0\x0\xf\xb6\x13\xb2\x02"
        );
        PatchMainModule(
            "Trial.02", 8,
            "\x48\x8d\x4d\x97\x41\x83\xcf\x08",
            "\x48\x8d\x4d\x97\x90\x90\x90\x90"
        );
        PatchMainModule(
            "Trial.03", 8,
            "\x48\x8d\x4d\x97\x41\x83\xcf\x10",
            "\x48\x8d\x4d\x97\x90\x90\x90\x90"
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
            4096 * 1132 // rdata segment length
        );*/
        PatchFontsKerning();
    }
#if 0
    const char* zd_str = getenv("ZWIFT_DEBUG");
    if (zd_str != NULL && *zd_str == '1') {
        //hook static EVP_DecryptInit_ex compiled into ZwiftApp.exe
/*; __int64 __fastcall EVP_DecryptInit_ex(int, int, int, int, void *)
EVP_DecryptInit_ex proc near            ; CODE XREF: Codec_decrypt+93↑p
                                        ; Codec_init+10A↑p ...

Src= qword ptr -18h
var_10= dword ptr -10h
arg_20= qword ptr  28h

B8 38 00 00 00		    mov     eax, 38h
E8 36 BB F7 FF		    call    __alloca_probe
48 2B E0		        sub     rsp, rax
48 8B 44 24 60		    mov     rax, [rsp+38h+arg_20]
C7 44 24 28 00 00 00 00	mov     [rsp+38h+var_10], 0             ; int
48 89 44 24 20		    mov     [rsp+38h+Src], rax              ; Src
E8 CC F9 FF FF		    call    EVP_CipherInit_ex
48 83 C4 38		        add     rsp, 38h
C3			            retn
EVP_DecryptInit_ex endp */
        char jmpNewXxcryptInit_ex[] = "\xff\x25\x0\x0\x0\x0" //6 bytes - jmp qword ptr [rip]
            "address"; //8 bytes
        void *newXxcryptInit_exPtr = newEVP_DecryptInit_ex;
        memcpy(jmpNewXxcryptInit_ex + 6, &newXxcryptInit_exPtr, 8);
        const uint8_t *place = PatchMainModule(
            "Hook.EVP_DecryptInit_ex", 6 + 8,
            "\xB8\x38\x0\x0\x0\xE8\x36\xBB\xF7\xFF\x48\x2B\xE0\x48",
            jmpNewXxcryptInit_ex
        );
        if (place) {
            int offset = *(int*)(place + 32); //should be 0xFFFFF9CC
            orgEVP_CipherInit_ex = (EVP_CipherInit_ex)(place + offset + 0x24);
        }
        newXxcryptInit_exPtr = newEVP_EncryptInit_ex;
        memcpy(jmpNewXxcryptInit_ex + 6, &newXxcryptInit_exPtr, 8);
        PatchMainModule(
            "Hook.EVP_EncryptInit_ex", 6 + 8,
            "\xB8\x38\x0\x0\x0\xE8\x6\xB7\xF7\xFF\x48\x2B\xE0\x48",
            jmpNewXxcryptInit_ex
        );
    }
#endif
    return 0;
}
FARPROC ptr = OnAttach;
bool WINAPI DllMain(HINSTANCE hDll, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)     {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringA("Zwift.DllMain.ATTACH");
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
                if (bJoystick && !glbNoSchwinn && (GetKeyState(VK_SCROLL) & 0x0001) == 0 /*scroll to turn off drifting mouse*/ &&
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
