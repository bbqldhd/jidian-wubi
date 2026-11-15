// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// FreeIME 11 —— 设置程序（原生 Win32 GUI）
//
// 界面：左侧分类 + 右侧可搜索的配置表（ListView），双击编辑：
//   Bool 直接切换开启/关闭，Int / String 弹出输入框。
// 改动先缓存在内存里，点「保存」才写盘；关闭时提示未保存的改动。
// 配置项清单由 settings.h 的 allConfigMeta() 提供（单一数据源，不另写一份）。
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../utils/settings.h"
#include "../../utils/utils.h"

namespace {

constexpr wchar_t kWndClass[] = L"FreeIME11SettingsWnd";
constexpr wchar_t kPromptClass[] = L"FreeIME11SettingsPrompt";

enum : int {
    IDC_CATS = 101,
    IDC_SEARCH = 102,
    IDC_LIST = 103,
    IDC_STATUS = 104,
    IDC_BTN_SAVE = 105,
    IDC_BTN_REVERT = 106,
    IDC_BTN_DEFAULTS = 107,
    IDC_BTN_OPENDATA = 108,
    IDC_BTN_ABOUT = 109,
    IDC_BTN_CLOSE = 110,
    IDC_TITLE = 111,
    IDC_PROMPT_EDIT = 201,
    IDC_PROMPT_HINT = 202,
    IDOK_PROMPT = 203,
    IDCANCEL_PROMPT = 204,
};

struct Category {
    const char *key;       // 对应 ConfigMeta::category，"*" 表示全部
    const wchar_t *name;
};

const Category kCategories[] = {
    {"*", L"全部设置"},
    {"!unwired", L"暂未生效"},
    {"Misc", L"常规"},
    {"Common", L"输入"},
    {"CandidateWinUi", L"候选窗 · 外观"},
    {"CandidateWinOptions", L"候选窗 · 行为"},
    {"Punctuation", L"标点"},
    {"ShortcutKey", L"快捷键"},
    {"Ui", L"界面"},
    {"Engine", L"引擎"},
    {"Advanced", L"高级"},
    {"Others", L"其它"},
};

struct Label {
    const char *key;
    const wchar_t *name;
    const wchar_t *hint;
};

// 常用项的中文名与说明；未列出的项显示原始键名，说明回退到配置里的英文描述/默认值。
const Label kLabels[] = {
    {"charSet", L"字符集", L"0 = GBK，1 = GB18030，2 = UTF-8"},
    {"enterCommitPreedit", L"回车上屏原文", L"组字中按回车，把候选窗输入框里的编码串原样上屏"},
    {"inputMode", L"默认键盘模式", L"engine:wbzx 五笔 / wbpy 五笔拼音 / py 拼音 / en 英文"},
    {"simpTradFlg", L"繁体输出", L"开启后把上屏内容转换为繁体"},
    {"fullWidthFlg", L"全角", L"数字与字母以全角上屏"},
    {"vkMode", L"虚拟键盘布局", L"-1 = 跟随系统"},
    {"separateChar", L"编码分隔符", L"候选窗编码串里显示的分隔字符"},
    {"cnEnSwitch", L"中英切换键", L"如 KEY_LEFT_SHIFT"},
    {"curUsedLexicon", L"当前词库", L"词库目录名"},
    {"CoustomChar", L"自定义字符表", L"zz 编码对应的符号序列"},
    {"CoustomMark", L"自定义标点表", L"Shift + 符号键对应的标点序列"},
    {"wubiTable", L"五笔码表", L"相对数据目录的码表路径"},
    {"pinyinTable", L"拼音码表", L"相对数据目录的码表路径"},
    {"pinyinFuzzy", L"拼音智能纠错", L"自动纠正 z/zh、c/ch、s/sh、an/ang、en/eng、in/ing"},
    {"candiWinDispMode", L"候选窗排列方式", L"0 = 横排，1 = 竖排"},
    {"candiCharCount", L"每页候选字数", L"候选窗一页显示多少个候选"},
    {"candiWordCount", L"每页候选词数", L"候选窗一页显示多少条词"},
    {"radius", L"候选窗圆角", L"窗口圆角半径（像素）"},
    {"transparency", L"候选窗透明度", L"0 = 不透明，数值越大越透明"},
    {"candiTextFontName", L"候选字体", L"候选文字使用的字体名"},
    {"candiTextFontSize", L"候选字号", L"候选文字字号（磅）"},
    {"bgColor", L"背景色", L"#AARRGGBB 格式"},
    {"bgImage", L"背景图", L"候选窗背景图片路径"},
    {"borderColor", L"边框色", L"#AARRGGBB 格式"},
    {"gradientColor0", L"渐变起始色", L"#AARRGGBB 格式"},
    {"gradientColor1", L"渐变结束色", L"#AARRGGBB 格式"},
    {"candiWordTextColor", L"候选文字色", L"#AARRGGBB 格式"},
    {"candiPromptTextColor", L"提示文字色", L"#AARRGGBB 格式"},
    {"enableTiled", L"平铺背景", L"背景图以平铺方式绘制"},
    {"useGradientColor", L"渐变背景", L"使用渐变色代替纯色背景"},
    {"useBgImage", L"使用背景图", L"显示候选窗背景图片"},
    {"showCandDictInfo", L"显示词典信息", L"候选里附带来源词典信息"},
    {"cursorFollow", L"候选窗跟随光标", L"候选窗出现在插入符附近"},
    {"showOpRemindInfo", L"显示操作提示", L"候选窗里显示翻页等操作提示"},
    {"shiftSelectRecode", L"Shift 选重码", L"用 Shift 选择重码候选"},
    {"secondRecodeKey", L"二重码选择键", L"如 KEY_SEMICOLON"},
    {"thirdRecodeKey", L"三重码选择键", L"如 KEY_QUOTE"},
    {"prevPageKey", L"上一页键", L"如 KEY_DASH"},
    {"nextPageKey", L"下一页键", L"如 KEY_EQUAL"},
    {"codeRemind", L"编码提示", L"显示当前输入的编码"},
    {"spaceFullWhenCharHalf", L"半角字后全角空格", L"半角字符后自动用全角空格"},
    {"wordThink", L"联想", L"上屏后显示联想词"},
    {"puncAutoPair", L"标点自动配对", L"输入左括号时自动补右括号"},
    {"chinesePunc", L"中文标点", L"把半角标点转换为中文标点"},
    {"autoToHalfPuncAfterNumber", L"数字后半角标点", L"数字后面自动使用半角标点"},
    {"remindExistWord", L"提示已有词", L"造词时提示已存在的词"},
    {"alertWhenEmptyCode", L"空码提示", L"编码无候选时给出提示"},
    {"autoAdjustFreq", L"自动调频", L"根据使用频率调整候选顺序"},
    {"zzSpecialEncodingSymbols", L"zz 特殊符号", L"zz 编码输出自定义符号表"},
    {"shiftCommitChar", L"Shift 上屏原文", L"组字中按 Shift 先上屏编码再切模式"},
    {"inputStatistic", L"输入统计", L"记录输入习惯"},
    {"typeEffect", L"打字音效", L"输入时播放音效"},
    {"recodeCalib", L"重码校准", L"重码候选顺序校准"},
    {"autoPhrase", L"自动造词", L"根据上屏内容自动积累新词"},
    {"autoToEnStr", L"自动英文前缀", L"匹配这些前缀时自动进入临时英文"},
    {"curSkinId", L"当前皮肤", L"皮肤目录名"},
    {"toolbarAutoLocate", L"工具栏自动定位", L"工具栏跟随输入位置"},
    {"toolbarAutoExpand", L"工具栏自动展开", L"鼠标靠近时展开工具栏"},
    {"uiAudioEffect", L"界面音效", L"界面操作播放音效"},
    {"useFreewbFont", L"使用内置字体", L"候选窗使用 FreeWB 自带字体"},
    {"showRealtimeHelp", L"实时帮助", L"显示实时编码帮助"},
    {"hideToolbar", L"隐藏工具栏", L"不显示输入法状态栏"},
    {"toolbarTransparency", L"工具栏透明度", L"0 = 不透明"},
    {"disableFullHalfSwitch", L"禁用全半角切换", L"禁用对应的快捷键"},
    {"disableAllShortcutKey", L"禁用所有快捷键", L"关闭输入法内部的快捷键"},
    {"setupOption", L"打开设置快捷键", L"如 CTRL+KEY_COMMA"},
    {"WbzxEngine", L"启用五笔引擎", L"关闭后四模式轮换会跳过五笔"},
    {"WbpyEngine", L"启用五笔拼音引擎", L"关闭后四模式轮换会跳过五笔拼音"},
    {"PyEngine", L"启用拼音引擎", L"关闭后四模式轮换会跳过拼音"},
};

std::wstring wide(const char *utf8) {
    if (!utf8 || !*utf8) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], n);
    return w;
}

std::string narrow(const std::wstring &w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(towlower(c));
    });
    return s;
}

const Label *findLabel(const char *key) {
    for (const auto &l : kLabels) {
        if (std::strcmp(l.key, key) == 0) return &l;
    }
    return nullptr;
}

bool isTrue(const std::string &v) {
    return v == "true" || v == "1" || v == "yes" || v == "on";
}

struct App {
    HWND hwnd = nullptr;
    HWND hTitle = nullptr, hSearch = nullptr, hCats = nullptr, hList = nullptr;
    HWND hStatus = nullptr;
    HWND hBtnSave = nullptr, hBtnRevert = nullptr, hBtnDefaults = nullptr;
    HWND hBtnOpenData = nullptr, hBtnAbout = nullptr, hBtnClose = nullptr;
    HFONT font = nullptr, fontTitle = nullptr;
    int dpi = 96;
    int catIndex = 0;
    std::wstring filter;
    // 未落盘的改动：key -> 值字符串。显示与保存都以它为准。
    std::unordered_map<std::string, std::string> pending;

    int S(int v) const { return MulDiv(v, dpi, 96); }
};

App *g_app = nullptr;

// ---------------------------------------------------------------------------
// 模态输入框（不用资源脚本，运行时创建，避免多一份 .rc 维护）
// ---------------------------------------------------------------------------
struct Prompt {
    HWND hwnd = nullptr, edit = nullptr;
    std::wstring value;
    bool done = false, ok = false;
};
Prompt *g_prompt = nullptr;

LRESULT CALLBACK promptProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Prompt *p = reinterpret_cast<Prompt *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_NCCREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams));
        // 必须交给 DefWindowProc：窗口标题是在 WM_NCCREATE 里创建的，
        // 直接 return TRUE 会导致标题栏一片空白。
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_COMMAND:
        if (p) {
            if (LOWORD(wp) == IDOK_PROMPT || LOWORD(wp) == 1) {
                wchar_t buf[2048]{};
                GetWindowTextW(p->edit, buf, 2048);
                p->value = buf;
                p->ok = true;
                p->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wp) == IDCANCEL_PROMPT || LOWORD(wp) == 2) {
                p->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
        }
        break;
    case WM_CLOSE:
        if (p) { p->done = true; DestroyWindow(hwnd); return 0; }
        break;
    case WM_DESTROY:
        if (g_prompt == p) g_prompt = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool promptValue(HWND parent, const std::wstring &title, const std::wstring &hint,
                 const std::wstring &initial, std::wstring &out) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = promptProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kPromptClass;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);
        registered = true;
    }

    Prompt p;
    g_prompt = &p;
    const int W = g_app->S(430), H = g_app->S(190);
    RECT pr{};
    GetWindowRect(parent, &pr);
    const int x = pr.left + (pr.right - pr.left - W) / 2;
    const int y = pr.top + (pr.bottom - pr.top - H) / 2;

    p.hwnd = CreateWindowExW(WS_EX_CONTROLPARENT | WS_EX_DLGMODALFRAME, kPromptClass, title.c_str(),
                             WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_CLIPCHILDREN,
                             x, y, W, H, parent, nullptr, GetModuleHandleW(nullptr), &p);
    if (!p.hwnd) { g_prompt = nullptr; return false; }

    const int pad = g_app->S(14);
    HWND hHint = CreateWindowW(L"STATIC", hint.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
                               pad, pad, W - pad * 2, g_app->S(48), p.hwnd,
                               reinterpret_cast<HMENU>(IDC_PROMPT_HINT), nullptr, nullptr);
    p.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initial.c_str(),
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                             pad, pad + g_app->S(54), W - pad * 2, g_app->S(28), p.hwnd,
                             reinterpret_cast<HMENU>(IDC_PROMPT_EDIT), nullptr, nullptr);
    const int bw = g_app->S(82), bh = g_app->S(28);
    HWND hOk = CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                             W - pad - bw * 2 - g_app->S(8), H - pad - bh - g_app->S(6), bw, bh, p.hwnd,
                             reinterpret_cast<HMENU>(IDOK_PROMPT), nullptr, nullptr);
    HWND hCancel = CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                 W - pad - bw, H - pad - bh - g_app->S(6), bw, bh, p.hwnd,
                                 reinterpret_cast<HMENU>(IDCANCEL_PROMPT), nullptr, nullptr);

    // 子控件不继承父窗口字体，要逐个设置，否则提示/按钮会用系统默认小字挤成一团
    const HWND kids[] = {hHint, p.edit, hOk, hCancel};
    for (HWND h : kids) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(g_app->font), TRUE);
    SetFocus(p.edit);
    SendMessageW(p.edit, EM_SETSEL, 0, -1);
    EnableWindow(parent, FALSE);

    MSG msg{};
    while (!p.done) {
        MsgWaitForMultipleObjects(0, nullptr, FALSE, INFINITE, QS_ALLINPUT);
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (!IsDialogMessageW(p.hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        if (!IsWindow(p.hwnd)) break;
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    g_prompt = nullptr;
    if (p.ok) out = p.value;
    return p.ok;
}

// ---------------------------------------------------------------------------
// 主窗口
// ---------------------------------------------------------------------------
std::wstring valueText(const settings::ConfigMeta &m, bool *modified = nullptr) {
    App &a = *g_app;
    auto it = a.pending.find(m.key);
    const std::string raw = (it != a.pending.end()) ? it->second : settings::instance().getByKey(m.key);
    if (modified) *modified = (it != a.pending.end());
    std::wstring w;
    if (m.type == settings::ValueType::Bool) {
        w = isTrue(raw) ? L"开启" : L"关闭";
    } else {
        w = wide(raw.c_str());
        if (w.size() > 60) w = w.substr(0, 57) + L"…";
    }
    if (it != a.pending.end()) w += L"  *";
    return w;
}

std::wstring hintText(const settings::ConfigMeta &m) {
    std::wstring body;
    if (const Label *l = findLabel(m.key)) {
        body = l->hint ? l->hint : L"";
    } else if (m.description && *m.description) {
        body = wide(m.description);
    } else {
        std::string d = m.defaultValue ? m.defaultValue : "";
        if (d.size() > 36) d = d.substr(0, 33) + "...";
        body = L"默认：" + wide(d.c_str());
    }
    if (!settings::configIsWired(m.key)) return L"[暂未生效] " + body;
    return body;
}

std::wstring nameText(const settings::ConfigMeta &m) {
    if (const Label *l = findLabel(m.key)) return l->name;
    return wide(m.key);
}

void updateStatus() {
    App &a = *g_app;
    std::wstring s;
    if (!a.pending.empty()) {
        s = std::to_wstring(a.pending.size()) + L" 项未保存（* 表示已修改）";
    } else {
        s = L"双击编辑 · 灰色项暂未生效 · 保存后生效";
    }
    SetWindowTextW(a.hStatus, s.c_str());
    EnableWindow(a.hBtnSave, !a.pending.empty());
}

void refreshList() {
    App &a = *g_app;
    SendMessageW(a.hList, WM_SETREDRAW, FALSE, 0);
    SendMessageW(a.hList, LVM_DELETEALLITEMS, 0, 0);

    size_t count = 0;
    const settings::ConfigMeta *metas = settings::allConfigMeta(&count);
    const std::wstring needle = lower(a.filter);
    const char *cat = kCategories[a.catIndex].key;
    const bool all = std::strcmp(cat, "*") == 0;
    const bool unwiredOnly = std::strcmp(cat, "!unwired") == 0;

    int row = 0;
    for (size_t i = 0; i < count; ++i) {
        const settings::ConfigMeta &m = metas[i];
        if (unwiredOnly && settings::configIsWired(m.key)) continue;
        if (!all && !unwiredOnly && std::strcmp(m.category, cat) != 0) continue;
        if (!needle.empty()) {
            std::wstring hay = lower(nameText(m) + L" " + wide(m.key) + L" " + hintText(m) + L" " + valueText(m));
            if (hay.find(needle) == std::wstring::npos) continue;
        }

        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.lParam = static_cast<LPARAM>(i);
        std::wstring name = nameText(m);
        item.pszText = const_cast<LPWSTR>(name.c_str());
        const int idx = static_cast<int>(SendMessageW(a.hList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item)));
        if (idx < 0) continue;

        std::wstring value = valueText(m);
        LVITEMW sub{};
        sub.iSubItem = 1;
        sub.pszText = const_cast<LPWSTR>(value.c_str());
        SendMessageW(a.hList, LVM_SETITEMTEXTW, idx, reinterpret_cast<LPARAM>(&sub));

        std::wstring hint = hintText(m);
        sub.iSubItem = 2;
        sub.pszText = const_cast<LPWSTR>(hint.c_str());
        SendMessageW(a.hList, LVM_SETITEMTEXTW, idx, reinterpret_cast<LPARAM>(&sub));
        ++row;
    }
    SendMessageW(a.hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(a.hList, nullptr, FALSE);
}

void setPending(const char *key, const std::string &value) {
    App &a = *g_app;
    const std::string current = settings::instance().getByKey(key);
    if (value == current) a.pending.erase(key);
    else a.pending[key] = value;
    updateStatus();
    refreshList();
}

bool saveAll() {
    App &a = *g_app;
    if (a.pending.empty()) return true;
    size_t failed = 0;
    for (const auto &kv : a.pending) {
        if (!settings::instance().setByKey(kv.first, kv.second)) ++failed;
    }
    const bool ok = settings::instance().save();
    if (ok) a.pending.clear();
    updateStatus();
    refreshList();
    if (failed || !ok) {
        MessageBoxW(a.hwnd, L"写入配置时出错，请确认数据目录可写。", L"极点五笔 11", MB_ICONERROR | MB_OK);
        return false;
    }
    return true;
}

void editRow(int row) {
    App &a = *g_app;
    if (row < 0) return;
    size_t count = 0;
    const settings::ConfigMeta *metas = settings::allConfigMeta(&count);

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = row;
    if (!SendMessageW(a.hList, LVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&item))) return;
    const size_t idx = static_cast<size_t>(item.lParam);
    if (idx >= count) return;
    const settings::ConfigMeta &m = metas[idx];

    if (!settings::configIsWired(m.key)) {
        const std::wstring msg = L"「" + nameText(m) + L"」目前还没有接入输入法，改了也不会生效。\n\n"
                                 L"它先保留在设置里，等后续版本实现；标灰的项都属于这一类。";
        MessageBoxW(a.hwnd, msg.c_str(), L"暂未生效", MB_OK | MB_ICONINFORMATION);
        return;
    }

    const std::string current = settings::instance().getByKey(m.key);
    auto it = a.pending.find(m.key);
    const std::string shown = (it != a.pending.end()) ? it->second : current;

    if (m.type == settings::ValueType::Bool) {
        setPending(m.key, isTrue(shown) ? "false" : "true");
        return;
    }

    std::wstring title = L"编辑 · " + nameText(m);
    std::wstring hint = hintText(m);
    if (m.type == settings::ValueType::Int) hint += L"（整数）";
    std::wstring def = wide(m.defaultValue ? m.defaultValue : "");
    if (!def.empty() && def.size() < 80) hint += L"\n默认值：" + def;
    std::wstring input = wide(shown.c_str());
    std::wstring result;
    if (!promptValue(a.hwnd, title, hint, input, result)) return;

    std::string value = narrow(result);
    if (m.type == settings::ValueType::Int) {
        try {
            value = std::to_string(std::stoi(value));
        } catch (...) {
            MessageBoxW(a.hwnd, L"这一项只能填整数。", L"极点五笔 11", MB_ICONWARNING | MB_OK);
            return;
        }
    }
    setPending(m.key, value);
}

void openDataDir() {
    const std::string dir = freewb::userFreewbPath();
    std::wstring w = wide(dir.c_str());
    ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void showAbout(HWND parent) {
    MessageBoxW(parent,
        L"极点五笔 11 (FreeIME 11)  v11.2.5.0\n\n"
        L"Windows TSF 输入法现代重制\n"
        L"版权所有 Copyright (c) 2026 Charry\n"
        L"许可证：木兰公共许可证，第 2 版（Mulan PubL v2）\n\n"
        L"GitHub : github.com/bbqldhd/jidian-wubi\n"
        L"Gitee  : gitee.com/red-egg-carrying-ice-cream/jidian-wubi\n"
        L"GitCode: gitcode.com/Charlie_Charry/jidian-wubi",
        L"关于 极点五笔 11", MB_OK | MB_ICONINFORMATION);
}

void layout() {
    App &a = *g_app;
    RECT rc{};
    GetClientRect(a.hwnd, &rc);
    const int W = rc.right - rc.left, H = rc.bottom - rc.top;
    const int pad = a.S(14);
    const int titleH = a.S(46);
    const int bottomH = a.S(56);
    const int catW = a.S(170);
    const int gap = a.S(10);
    const int searchW = a.S(230), searchH = a.S(26);

    MoveWindow(a.hTitle, pad, pad, W - pad * 2 - searchW - gap, titleH, TRUE);
    MoveWindow(a.hSearch, W - pad - searchW, pad + a.S(9), searchW, searchH, TRUE);

    const int top = pad + titleH + gap;
    const int bodyH = H - top - bottomH - pad;
    MoveWindow(a.hCats, pad, top, catW, bodyH, TRUE);
    const int listX = pad + catW + gap;
    const int listW = W - listX - pad;
    MoveWindow(a.hList, listX, top, listW, bodyH, TRUE);

    const int col0 = a.S(210), col1 = a.S(120);
    SendMessageW(a.hList, LVM_SETCOLUMNWIDTH, 0, col0);
    SendMessageW(a.hList, LVM_SETCOLUMNWIDTH, 1, col1);
    const int col2 = listW - col0 - col1 - a.S(6);
    SendMessageW(a.hList, LVM_SETCOLUMNWIDTH, 2, col2 > a.S(160) ? col2 : a.S(160));

    const int by = H - pad - a.S(30);
    const int bw = a.S(80), bh = a.S(30), bs = a.S(8);
    int x = W - pad;
    // 从右往左：保存 / 还原 / 恢复默认 / 打开数据目录 / 关于 / 关闭
    const HWND order[] = {a.hBtnSave, a.hBtnRevert, a.hBtnDefaults, a.hBtnOpenData, a.hBtnAbout, a.hBtnClose};
    for (HWND h : order) {
        x -= bw;
        MoveWindow(h, x, by, bw, bh, TRUE);
        x -= bs;
    }
    MoveWindow(a.hStatus, pad, by + a.S(3), x - pad, a.S(24), TRUE);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App &a = *g_app;
    switch (msg) {
    case WM_CREATE: {
        a.hwnd = hwnd;
        // 视觉样式 + DPI
        INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&icc);

        HDC dc = GetDC(nullptr);
        a.dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(nullptr, dc);
        if (a.dpi < 96) a.dpi = 96;

        auto makeFont = [&](int pt, bool bold) -> HFONT {
            return CreateFontW(-MulDiv(pt, a.dpi, 72), 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
                               FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                               L"Microsoft YaHei UI");
        };
        a.font = makeFont(9, false);
        a.fontTitle = makeFont(15, true);

        const HINSTANCE hinst = GetModuleHandleW(nullptr);
        auto setFont = [&](HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE); };

        a.hTitle = CreateWindowW(L"STATIC", L"极点五笔 11 · 设置",
                                 WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, hwnd,
                                 reinterpret_cast<HMENU>(IDC_TITLE), hinst, nullptr);
        a.hSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                    0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(IDC_SEARCH), hinst, nullptr);
        SendMessageW(a.hSearch, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"搜索设置项…"));

        a.hCats = CreateWindowW(L"LISTBOX", L"",
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_TABSTOP,
                                0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(IDC_CATS), hinst, nullptr);
        for (const auto &c : kCategories) {
            SendMessageW(a.hCats, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(c.name));
        }
        SendMessageW(a.hCats, LB_SETCURSEL, 0, 0);

        a.hList = CreateWindowW(WC_LISTVIEWW, L"",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_ALIGNTOP,
                                0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(IDC_LIST), hinst, nullptr);
        SendMessageW(a.hList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                     LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.cx = 210; col.pszText = const_cast<LPWSTR>(L"项目"); col.iSubItem = 0;
        SendMessageW(a.hList, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>(&col));
        col.cx = 120; col.pszText = const_cast<LPWSTR>(L"当前值"); col.iSubItem = 1;
        SendMessageW(a.hList, LVM_INSERTCOLUMNW, 1, reinterpret_cast<LPARAM>(&col));
        col.cx = 300; col.pszText = const_cast<LPWSTR>(L"说明"); col.iSubItem = 2;
        SendMessageW(a.hList, LVM_INSERTCOLUMNW, 2, reinterpret_cast<LPARAM>(&col));

        a.hStatus = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(IDC_STATUS), hinst, nullptr);
        auto button = [&](int id, const wchar_t *text) -> HWND {
            return CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                 0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(id), hinst, nullptr);
        };
        a.hBtnSave = button(IDC_BTN_SAVE, L"保存");
        a.hBtnRevert = button(IDC_BTN_REVERT, L"还原");
        a.hBtnDefaults = button(IDC_BTN_DEFAULTS, L"恢复默认");
        a.hBtnOpenData = button(IDC_BTN_OPENDATA, L"打开数据目录");
        a.hBtnAbout = button(IDC_BTN_ABOUT, L"关于");
        a.hBtnClose = button(IDC_BTN_CLOSE, L"关闭");

        const HWND all[] = {a.hTitle, a.hSearch, a.hCats, a.hList, a.hStatus,
                            a.hBtnSave, a.hBtnRevert, a.hBtnDefaults, a.hBtnOpenData, a.hBtnAbout, a.hBtnClose};
        for (HWND h : all) setFont(h, a.font);
        setFont(a.hTitle, a.fontTitle);

        refreshList();
        updateStatus();
        return 0;
    }

    case WM_SIZE:
        layout();
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(lp);
        mmi->ptMinTrackSize.x = a.S(720);
        mmi->ptMinTrackSize.y = a.S(460);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        SetBkColor(reinterpret_cast<HDC>(wp), RGB(255, 255, 255));
        return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
    }

    case WM_COMMAND: {
        const int id = LOWORD(wp), code = HIWORD(wp);
        if (code == EN_CHANGE && id == IDC_SEARCH) {
            wchar_t buf[512]{};
            GetWindowTextW(a.hSearch, buf, 512);
            a.filter = buf;
            refreshList();
            return 0;
        }
        if (code == LBN_SELCHANGE && id == IDC_CATS) {
            const int sel = static_cast<int>(SendMessageW(a.hCats, LB_GETCURSEL, 0, 0));
            if (sel >= 0 && sel < static_cast<int>(sizeof(kCategories) / sizeof(kCategories[0]))) {
                a.catIndex = sel;
                refreshList();
            }
            return 0;
        }
        if (code == BN_CLICKED) {
            switch (id) {
            case IDC_BTN_SAVE:
                if (saveAll()) {
                    MessageBoxW(hwnd, L"设置已保存。\n正在运行的程序会在下一次输入结束时读取新配置。",
                                L"极点五笔 11", MB_OK | MB_ICONINFORMATION);
                }
                return 0;
            case IDC_BTN_REVERT:
                a.pending.clear();
                updateStatus();
                refreshList();
                return 0;
            case IDC_BTN_DEFAULTS:
                if (MessageBoxW(hwnd, L"把所有设置恢复为默认值？当前改动会一并丢弃。",
                                L"极点五笔 11", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    settings::instance().restoreAllDefaults();
                    settings::instance().save();
                    a.pending.clear();
                    updateStatus();
                    refreshList();
                }
                return 0;
            case IDC_BTN_OPENDATA: openDataDir(); return 0;
            case IDC_BTN_ABOUT: showAbout(hwnd); return 0;
            case IDC_BTN_CLOSE: PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
            default: break;
            }
        }
        break;
    }

    case WM_NOTIFY: {
        NMHDR *nm = reinterpret_cast<NMHDR *>(lp);
        if (nm->idFrom != IDC_LIST) break;
        if (nm->code == NM_DBLCLK) {
            const int row = static_cast<int>(SendMessageW(a.hList, LVM_GETNEXTITEM, static_cast<WPARAM>(-1),
                                                          LVNI_SELECTED));
            editRow(row);
            return 0;
        }
        if (nm->code == NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW *cd = reinterpret_cast<NMLVCUSTOMDRAW *>(lp);
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: return CDRF_NOTIFYSUBITEMDRAW;
            case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
                size_t count = 0;
                const settings::ConfigMeta *metas = settings::allConfigMeta(&count);
                const size_t idx = static_cast<size_t>(cd->nmcd.lItemlParam);
                const bool wired = idx < count && settings::configIsWired(metas[idx].key);
                if (!wired) {
                    cd->clrText = RGB(155, 155, 155);
                    return CDRF_DODEFAULT;
                }
                if (cd->iSubItem == 2) {
                    cd->clrText = RGB(85, 85, 85);
                    return CDRF_DODEFAULT;
                }
                if (cd->iSubItem == 1) {
                    bool modified = false;
                    const std::wstring v = valueText(metas[idx], &modified);
                    if (modified) cd->clrText = RGB(190, 90, 0);
                    else if (metas[idx].type == settings::ValueType::Bool)
                        cd->clrText = (v == L"开启") ? RGB(0, 90, 158) : RGB(120, 120, 120);
                }
                return CDRF_DODEFAULT;
            }
        }
        break;
    }

    case WM_CLOSE: {
        if (!a.pending.empty()) {
            const int r = MessageBoxW(hwnd, L"还有未保存的改动，是否保存？", L"极点五笔 11",
                                      MB_YESNOCANCEL | MB_ICONQUESTION);
            if (r == IDCANCEL) return 0;
            if (r == IDYES && !saveAll()) return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY:
        if (a.font) DeleteObject(a.font);
        if (a.fontTitle) DeleteObject(a.fontTitle);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE, LPWSTR cmd, int) {
    // 保留原有的命令行入口（工具栏/脚本用），无参数时打开 GUI
    if (cmd && *cmd) {
        if (std::wcsstr(cmd, L"--opendata")) {
            const std::wstring dir = wide(freewb::userFreewbPath().c_str());
            ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        if (std::wcsstr(cmd, L"--register")) {
            MessageBoxW(nullptr, L"请以管理员身份运行：\nregsvr32 FreeIME11.dll\n"
                        L"并在 设置→时间和语言→语言和区域 中添加「极点五笔 11」。",
                        L"极点五笔 11", MB_OK);
            return 0;
        }
        if (std::wcsstr(cmd, L"--unregister")) {
            MessageBoxW(nullptr, L"请以管理员身份运行：\nregsvr32 /u FreeIME11.dll", L"极点五笔 11", MB_OK);
            return 0;
        }
    }

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hinst;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWndClass;
    if (!RegisterClassExW(&wc)) return 1;

    App app;
    g_app = &app;

    HDC dc = GetDC(nullptr);
    app.dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    if (app.dpi < 96) app.dpi = 96;
    const int winW = app.S(880), winH = app.S(600);

    const int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(0, kWndClass, L"极点五笔 11 · 设置",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                (sw - winW) / 2, (sh - winH) / 2, winW, winH,
                                nullptr, nullptr, hinst, &app);
    if (!hwnd) return 1;
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
