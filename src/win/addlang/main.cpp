// FreeIME 11 —— 把输入法启用为当前用户的一个键盘（TSF 官方方式）
//
// 首选路径：ITfInputProcessorProfileMgr::ActivateProfile
//   —— 这是微软给输入法安装程序的标准 API，会同时更新语言列表与排序，
//      不靠手写注册表，不会被 Winlangdb 缓存过滤掉。
// 回落路径：若 API 不可用（如 TSF 服务异常），退化为只追加 HKCU 值的保守写法，
//      绝不覆盖 InputMethodTips 或其他输入法条目。
#include <windows.h>
#include <msctf.h>
#include <cstdlib>
#include <cstdio>
#include <string>

// MinGW 的 msctf.h 未随附该 IID，按 SDK 定义补齐
static const GUID kIID_ITfInputProcessorProfileMgr = {
    0x71C6E74C, 0x0F28, 0x11D8, {0xA8, 0x2A, 0x00, 0x06, 0x5B, 0x84, 0x43, 0x5C}};

static const GUID kClsid  = {0xB6F2C8A1, 0x7D4E, 0x4F1A, {0x9C, 0x3B, 0x2E, 0x5D, 0x8A, 0x7F, 0x6E, 0x10}};
static const GUID kProfile = {0xC9D4E2B7, 0x5A3F, 0x4B8C, {0x9D, 0x1E, 0x6F, 0x0A, 0x2C, 0x4B, 0x8D, 0x53}};

static const wchar_t *kLangKey = L"Control Panel\\International\\User Profile\\zh-Hans-CN";
static const wchar_t *kEntryName =
    L"0804:{B6F2C8A1-7D4E-4F1A-9C3B-2E5D8A7F6E10}{C9D4E2B7-5A3F-4B8C-9D1E-6F0A2C4B8D53}";

// 用 TSF 官方 API 启用；成功返回 true
static bool activateViaTsf(DWORD *pHr) {
    ITfInputProcessorProfiles *pProfiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfiles, (void **)&pProfiles);
    if (FAILED(hr)) {
        if (pHr) *pHr = static_cast<DWORD>(hr);
        return false;
    }

    ITfInputProcessorProfileMgr *pMgr = nullptr;
    hr = pProfiles->QueryInterface(kIID_ITfInputProcessorProfileMgr, (void **)&pMgr);
    if (FAILED(hr) || !pMgr) {
        pProfiles->Release();
        if (pHr) *pHr = static_cast<DWORD>(hr);
        return false;
    }

    // TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE = 0x00000004
    hr = pMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0804, kClsid, kProfile, nullptr, 0x00000004);
    pMgr->Release();
    pProfiles->Release();
    if (pHr) *pHr = static_cast<DWORD>(hr);
    return SUCCEEDED(hr);
}

// 回落：只追加自己的 HKCU 值（幂等，不碰其他值）
static bool activateViaRegistry() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kLangKey, 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &hKey,
                        nullptr) != ERROR_SUCCESS)
        return false;

    DWORD type = 0, size = 0;
    if (RegQueryValueExW(hKey, kEntryName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }

    DWORD maxOrder = 0;
    for (DWORD i = 0;; ++i) {
        wchar_t name[512];
        DWORD nameLen = 512;
        BYTE data[128] = {0};
        DWORD dataLen = sizeof(data);
        DWORD vtype = 0;
        if (RegEnumValueW(hKey, i, name, &nameLen, nullptr, &vtype, data, &dataLen) != ERROR_SUCCESS) break;
        if (vtype == REG_SZ) {
            const int v = _wtoi(reinterpret_cast<const wchar_t *>(data));
            if (v > static_cast<int>(maxOrder)) maxOrder = static_cast<DWORD>(v);
        }
    }

    const std::wstring value = std::to_wstring(maxOrder + 1);
    const LONG rc = RegSetValueExW(hKey, kEntryName, 0, REG_SZ,
                                   reinterpret_cast<const BYTE *>(value.c_str()),
                                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

// 把随程序安装的词库复制到当前用户目录（多用户场景各自可用；已存在则跳过）
static void ensureUserData() {
    wchar_t exePath[MAX_PATH] = {0};
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return;

    std::wstring exeDir(exePath);
    const size_t slash = exeDir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return;
    exeDir.resize(slash);

    wchar_t appData[MAX_PATH] = {0};
    const DWORD n = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;

    const std::wstring dstRoot = std::wstring(appData) + L"\\FreeIME11\\data";

    struct Item {
        const wchar_t *file;  // 文件名
        const wchar_t *sub;   // 相对 data 的子目录（可为空）
    };
    const Item items[] = {
        {L"wbzx.mb", L"mb\\default"},
        {L"pinyin.mb", L"mb\\default"},
        {L"freeime_chinese_dict.db", L""},
    };

    for (const Item &it : items) {
        std::wstring relDir = it.sub;
        std::wstring src = exeDir + L"\\data\\";
        if (!relDir.empty()) src += relDir + L"\\";
        src += it.file;

        if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES) continue;

        std::wstring dstDir = dstRoot;
        if (!relDir.empty()) dstDir += std::wstring(L"\\") + relDir;
        // 逐级创建目录
        std::wstring partial;
        for (size_t i = 0; i <= dstDir.size(); ++i) {
            if (i == dstDir.size() || dstDir[i] == L'\\') {
                if (partial.size() > 3) CreateDirectoryW(partial.c_str(), nullptr);
                if (i < dstDir.size()) partial.push_back(L'\\');
            } else {
                partial.push_back(dstDir[i]);
            }
        }
        const std::wstring dst = dstDir + L"\\" + it.file;
        if (GetFileAttributesW(dst.c_str()) == INVALID_FILE_ATTRIBUTES)
            CopyFileW(src.c_str(), dst.c_str(), FALSE);
    }
}

int wmain() {
    ensureUserData();

    const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    DWORD hr = 0;
    const bool okApi = activateViaTsf(&hr);
    if (okApi) {
        wprintf(L"ActivateProfile OK\n");
    } else {
        wprintf(L"ActivateProfile failed hr=0x%08lX, falling back to registry\n", static_cast<unsigned long>(hr));
        activateViaRegistry();
    }

    if (SUCCEEDED(hrInit)) CoUninitialize();
    return okApi ? 0 : 3;
}
