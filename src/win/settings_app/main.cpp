// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 沈阳蜂蒿文化传媒有限公司 (Charry)
// FreeIME 11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

// FreeIME 11 —— 设置程序（Windows 11，最小可用骨架）
// 负责：运行迁移、注册/注销文本服务、打开配置目录。
#include <windows.h>
#include <shellapi.h>
#include <cstdio>

#include "../../utils/utils.h"

static int showUsage() {
    MessageBoxW(nullptr, L"极点五笔 11 设置\n\n"
        L"--register   注册输入法\n"
        L"--unregister 注销输入法\n"
        L"--migrate <旧版目录>  迁移 10.8.9 数据\n"
        L"--opendata   打开用户数据目录\n",
        L"极点五笔 11", MB_OK);
    return 0;
}

static void openDataDir() {
    const std::string dir = freewb::userFreewbPath();
    wchar_t w[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, w, MAX_PATH);
    ShellExecuteW(nullptr, L"open", w, nullptr, nullptr, SW_SHOWNORMAL);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR cmd, int) {
    if (!cmd || !*cmd) return showUsage();

    if (wcsstr(cmd, L"--opendata")) { openDataDir(); return 0; }

    // 注册/注销通过 regsvr32 或直接调用 DllRegisterServer 完成；这里给提示
    if (wcsstr(cmd, L"--register")) {
        MessageBoxW(nullptr, L"请以管理员身份运行：\nregsvr32 FreeIME11.dll\n并在 设置→时间和语言→语言和区域 中添加“极点五笔 11”。",
                    L"极点五笔 11", MB_OK);
        return 0;
    }
    if (wcsstr(cmd, L"--unregister")) {
        MessageBoxW(nullptr, L"请以管理员身份运行：\nregsvr32 /u FreeIME11.dll",
                    L"极点五笔 11", MB_OK);
        return 0;
    }
    return showUsage();
}
