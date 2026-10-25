// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 沈阳蜂蒿文化传媒有限公司
// 本文件以木兰宽松许可证第 2 版授权，许可证全文见项目根目录 LICENSE。

// FreeIME 11 —— 10.8.9 旧版数据迁移实现
#include "legacy_importer.h"

#include <windows.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include "../../utils/settings.h"

namespace freeime11 {

namespace {

// GBK(CP936) → UTF-8
std::string gbkToUtf8(const std::string &gbk) {
    if (gbk.empty()) return {};
    const int wlen = ::MultiByteToWideChar(936, 0, gbk.data(), static_cast<int>(gbk.size()), nullptr, 0);
    if (wlen <= 0) return gbk;
    std::wstring w(wlen, L'\0');
    ::MultiByteToWideChar(936, 0, gbk.data(), static_cast<int>(gbk.size()), &w[0], wlen);
    const int ulen = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wlen, nullptr, 0, nullptr, nullptr);
    std::string u(ulen, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), wlen, &u[0], ulen, nullptr, nullptr);
    return u;
}

// 自动检测编码并转 UTF-8（10.8.9 配置文件实测为 UTF-16LE 带 BOM，兼容 GBK/UTF-8）
std::string decodeToUtf8(const std::string &raw) {
    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
        static_cast<unsigned char>(raw[1]) == 0xFE) {
        // UTF-16LE
        const wchar_t *w = reinterpret_cast<const wchar_t *>(raw.data() + 2);
        const int wlen = static_cast<int>((raw.size() - 2) / 2);
        if (wlen <= 0) return {};
        const int ulen = ::WideCharToMultiByte(CP_UTF8, 0, w, wlen, nullptr, 0, nullptr, nullptr);
        std::string u(ulen, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, w, wlen, &u[0], ulen, nullptr, nullptr);
        return u;
    }
    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFE &&
        static_cast<unsigned char>(raw[1]) == 0xFF) {
        // UTF-16BE（逐字节换位为 LE）
        std::string le = raw.substr(2);
        for (size_t i = 0; i + 1 < le.size(); i += 2) std::swap(le[i], le[i + 1]);
        return decodeToUtf8(std::string("\xFF\xFE") + le);
    }
    if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
        static_cast<unsigned char>(raw[1]) == 0xBB && static_cast<unsigned char>(raw[2]) == 0xBF) {
        return raw.substr(3); // UTF-8 BOM
    }
    return gbkToUtf8(raw); // 默认 GBK
}

std::string trim(const std::string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// 旧键 → 新键（settings uniquename）。返回空表示不迁移。
std::string mapLegacyKey(const std::string &key) {
    static const std::unordered_map<std::string, std::string> kMap = {
        // [System]
        {"FullShape", "fullWidthFlg"},          // 全角字符
        {"ChinaSymbol", "chinesePunc"},        // 中文标点
        {"SmartSymbol", "puncAutoPair"},       // 智能标点
        {"AutoAdjust", "autoAdjustFreq"},      // 自动调频
        {"UseAutoPhrase", "autoPhrase"},       // 自动造词
        {"KeySound", "uiAudioEffect"},         // 打字音效
        {"CandFollowCursor", "cursorFollow"},  // 候选窗光标跟随
        {"ShowInfoMsg", "showOpRemindInfo"},   // 候选窗提示信息
        {"JianCodeFanText", "simpTradFlg"},    // 简入繁出
        {"SeparateChar", "separateChar"},      // 序号与候选项间隔符
        {"EnterFunction", "enterCommitPreedit"},// 回车键作用
        {"InsertSpace", "addCharAfterOutput"}, // 插入特定字符
        {"ShowSpeed", "inputStatistic"},       // 输入速度统计
        // [QuickKey]
        {"CN_EN", "cnEnSwitch"},               // 中英文切换键
        {"SelectSecond", "secondRecodeKey"},   // 第二重码选择键
        {"SelectThird", "thirdRecodeKey"},     // 第三重码选择键
        {"PageNext", "nextPageKey"},           // 下一页翻页键
        {"PagePrior", "prevPageKey"},          // 上一页翻页键
        {"AutoEnStr", "autoToEnStr"},          // 自动转英文前缀
    };
    auto it = kMap.find(key);
    return it == kMap.end() ? std::string() : it->second;
}

// 旧值 → 新值（按新键类型转换）
std::string convertValue(const std::string &newKey, const std::string &oldValue) {
    const std::string v = trim(oldValue);
    // 布尔项
    if (newKey == "fullWidthFlg" || newKey == "chinesePunc" || newKey == "puncAutoPair" ||
        newKey == "autoAdjustFreq" || newKey == "autoPhrase" || newKey == "uiAudioEffect" ||
        newKey == "cursorFollow" || newKey == "showOpRemindInfo" || newKey == "simpTradFlg" ||
        newKey == "inputStatistic") {
        return (v == "0" || v == "off" || v == "false") ? "false" : "true";
    }
    // 回车键：10.8.9 EnterFunction=1 表示编码上屏
    if (newKey == "enterCommitPreedit") {
        return (v == "1") ? "true" : "false";
    }
    // 键名映射：极点的字符键 → KEY_ 形式
    if (newKey == "secondRecodeKey" || newKey == "thirdRecodeKey" ||
        newKey == "nextPageKey" || newKey == "prevPageKey" || newKey == "cnEnSwitch") {
        if (v == ";") return "KEY_SEMICOLON";
        if (v == "'") return "KEY_QUOTE";
        if (v == "=") return "KEY_EQUAL";
        if (v == "-") return "KEY_DASH";
        if (v == "RShift") return "KEY_RIGHT_SHIFT";
        if (v == "LShift") return "KEY_LEFT_SHIFT";
        if (v == "Ctrl") return "KEY_CTRL";
    }
    return v;
}

} // namespace

bool parseLegacyIni(const std::string &gbkPath, std::vector<LegacyConfigItem> &out) {
    std::ifstream in(gbkPath, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = decodeToUtf8(ss.str());

    std::string section;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        const std::string t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#') continue; // 注释
        if (t[0] == '[' && t.back() == ']') {
            section = t.substr(1, t.size() - 2);
            continue;
        }
        const size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        out.push_back({section, trim(t.substr(0, eq)), trim(t.substr(eq + 1))});
    }
    return true;
}

int applyLegacyConfig(const std::vector<LegacyConfigItem> &items) {
    int mapped = 0;
    for (const auto &it : items) {
        const std::string newKey = mapLegacyKey(it.key);
        if (newKey.empty()) continue;
        settings::instance().setByKey(newKey, convertValue(newKey, it.value));
        ++mapped;
    }
    if (mapped > 0) settings::instance().save();
    return mapped;
}

int migrateLegacyData(const std::string &legacyDir) {
    std::vector<LegacyConfigItem> items;
    // 优先级：freeime.ini（指向方案）→ 方案 ini（default.ini / jidian.ini）
    std::string scheme = "default";
    const std::string freeimeIni = legacyDir + "/conf/freeime.ini";
    if (parseLegacyIni(freeimeIni, items)) {
        for (const auto &it : items) {
            if (it.key == "ConfigFile") { scheme = trim(it.value); break; }
        }
    }
    items.clear();
    std::string schemeFile = scheme;
    if (schemeFile.find(".ini") == std::string::npos) schemeFile += ".ini";
    const std::string schemeIni = legacyDir + "/conf/" + schemeFile;
    if (!parseLegacyIni(schemeIni, items)) {
        if (!parseLegacyIni(legacyDir + "/conf/default.ini", items)) return -1;
    }
    return applyLegacyConfig(items);
}

} // namespace freeime11
