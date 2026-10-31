// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

// FreeIME 11 —— TSF 文本服务 DLL 入口、类厂与自注册
#include <windows.h>
#include <msctf.h>

#include <new>

#include "FreeIME11Service.h"
#include "CandidateWindow.h"
#include "ToolbarWindow.h"

// MinGW 的 libuuid 缺少部分 TSF GUID，msctf.h 中仅 extern 声明，此处提供定义
EXTERN_C const GUID GUID_PROP_COMPOSING = {0xE12AC060, 0xAF15, 0x11D2, {0xAF, 0xC5, 0x00, 0x11, 0x05, 0xA2, 0x79, 0x9B}};

static HINSTANCE g_hInst = nullptr;
static LONG g_lDllRef = 0;

// 标准类厂
class FreeIME11Factory : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP CreateInstance(IUnknown *punkOuter, REFIID riid, void **ppv) override {
        if (punkOuter) return CLASS_E_NOAGGREGATION;
        FreeIME11Service *p = new (std::nothrow) FreeIME11Service();
        if (!p) return E_OUTOFMEMORY;
        HRESULT hr = p->QueryInterface(riid, ppv);
        p->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) InterlockedIncrement(&g_lDllRef);
        else InterlockedDecrement(&g_lDllRef);
        return S_OK;
    }

private:
    LONG ref_ = 1;
};

// 注册键写入辅助
static HRESULT setRegValue(HKEY root, const wchar_t *sub, const wchar_t *name, const wchar_t *value) {
    HKEY k = nullptr;
    if (RegCreateKeyExW(root, sub, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return E_FAIL;
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return S_OK;
}

// 注册文本服务（HKLM，系统级安装需管理员）
static HRESULT registerTextService() {
    const wchar_t *clsid = FREEWB_TEXTSERVICE_CLSID;
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(g_hInst, path, MAX_PATH);

    // 1) COM 类注册
    wchar_t sub[MAX_PATH];
    swprintf_s(sub, L"CLSID\\%s", clsid);
    setRegValue(HKEY_CLASSES_ROOT, sub, L"", L"FreeIME 11 Text Service");
    wchar_t sub2[MAX_PATH];
    swprintf_s(sub2, L"CLSID\\%s\\InprocServer32", clsid);
    setRegValue(HKEY_CLASSES_ROOT, sub2, L"", path);
    setRegValue(HKEY_CLASSES_ROOT, sub2, L"ThreadingModel", L"Apartment");

    // 2) TIP 注册
    swprintf_s(sub, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", clsid);
    setRegValue(HKEY_LOCAL_MACHINE, sub, L"", L"FreeIME 11");

    // 3) 语言配置
    swprintf_s(sub, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\LanguageProfile\\0x00000804\\%s",
               clsid, FREEWB_PROFILE_GUID);
    setRegValue(HKEY_LOCAL_MACHINE, sub, L"Enable", L"1");
    setRegValue(HKEY_LOCAL_MACHINE, sub, L"Description", L"极点五笔 11");
    setRegValue(HKEY_LOCAL_MACHINE, sub, L"IconFile", path);
    setRegValue(HKEY_LOCAL_MACHINE, sub, L"IconIndex", L"0");

    // 4) 类别注册
    //    系统按 CLSID 查类别，所以正向表 Category\Category\{catid}\{CLSID}
    //    与反向表 Category\Item\{CLSID}\{catid} 都要写。
    //    早期版本把 {CLSID} 写成了语言配置 GUID，导致系统认为该 TIP 不属于任何类别，
    //    表现就是“设置里能加、返回即失效”。
    static const wchar_t *kCategories[] = {
        FREEWB_CATEGORY_KEYBOARD,                        // GUID_TFCAT_TIP_KEYBOARD
        L"{13A016DF-560B-46CD-947A-4C3AF1E0E35D}",       // GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT
        L"{25504FB4-7BAB-4BC1-9C69-CF81890F0EF5}",       // GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT
    };
    for (const wchar_t *cat : kCategories) {
        swprintf_s(sub, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\Category\\Category\\%s\\%s", clsid, cat, clsid);
        setRegValue(HKEY_LOCAL_MACHINE, sub, L"", L"");

        swprintf_s(sub, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\Category\\Item\\%s\\%s", clsid, clsid, cat);
        setRegValue(HKEY_LOCAL_MACHINE, sub, L"", L"");
    }

    // 5) 清理历史版本写错位置的键：
    //    a) Category\Category\{catid}\{profileGUID}
    //    b) 误当作 UIELEMENTENABLED 的 {F4BAA7A3-...}（本实现未提供 UI 元素）
    //    注意 RegDeleteKey 不能删带子键的项，必须先删子键。
    for (const wchar_t *cat : kCategories) {
        swprintf_s(sub, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\Category\\Category\\%s\\%s", clsid, cat,
                   FREEWB_PROFILE_GUID);
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, sub);
    }
    swprintf_s(sub, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\Category\\Category\\%s\\%s", clsid,
               L"{F4BAA7A3-DC22-4D68-9F17-2A3D46CB3B4D}", FREEWB_PROFILE_GUID);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, sub);
    swprintf_s(sub, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\Category\\Category\\%s", clsid,
               L"{F4BAA7A3-DC22-4D68-9F17-2A3D46CB3B4D}");
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, sub);

    // 6) 运行时注册：ITfInputProcessorProfiles
    ITfInputProcessorProfiles *pProfiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfiles, (void **)&pProfiles))) {
        CLSID clsidGuid, profileGuid, catKeyboard;
        CLSIDFromString((LPOLESTR)clsid, &clsidGuid);
        CLSIDFromString((LPOLESTR)FREEWB_PROFILE_GUID, &profileGuid);
        CLSIDFromString((LPOLESTR)FREEWB_CATEGORY_KEYBOARD, &catKeyboard);
        pProfiles->Register(clsidGuid);
        pProfiles->AddLanguageProfile(clsidGuid, 0x0804, profileGuid, L"极点五笔 11",
                                      (ULONG)wcslen(L"极点五笔 11") + 1, path, (ULONG)wcslen(path) + 1, 0);
        pProfiles->Release();
    }
    return S_OK;
}

static HRESULT unregisterTextService() {
    const wchar_t *clsid = FREEWB_TEXTSERVICE_CLSID;
    wchar_t sub[MAX_PATH];

    ITfInputProcessorProfiles *pProfiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfiles, (void **)&pProfiles))) {
        CLSID clsidGuid, profileGuid;
        CLSIDFromString((LPOLESTR)clsid, &clsidGuid);
        CLSIDFromString((LPOLESTR)FREEWB_PROFILE_GUID, &profileGuid);
        pProfiles->RemoveLanguageProfile(clsidGuid, 0x0804, profileGuid);
        pProfiles->Unregister(clsidGuid);
        pProfiles->Release();
    }

    swprintf_s(sub, L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", clsid);
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, sub);
    swprintf_s(sub, L"CLSID\\%s", clsid);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, sub);
    return S_OK;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInst = hinst;
        DisableThreadLibraryCalls(hinst);
        CandidateWindow::setInstanceHandle(hinst);
        ToolbarWindow::setInstanceHandle(hinst);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv) {
    CLSID clsid;
    CLSIDFromString((LPOLESTR)FREEWB_TEXTSERVICE_CLSID, &clsid);
    if (!IsEqualCLSID(rclsid, clsid)) return CLASS_E_CLASSNOTAVAILABLE;

    FreeIME11Factory *pFactory = new (std::nothrow) FreeIME11Factory();
    if (!pFactory) return E_OUTOFMEMORY;
    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() { return g_lDllRef == 0 ? S_OK : S_FALSE; }

STDAPI DllRegisterServer() {
    // regsvr32 进程不保证已初始化 COM，ITfInputProcessorProfiles 需要 COM
    const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const HRESULT hr = registerTextService();
    if (SUCCEEDED(hrInit)) CoUninitialize();
    return hr;
}

STDAPI DllUnregisterServer() {
    const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const HRESULT hr = unregisterTextService();
    if (SUCCEEDED(hrInit)) CoUninitialize();
    return hr;
}
