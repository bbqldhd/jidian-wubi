// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

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

    // 关键：把语言配置「为当前用户启用」。只调 AddLanguageProfile 只是系统登记，
    // 不调这个的话，语言列表会加进去又立刻失效（Windows 认为该配置未启用）。
    if (SUCCEEDED(hr)) {
        HRESULT hrEnable = pProfiles->EnableLanguageProfile(kClsid, 0x0804, kProfile, TRUE);
        if (FAILED(hrEnable)) {
            // 启用失败不覆盖激活结果，但要报告出来
            wprintf(L"EnableLanguageProfile hr=0x%08lX\n", static_cast<unsigned long>(hrEnable));
        }
    }

    pProfiles->Release();
    if (pHr) *pHr = static_cast<DWORD>(hr);
    return SUCCEEDED(hr);
}

// 确保语言列表里存在本输入法，且值类型必须是 REG_DWORD。
// Windows 只认 DWORD 序号；写成 REG_SZ 会被静默忽略，表现为
// 「设置里加进去、返回就没了」。
static bool ensureLanguageListEntry() {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kLangKey, 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &hKey,
                        nullptr) != ERROR_SUCCESS)
        return false;

    DWORD type = 0, size = 0, order = 0;
    if (RegQueryValueExW(hKey, kEntryName, nullptr, &type, nullptr, &size) == ERROR_SUCCESS) {
        if (type == REG_DWORD && size == sizeof(DWORD)) {
            RegCloseKey(hKey);  // 已是正确类型
            return true;
        }
        if (type == REG_SZ) {
            wchar_t buf[32] = {0};
            DWORD cb = sizeof(buf) - sizeof(wchar_t);
            if (RegQueryValueExW(hKey, kEntryName, nullptr, &type, reinterpret_cast<BYTE *>(buf), &cb) ==
                ERROR_SUCCESS)
                order = static_cast<DWORD>(_wtoi(buf));
        }
    }

    if (order == 0) {
        for (DWORD i = 0;; ++i) {
            wchar_t name[512];
            DWORD nameLen = 512;
            DWORD vtype = 0, data = 0, dataLen = sizeof(data);
            if (RegEnumValueW(hKey, i, name, &nameLen, nullptr, &vtype, nullptr, nullptr) != ERROR_SUCCESS) break;
            if (vtype == REG_DWORD &&
                RegQueryValueExW(hKey, name, nullptr, nullptr, reinterpret_cast<BYTE *>(&data), &dataLen) ==
                    ERROR_SUCCESS &&
                data > order)
                order = data;
        }
        order += 1;
    }

    const LONG rc = RegSetValueExW(hKey, kEntryName, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&order),
                                   sizeof(order));
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
        wprintf(L"ActivateProfile failed hr=0x%08lX\n", static_cast<unsigned long>(hr));
    }

    // 无论 API 是否成功，都确保语言列表条目存在且为 REG_DWORD
    const bool okList = ensureLanguageListEntry();
    wprintf(L"language list entry: %s\n", okList ? L"OK" : L"FAILED");

    if (SUCCEEDED(hrInit)) CoUninitialize();
    return (okApi || okList) ? 0 : 3;
}
