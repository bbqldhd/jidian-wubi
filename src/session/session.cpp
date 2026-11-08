// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// FreeIME 11 —— 输入会话实现（跨平台）
// 复刻极点五笔 10.8.9 的按键/候选/标点/简繁/造词行为。
#include "session.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <unordered_map>

#include "../engine/en.h"
#include "../engine/py.h"
#include "../engine/wbpy.h"
#include "../engine/wbzx.h"
#include "../utils/settings.h"
#include "../utils/utils.h"

// 虚拟键码（无 windows.h 时的跨平台常量；与 WinUser.h 一致）
#ifndef VK_BACK
#define VK_BACK 0x08
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_RSHIFT 0xA1
#endif

namespace freeime11 {

using freewb::CandidatePayload;
using freewb::En;
using freewb::PyEngine;
using freewb::Wbpy;
using freewb::WbzxEngine;

namespace {

// 中文标点映射（对齐 10.8.9 symbol.txt 的默认值，并补齐引号/书名号/方头括号）
const std::unordered_map<char, const char *> kCnPunc = {
    {'`', "`"},   {'~', "～"},  {'!', "！"}, {'@', "·"},  {'#', "#"},   {'$', "￥"},
    {'%', "%"},   {'^', "…"},   {'&', "—"},  {'*', "*"},  {'(', "（"},  {')', "）"},
    {'-', "－"},  {'_', "—"},   {'=', "＝"}, {'+', "＋"}, {'[', "【"},  {'{', "『"},
    {']', "】"},  {'}', "』"},  {'\\', "、"},{'|', "｜"}, {';', "；"},  {':', "："},
    {',', "，"},  {'<', "《"},  {'.', "。"}, {'>', "》"}, {'/', "／"},  {'?', "？"},
};

// 需要成对输出的标点：首次给左符，再次给右符（引号类无法把光标塞进中间，只能这样交替）
struct PuncPair {
    char ascii;
    const char *open;
    const char *close;
};
const PuncPair kPuncPairs[] = {
    {'"', "\xE2\x80\x9C", "\xE2\x80\x9D"},   // “ ”
    {'\'', "\xE2\x80\x98", "\xE2\x80\x99"},  // ‘ ’
};

const char *puncPairAt(char c) {
    for (const PuncPair &p : kPuncPairs) {
        if (p.ascii == c) return p.open;
    }
    return nullptr;
}

// ASCII → 全角（全角模式下没有中文标点映射的字符走这里）
std::string toFullWidth(char c) {
    unsigned int cp = static_cast<unsigned char>(c);
    if (cp >= 0x21 && cp <= 0x7E) cp += 0xFEE0;
    else if (cp == 0x20) cp = 0x3000;
    else return std::string(1, c);

    std::string out;
    if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

bool isLetterCode(char c) { return (c >= 'a' && c <= 'z'); }

// 这个字符在中文标点模式下是否会变成别的字符（决定要不要接管这个按键）
bool needsPuncConversion(char c, bool chinesePunc, bool fullShape) {
    if (chinesePunc) {
        if (puncPairAt(c)) return true;
        const auto it = kCnPunc.find(c);
        // 映射成自己（# % * `）不算转换：这种键必须留给应用
        if (it != kCnPunc.end() && (it->second[0] != c || it->second[1] != '\0')) return true;
    }
    return fullShape && c > 0x20 && c < 0x7F;
}

bool isDigit(char c) { return (c >= '0' && c <= '9'); }

} // namespace

struct Session::Impl {
    // 引擎（构造顺序：wbzx/py 先，wbpy 依赖前两者）
    std::unique_ptr<WbzxEngine> wbzx;
    std::unique_ptr<PyEngine> py;
    std::unique_ptr<Wbpy> wbpy;
    std::unique_ptr<En> en;

    int engineIdx = 1; // 0 wbzx, 1 wbpy, 2 py, 3 en（默认五笔拼音）
    enum EngineKind { kWbzx = 0, kWbpy = 1, kPy = 2, kEn = 3 };

    // 会话状态
    SessionState state = SessionState::Idle;
    std::string inputCode_;   // 当前编码（不含引导符）
    std::string lead_;        // 引导符（; 命令 / ` 生僻 / z 临时拼音 / www. 等临时英文前缀）
    std::string tempEngPrefix_; // 临时英文已捕获的前缀
    CandidatePayload cand_;   // 引擎最近一次候选
    int pageIndex_ = 0;
    int pageSize_ = 5;
    int candCursor_ = 0;      // 当前页内高亮候选（0 起），↑/↓ 移动
    bool commitExactPending_ = false;

    // 引号配对状态（“” 与 ‘’ 各一套）。不随组字复位：它是文档级上下文，
    // 清掉的话每次上屏后又会从“左引号”重新开始。
    bool quoteOpen_[2] = {true, true};

    bool isEnglish() const { return engineIdx == kEn; }

    // 简繁转换钩子（前端注入 opencc 或旧版映射）
    std::function<std::string(const std::string &)> tradConverter;

    std::string userFreewbPath() { return freewb::userFreewbPath(); }

    freewb::IFreewbEngine *cur() const {
        switch (engineIdx) {
        case kWbpy: return wbpy.get();
        case kPy:   return py.get();
        case kEn:   return en.get();
        default:    return wbzx.get();
        }
    }

    bool isWubiFamily() const { return engineIdx == kWbzx || engineIdx == kWbpy; }

    void refresh() {
        if (cur()) cur()->putKey(inputCode_.c_str());
        cand_ = cur() ? cur()->getResult() : CandidatePayload{};
        pageIndex_ = 0;
        candCursor_ = 0;
    }

    void resetSession() {
        state = SessionState::Idle;
        inputCode_.clear();
        lead_.clear();
        tempEngPrefix_.clear();
        cand_ = CandidatePayload{};
        pageIndex_ = 0;
        candCursor_ = 0;
        commitExactPending_ = false;
        if (cur()) cur()->reset();
    }

    std::string applyTrad(const std::string &s) const {
        if (!settings::instance().get_simpTradFlg() || !tradConverter) return s;
        return tradConverter(s);
    }

    // ASCII 标点 → 实际要上屏的字符串（中文标点 / 全角 / 原样）
    std::string punctuation(char c, bool chinesePunc, bool fullShape) {
        if (chinesePunc) {
            for (int i = 0; i < 2; ++i) {
                if (kPuncPairs[i].ascii != c) continue;
                // 引号成对：左、右交替（关掉智能配对就固定输出左引号）
                const bool autoPair = settings::instance().get_puncAutoPair();
                const bool open = quoteOpen_[i] || !autoPair;
                if (autoPair) quoteOpen_[i] = !quoteOpen_[i];
                return open ? kPuncPairs[i].open : kPuncPairs[i].close;
            }
            const auto it = kCnPunc.find(c);
            if (it != kCnPunc.end()) return it->second;
        }
        if (fullShape) return toFullWidth(c);
        return std::string(1, c);
    }

    int pageCount() const {
        const int n = static_cast<int>(cand_.texts.size());
        if (n == 0) return 0;
        return (n + pageSize_ - 1) / pageSize_;
    }

    void pageRange(int &begin, int &end) const {
        begin = pageIndex_ * pageSize_;
        end = std::min<int>(begin + pageSize_, cand_.texts.size());
    }

    // 把高亮游标限制在当前页内
    void clampCursor() {
        int b = 0, e = 0;
        pageRange(b, e);
        const int n = e - b;
        if (n <= 0) { candCursor_ = 0; return; }
        if (candCursor_ < 0) candCursor_ = 0;
        if (candCursor_ >= n) candCursor_ = n - 1;
    }

    // 当前高亮候选的全局下标
    int currentCandidateIndex() const { return pageIndex_ * pageSize_ + candCursor_; }

    // 组装 preedit / 候选页（processKey 尾部、翻页与鼠标选词共用）
    void fillOutcome(SessionOutcome &out) {
        out.state = state;
        out.englishMode = isEnglish();
        if (state == SessionState::Idle || inputCode_.empty()) return;

        out.preedit = lead_ + inputCode_;
        out.handled = true;
        int b = 0, e = 0;
        pageRange(b, e);
        for (int i = b; i < e; ++i) {
            out.cands.push_back(cand_.texts[i]);
            out.candPrompts.push_back(i < static_cast<int>(cand_.prompts.size()) ? cand_.prompts[i] : std::string());
            out.candCodes.push_back(i < static_cast<int>(cand_.fullCodes.size()) ? cand_.fullCodes[i] : std::string());
        }
        out.candHasPrev = pageIndex_ > 0;
        out.candHasNext = pageIndex_ + 1 < pageCount();
        out.candCursor = candCursor_;
    }
};

Session::Session() : impl_(new Impl()) {
    auto &im = *impl_;
    // 读取配置（settings::instance() 已内置 10.8.9 兼容默认值）
    settings::instance().reload();
    im.pageSize_ = std::max(1, settings::instance().get_candiWordCount());

    im.wbzx = std::make_unique<WbzxEngine>();
    im.py = std::make_unique<PyEngine>();
    im.wbpy = std::make_unique<Wbpy>(im.wbzx.get(), im.py.get());
    im.en = std::make_unique<En>();

    applyEngineFromSettings();
}

void Session::applyEngineFromSettings() {
    auto &im = *impl_;
    const std::string mode = settings::instance().get_inputMode();
    if (mode.rfind("engine:", 0) != 0) return;

    const std::string name = mode.substr(7);
    int idx = im.engineIdx;
    if (name == "wbzx") idx = Impl::kWbzx;
    else if (name == "wbpy") idx = Impl::kWbpy;
    else if (name == "py") idx = Impl::kPy;
    else if (name == "en") idx = Impl::kEn;
    else return;

    if (idx != im.engineIdx) {
        im.engineIdx = idx;
        im.resetSession();
    }
}

Session::~Session() = default;

bool Session::loadSettings(const std::string &dataDir) {
    (void)dataDir;
    settings::instance().reload();
    impl_->pageSize_ = std::max(1, settings::instance().get_candiWordCount());
    applyEngineFromSettings();
    return true;
}

void Session::reset() { impl_->resetSession(); }

bool Session::chinesePunc() const { return settings::instance().get_chinesePunc(); }
bool Session::fullShape() const { return settings::instance().get_fullWidthFlg(); }
bool Session::simpTrad() const { return settings::instance().get_simpTradFlg(); }
bool Session::englishMode() const { return impl_->isEnglish(); }
bool Session::hasPendingInput() const {
    const auto &im = *impl_;
    return !im.inputCode_.empty() || !im.lead_.empty();
}

std::string Session::rawPreedit() const {
    const auto &im = *impl_;
    if (im.inputCode_.empty() && im.lead_.empty()) return std::string();
    return im.lead_ + im.inputCode_;
}
std::string Session::engineName() const {
    switch (impl_->engineIdx) {
    case Impl::kWbpy: return "wbpy";
    case Impl::kPy:   return "py";
    case Impl::kEn:   return "en";
    default:          return "wbzx";
    }
}

void Session::toggleFullShape() { settings::instance().set_fullWidthFlg(!fullShape()); }
void Session::toggleChinesePunc() { settings::instance().set_chinesePunc(!chinesePunc()); }
void Session::toggleSimpTrad() { settings::instance().set_simpTradFlg(!simpTrad()); }

void Session::cycleMode() {
    auto &im = *impl_;
    // 四模式固定顺序，左右 Shift 单击都按这条链走
    static const int kOrder[] = {Impl::kWbpy, Impl::kWbzx, Impl::kPy, Impl::kEn};
    constexpr int kCount = static_cast<int>(sizeof(kOrder) / sizeof(kOrder[0]));

    bool schemeOn[3] = {settings::instance().get_WbzxEngine(), settings::instance().get_WbpyEngine(),
                        settings::instance().get_PyEngine()};
    if (!schemeOn[0] && !schemeOn[1] && !schemeOn[2]) schemeOn[0] = schemeOn[1] = schemeOn[2] = true;

    int pos = 0;
    for (int i = 0; i < kCount; ++i) {
        if (kOrder[i] == im.engineIdx) { pos = i; break; }
    }

    int next = im.engineIdx;
    for (int step = 1; step <= kCount; ++step) {
        const int cand = kOrder[(pos + step) % kCount];
        // 英文档永远可用；三种编码方案受设置开关控制，关掉的跳过
        if (cand == Impl::kEn || schemeOn[cand]) { next = cand; break; }
    }

    im.engineIdx = next;
    im.resetSession();

    // 只记住编码方案：英文是临时状态，重启后不该把用户锁在英文键盘上
    if (next != Impl::kEn) settings::instance().set_inputMode("engine:" + engineName());
}

char vkToCodeChar(int vk, bool shift) {
    /* Windows 送来的是 VK_OEM_* 这类虚拟键码，不是 ASCII：
       VK_OEM_PERIOD 是 0xBE，而 '.' 是 0x2E，两者不相等。
       所以先把 VK_OEM_* 归一化成 ASCII 字符，再做上档映射。

       ★ 归一化必须走独立的局部变量：ASCII 标点和虚拟键码存在**撞号**，
         VK_DELETE=0x2E 就是 '.'、VK_INSERT=0x2D 就是 '-'、VK_SNAPSHOT=0x2C 就是 ','
         、VK_LWIN=0x5B 就是 '['、VK_NUMPAD0=0x60 就是 '`'。
         如果把裸 ASCII 也当输入收下，Delete 会变成「。」、Win 键会变成「【」。 */
    char ch = 0;
#ifdef VK_OEM_1
    switch (vk) {
    case VK_OEM_1:      ch = ';';  break;   // ; :
    case VK_OEM_PLUS:   ch = '=';  break;   // = +
    case VK_OEM_COMMA:  ch = ',';  break;   // , <
    case VK_OEM_MINUS:  ch = '-';  break;   // - _
    case VK_OEM_PERIOD: ch = '.';  break;   // . >
    case VK_OEM_2:      ch = '/';  break;   // / ?
    case VK_OEM_3:      ch = '`';  break;   // ` ~
    case VK_OEM_4:      ch = '[';  break;   // [ {
    case VK_OEM_5:      ch = '\\'; break;   // \ |
    case VK_OEM_6:      ch = ']';  break;   // ] }
    case VK_OEM_7:      ch = '\''; break;   // ' "
    default: break;
    }
#endif

    if (ch) {
        // 上档符号（只对归一化出来的符号键生效）
        switch (ch) {
        case '`':  return shift ? '~' : ch;
        case '-':  return shift ? '_' : ch;
        case '=':  return shift ? '+' : ch;
        case '[':  return shift ? '{' : ch;
        case ']':  return shift ? '}' : ch;
        case '\\': return shift ? '|' : ch;
        case ';':  return shift ? ':' : ch;
        case '\'': return shift ? '"' : ch;
        case ',':  return shift ? '<' : ch;
        case '.':  return shift ? '>' : ch;
        case '/':  return shift ? '?' : ch;
        default:   return ch;
        }
    }

    // 字母和数字的虚拟键码本来就和 ASCII 同值（0x41–0x5A / 0x30–0x39）
    if (vk >= 'A' && vk <= 'Z') return static_cast<char>(vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9') {
        if (!shift) return static_cast<char>(vk);
        static const char kNumShift[10] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
        return kNumShift[vk - '0'];
    }

    // 其余一律不认。不要在这里按 ASCII 判断标点，否则会吃掉 Delete/Insert/PrintScreen/Win 等键。
    return 0;
}

// 配置键名 → 字符（映射 10.8.9 的键名约定）
static char keyNameToChar(const std::string &name) {
    if (name.find("SEMICOLON") != std::string::npos) return ';';
    if (name.find("QUOTE") != std::string::npos) return '\'';
    if (name.find("DASH") != std::string::npos) return '-';
    if (name.find("EQUAL") != std::string::npos) return '=';
    if (name.find("SLASH") != std::string::npos) return '/';
    if (name.find("BACKSPACE") != std::string::npos) return '\b';
    if (name.find("SPACE") != std::string::npos) return ' ';
    if (name.find("RETURN") != std::string::npos || name.find("ENTER") != std::string::npos) return '\n';
    if (name.size() == 1) return name[0];
    return 0;
}

SessionOutcome Session::processKey(int vk, bool shift, bool ctrl, bool alt) {
    auto &im = *impl_;
    SessionOutcome out;
    out.state = im.state;
    out.englishMode = im.isEnglish();

    // Ctrl 快捷键（全局，无论哪个模式）
    if (ctrl && !alt && !shift) {
        switch (vk) {
        case 'M': toggleCharset(); out.handled = true; break;
        case ',': /* 设置面板（前端处理） */ out.handled = true; break;
        case ' ': // 键盘模式循环（备用；常规入口是左右 Shift 单击）
            cycleMode(); out.handled = true; break;
        default: break;
        }
        if (out.handled) { out.state = im.state; out.englishMode = im.isEnglish(); return out; }
    }

    // 左右 Shift 单击切模式（TSF 前端另有“按下不切、松开才切”的判定，这里守住状态机）
    if (shift && (vk == VK_RSHIFT || vk == 0xA0 /*VK_LSHIFT*/) && !ctrl && !alt) {
        // 组字中按 Shift：先把输入框里的原文上屏（和回车一个效果），再切模式。
        // 顺序不能反——cycleMode() 会复位会话，编码串就没了。
        if (!im.inputCode_.empty() || !im.lead_.empty()) {
            out.commitText = im.applyTrad(im.lead_ + im.inputCode_);
        }
        cycleMode();
        out.handled = true; out.state = im.state; out.englishMode = im.isEnglish();
        return out;
    }

    // 英文模式：整键交给应用处理，输入法不吞键（自己插字符会破坏快捷键与自动补全）
    if (im.isEnglish() && im.state == SessionState::Idle) {
        return out;
    }

    // 组合/特殊状态路由
    if (im.state == SessionState::AddPhrase || im.state == SessionState::DeletePhrase) {
        // 在线造词/删词确认（回车=确认，Esc=取消）
        if (vk == VK_RETURN) {
            // 确认动作由进入状态时缓存（简化：直接复位）
            im.resetSession();
            out.handled = true; out.state = im.state;
            return out;
        }
        if (vk == VK_ESCAPE) {
            im.resetSession();
            out.handled = true; out.state = im.state;
            return out;
        }
        out.handled = true; out.state = im.state;
        return out;
    }

    if (im.state == SessionState::TempEnglish) {
        // 临时英文：回车转回中文，Esc 取消，其余字符直接上屏
        if (vk == VK_RETURN || vk == VK_ESCAPE) {
            im.resetSession();
            out.handled = true; out.state = im.state;
            return out;
        }
        char c = vkToCodeChar(vk, shift);
        if (c) { out.commitText.assign(1, c); }
        out.handled = true; out.state = im.state;
        return out;
    }

    // 命令直通车（; 引导）
    if (!im.lead_.empty() && im.lead_[0] == ';') {
        if (vk == VK_RETURN) {
            // 提交命令（前端执行 ShellExecute）；此处仅作为文本上屏
            out.commitText = im.inputCode_;
            im.resetSession();
            out.handled = true; out.state = im.state;
            return out;
        }
        if (vk == VK_ESCAPE) { im.resetSession(); out.handled = true; out.state = im.state; return out; }
        if (vk == VK_BACK) {
            if (!im.inputCode_.empty()) im.inputCode_.pop_back();
            out.preedit = im.lead_ + im.inputCode_;
            out.handled = true; out.state = im.state;
            return out;
        }
        char c = vkToCodeChar(vk, shift);
        if (c) { im.inputCode_.push_back(c); out.preedit = im.lead_ + im.inputCode_; }
        out.handled = true; out.state = im.state;
        return out;
    }

    // 空闲态（无编码）：这些键属于应用，输入法不能吃
    if (im.inputCode_.empty() && im.state == SessionState::Idle) {
        if (vk == VK_SPACE || vk == VK_RETURN || vk == VK_BACK || vk == VK_ESCAPE || vk == VK_TAB) {
            return out;   // handled = false
        }
    }

    // 临时生僻字（` 引导，GBK）与临时拼音（z 引导）
    const bool tmpPinyin = !im.inputCode_.empty() && im.inputCode_[0] == 'z' && im.isWubiFamily();

    switch (vk) {
    case VK_BACK:
        if (!im.inputCode_.empty()) {
            im.inputCode_.pop_back();
            if (im.inputCode_.empty()) { im.lead_.clear(); im.resetSession(); out.state = im.state; }
            else { im.refresh(); out.state = im.state; }
        }
        out.handled = true;
        break;

    case VK_ESCAPE:
        im.resetSession();
        out.handled = true; out.state = im.state;
        break;

    case VK_RETURN: {
        /* 回车：把「候选框输入框里的原文」原样上屏，不是候选词。
           空格/数字才是「选词上屏」的语义；回车留给「我就是要打这串编码」。 */
        if (!im.inputCode_.empty() || !im.lead_.empty()) {
            out.commitText = im.applyTrad(im.lead_ + im.inputCode_);
        } else if (!im.cand_.texts.empty()) {
            // 没有输入串却还有候选（少见）：仍按老规矩上屏高亮候选
            int b = 0, e = 0; im.pageRange(b, e);
            const int idx = b + im.candCursor_;
            out.commitText = im.applyTrad(im.cand_.texts[idx < e ? idx : b]);
        }
        im.resetSession();
        out.handled = true; out.state = im.state;
        break;
    }

    case VK_UP:
    case VK_DOWN:
    case VK_PRIOR:
    case VK_NEXT: {
        // ↑/↓ 在当前页内移动高亮；PageUp/PageDown 翻页
        if (im.cand_.texts.empty()) { out.handled = true; out.state = im.state; break; }

        int b = 0, e = 0; im.pageRange(b, e);
        const int n = e - b;
        switch (vk) {
        case VK_UP:    im.candCursor_ = std::max(0, im.candCursor_ - 1); break;
        case VK_DOWN:  im.candCursor_ = std::min(n - 1, im.candCursor_ + 1); break;
        case VK_PRIOR:
            if (im.pageIndex_ > 0) { --im.pageIndex_; im.candCursor_ = 0; }
            break;
        case VK_NEXT:
            if (im.pageIndex_ + 1 < im.pageCount()) { ++im.pageIndex_; im.candCursor_ = 0; }
            break;
        default: break;
        }
        im.clampCursor();
        out.handled = true; out.state = im.state;
        break;
    }

    case VK_SPACE: {
        // 空格：上屏当前高亮候选（默认第一个）；无候选且空码→上屏空格
        if (!im.cand_.texts.empty()) {
            int b = 0, e = 0; im.pageRange(b, e);
            const int idx = b + im.candCursor_;
            out.commitText = im.applyTrad(im.cand_.texts[idx < e ? idx : b]);
            im.resetSession();
        } else if (im.inputCode_.empty()) {
            out.commitText = fullShape() ? "　" : " ";
        }
        out.handled = true; out.state = im.state;
        break;
    }

    default: {
        // 候选选择键（数字 / 分号 / 引号）
        char c = vkToCodeChar(vk, shift);
        if (c && isDigit(c) && !im.cand_.texts.empty()) {
            int n = c - '0';
            if (n == 0) n = 10;
            int b = 0, e = 0; im.pageRange(b, e);
            if (n >= 1 && b + n - 1 < e) {
                out.commitText = im.applyTrad(im.cand_.texts[b + n - 1]);
                im.resetSession();
                out.handled = true; out.state = im.state;
                return out;
            }
        }
        // 第二/第三重码：; 和 '（默认，可通过配置键名重映射）
        const char secondKey = keyNameToChar(settings::instance().get_secondRecodeKey());
        const char thirdKey = keyNameToChar(settings::instance().get_thirdRecodeKey());
        if (secondKey && c == secondKey && !shift && im.cand_.texts.size() >= 2) {
            out.commitText = im.applyTrad(im.cand_.texts[1]);
            im.resetSession(); out.handled = true; out.state = im.state; return out;
        }
        if (thirdKey && c == thirdKey && !shift && im.cand_.texts.size() >= 3) {
            out.commitText = im.applyTrad(im.cand_.texts[2]);
            im.resetSession(); out.handled = true; out.state = im.state; return out;
        }
        // 翻页 - / =：只有正在选词时才是翻页键。空闲时它们是普通标点，
        // 不能在这里被吃掉，否则「－」「＝」永远打不出来。
        if ((c == '-' || c == '=') && !im.cand_.texts.empty()) {
            if (c == '-') {
                if (im.pageIndex_ > 0) { --im.pageIndex_; im.candCursor_ = 0; }
            } else if (im.pageIndex_ + 1 < im.pageCount()) {
                ++im.pageIndex_; im.candCursor_ = 0;
            }
            out.handled = true;   // 有候选时归输入法，翻不动也算消费掉
        }
        else if (c && isLetterCode(c)) {
            // 编码输入
            im.inputCode_.push_back(c);
            im.state = SessionState::Composing;
            out.handled = true;

            // 临时拼音引导 z
            if (tmpPinyin && im.isWubiFamily()) { /* 保留：z 引导临时拼音 */ }
            im.refresh();

            // 自动上屏：只对「确定的完整编码」生效。
            // 拼音（含五笔拼音的拼音部分）一律不自动上屏 —— 整句候选常常只有一条，
            // 若沿用「2 码唯一」，打到 woxihu 就会被截断成「我西湖」，一句话永远打不完。
            if (im.cand_.texts.size() == 1) {
                const std::string code = im.inputCode_;
                const bool wubiExact = im.wbzx && im.wbzx->isExactDictionaryKey(code);
                bool autoCommit = false;
                if (im.engineIdx == Impl::kWbzx) {
                    autoCommit = (code.size() >= 4) || wubiExact;   // 五笔：满 4 码唯一 / 简码唯一
                } else if (im.engineIdx == Impl::kWbpy) {
                    autoCommit = (code.size() >= 4) && wubiExact;   // 五笔拼音：只认纯五笔编码
                }                                                    // 拼音 / 英文：不自动上屏
                if (autoCommit) {
                    out.commitText = im.applyTrad(im.cand_.texts[0]);
                    im.resetSession();
                    out.handled = true; out.state = im.state;
                    return out;
                }
            }
        } else if (c && isDigit(c)) {
            // 空闲态的数字交给应用；但组字中（例如拼音整句还没出候选）必须吞掉，
            // 否则「1」会被打进文档，把正在拼的编码打断。
            // 这里要把当前编码原样回填，否则上层会当成“空结果”把组合区清掉。
            if (im.inputCode_.empty() && im.state == SessionState::Idle) {
                // 全角模式：数字也要全角，否则「全角」只对字母生效，很割裂
                if (fullShape()) {
                    out.commitText = toFullWidth(c);
                    out.handled = true; out.state = im.state;
                    return out;
                }
                return out;
            }
            im.fillOutcome(out);
            out.handled = true;
            return out;
        } else if (c) {
            // 标点：有候选→顶字上屏；无候选→标点转换
            // 空闲且这个字符不需要转换（中文标点/全角都关）就整键放行，
            // 别把应用原本的按键行为替换成一次等价的自插字符。
            const bool convert = needsPuncConversion(c, chinesePunc(), fullShape());
            if (!convert && im.cand_.texts.empty() && im.inputCode_.empty()) return out;

            std::string candText;
            if (!im.cand_.texts.empty()) {
                int b = 0, e = 0; im.pageRange(b, e);
                candText = im.applyTrad(im.cand_.texts[b]);
            }
            const std::string punc = im.punctuation(c, chinesePunc(), fullShape());
            im.resetSession();

            out.commitText = candText + punc;
            out.handled = true; out.state = im.state;
            return out;
        }
        break;
    }
    }

    // 组装输出：preedit / 候选
    im.fillOutcome(out);
    return out;
}

bool Session::wantsKey(int vk, bool shift, bool ctrl, bool alt) const {
    auto &im = *impl_;

    // 系统快捷键一律放行：Ctrl/Alt/Win 组合不归输入法（否则 Ctrl+空格 切不了输入法）
    if (ctrl || alt) return false;
    if (::GetKeyState(VK_LWIN) & 0x8000) return false;
    if (::GetKeyState(VK_RWIN) & 0x8000) return false;

    const bool composing = (im.state != SessionState::Idle) && !im.inputCode_.empty();

    if (composing) {
        // 组字中：编码字符、候选选择键、翻页键、编辑键
        if (vk == VK_BACK || vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_SPACE) return true;
        if (vk == VK_UP || vk == VK_DOWN || vk == VK_PRIOR || vk == VK_NEXT) return true;
        if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return true;
        switch (vk) {
        case VK_OEM_MINUS: case VK_OEM_PLUS: case VK_OEM_1: case VK_OEM_7:
        case VK_OEM_PERIOD: case VK_OEM_COMMA: case VK_OEM_2: case VK_OEM_3:
        case VK_OEM_4: case VK_OEM_5: case VK_OEM_6:
            return true;
        default:
            return false;
        }
    }

    // 空闲 + 英文模式：全部放行
    if (im.isEnglish()) return false;

    // 空闲：只有小写字母起编码；Shift+字母留给应用（大写输入/快捷键）
    if (vk >= 'A' && vk <= 'Z') return !shift;

    // 空闲：数字本身不归输入法（全角模式例外，那时要自己输出全角数字）。
    // 但 Shift+数字是符号（! @ # $ % ^ & * ( )），必须按标点规则判断——
    // 这里以前一律 return fullShape()，拦键判断说「不归我」，processKey 却会把
    // Shift+1 转成「！」，两边打架的结果就是 Shift 打出来全是半角符号。
    if (vk >= '0' && vk <= '9') {
        if (!shift) return fullShape();
        const char c = vkToCodeChar(vk, true);
        return c && needsPuncConversion(c, chinesePunc(), fullShape());
    }

    // 空闲：中文标点 / 全角转换。只有真的会变成别的字符时才接管，
    // 否则（例如 # % * ` 或两个开关都关）按键原样交给应用。
    const char c = vkToCodeChar(vk, shift);
    if (c) return needsPuncConversion(c, chinesePunc(), fullShape());
    return false;
}

SessionOutcome Session::selectCandidate(int index) {
    auto &im = *impl_;
    SessionOutcome out;
    if (index >= 0 && index < static_cast<int>(im.cand_.texts.size())) {
        out.commitText = im.applyTrad(im.cand_.texts[index]);
    }
    im.resetSession();
    out.state = im.state;
    out.englishMode = im.isEnglish();
    out.handled = true;
    return out;
}

SessionOutcome Session::selectCandidateOnPage(int index) {
    auto &im = *impl_;
    const int base = im.pageIndex_ * im.pageSize_;
    return selectCandidate(base + index);
}

SessionOutcome Session::pageCandidates(int delta) {
    auto &im = *impl_;
    SessionOutcome out;
    if (im.state == SessionState::Idle || im.inputCode_.empty() || im.cand_.texts.empty() || delta == 0) {
        return out;
    }

    int page = im.pageIndex_ + delta;
    if (page < 0) page = 0;
    if (page >= im.pageCount()) page = im.pageCount() - 1;
    if (page == im.pageIndex_) return out;

    im.pageIndex_ = page;
    im.candCursor_ = 0;
    im.fillOutcome(out);
    return out;
}

void Session::toggleCharset() {
    auto &im = *impl_;
    if (im.wbzx) im.wbzx->toggleCharset();
    im.refresh();
}

bool Session::addUserWord(const std::string &code, const std::string &text) {
    if (impl_->wbzx) return impl_->wbzx->addUserWord(code, text);
    return false;
}

bool Session::deleteUserWord(const std::string &code, const std::string &text) {
    if (impl_->wbzx) return impl_->wbzx->deleteUserWord(code, text);
    return false;
}

} // namespace freeime11
