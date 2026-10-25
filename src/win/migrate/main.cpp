// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 沈阳蜂蒿文化传媒有限公司
// 本文件以木兰宽松许可证第 2 版授权，许可证全文见项目根目录 LICENSE。

// FreeIME 11 —— 数据迁移工具（10.8.9 → 11.0.0）
#include <windows.h>
#include <cstdio>
#include <string>

#include "../config/legacy_importer.h"

int wmain(int argc, wchar_t **argv) {
    if (argc < 2) {
        wprintf(L"用法: freeime11-migrate <旧版极点目录>\n");
        wprintf(L"示例: freeime11-migrate \"C:\\Program Files\\freeime\"\n");
        return 1;
    }
    char buf[1024];
    WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, buf, sizeof(buf), nullptr, nullptr);
    const int n = freeime11::migrateLegacyData(buf);
    if (n < 0) {
        wprintf(L"未找到旧版配置文件。\n");
        return 2;
    }
    wprintf(L"迁移完成：识别并应用 %d 项配置（旧版 GBK INI → 新 UTF-8 config.ini）。\n", n);
    return 0;
}
