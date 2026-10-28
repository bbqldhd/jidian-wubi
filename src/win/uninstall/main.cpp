// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 沈阳蜂蒿文化传媒有限公司 (Charry)
// FreeIME 11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

// FreeIME 11 —— 卸载程序（Windows）
// 注销 TSF 文本服务、删除安装文件，可选删除用户词典与配置。
#include <windows.h>
#include <shlobj.h>
#include <string>

static std::wstring getExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    const size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"" : p.substr(0, pos);
}

static std::wstring getAppDataDir() {
    wchar_t buf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, buf)))
        return std::wstring(buf) + L"\\FreeIME11";
    return L"";
}

// 调用 FreeIME11.dll 的 DllUnregisterServer 注销 TSF 文本服务
static void unregisterTip(const std::wstring &dll) {
    HMODULE h = LoadLibraryW(dll.c_str());
    if (!h) return;
    typedef HRESULT(WINAPI *FnUnregister)();
    FnUnregister fn = reinterpret_cast<FnUnregister>(GetProcAddress(h, "DllUnregisterServer"));
    if (fn) fn();
    FreeLibrary(h);
}

// 递归删除目录
static void deleteTree(const std::wstring &path) {
    const std::wstring pat = path + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        const std::wstring full = path + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            deleteTree(full);
        else
            DeleteFileW(full.c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    RemoveDirectoryW(path.c_str());
}

static void deleteFileQuiet(const std::wstring &p) {
    if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) DeleteFileW(p.c_str());
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    if (MessageBoxW(nullptr, L"确定要卸载「极点五笔 11」吗？", L"极点五笔 11 卸载",
                    MB_YESNO | MB_ICONQUESTION) != IDYES)
        return 0;

    const std::wstring instDir = getExeDir();

    // 1) 注销 TSF 文本服务
    unregisterTip(instDir + L"\\FreeIME11.dll");

    // 2) 删除安装文件（卸载程序自身最后删除）
    deleteFileQuiet(instDir + L"\\FreeIME11.dll");
    deleteFileQuiet(instDir + L"\\freeime11-migrate.exe");
    deleteFileQuiet(instDir + L"\\freeime11-settings.exe");
    deleteFileQuiet(instDir + L"\\install.bat");

    // 3) 可选删除用户词典与配置
    if (MessageBoxW(nullptr, L"是否同时删除用户词典与配置？\n（%APPDATA%\\FreeIME11，删除后不可恢复）",
                    L"极点五笔 11 卸载", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        deleteTree(getAppDataDir());
    }

    // 4) 延迟删除卸载程序自身并清理安装目录
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const std::wstring cmd = L"/c ping 127.0.0.1 -n 2 >nul & del \"" + std::wstring(self) +
                             L"\" & rmdir \"" + instDir + L"\"";
    ShellExecuteW(nullptr, L"open", L"cmd.exe", cmd.c_str(), nullptr, SW_HIDE);

    MessageBoxW(nullptr, L"卸载完成。建议注销或重启系统以完全移除。", L"极点五笔 11 卸载",
                MB_OK | MB_ICONINFORMATION);
    return 0;
}
