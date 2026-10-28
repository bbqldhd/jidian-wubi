// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 沈阳蜂蒿文化传媒有限公司 (Charry)
// FreeIME 11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

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

// 中文标点映射（对齐 10.8.9 symbol.txt 的默认值）
const std::unordered_map<char, const char *> kCnPunc = {
    {'`', "`"},   {'~', "~"},   {'!', "！"}, {'@', "·"},  {'#', "#"},   {'$', "￥"},
    {'%', "%"},   {'^', "…"},   {'&', "—"},  {'*', "*"},  {'(', "（"},  {')', "）"},
    {'-', "－"},  {'_', "—"},   {'=', "＝"}, {'+', "+"},  {'[', "["},   {'{', "{"},
    {']', "]"},   {'}', "}"},   {'\\', "、"},{'|', "｜"}, {';', "；"},  {':', "："},
    {',', "，"},  {'.', "。"},  {'/', "、"}, {'?', "？"},
};

// 智能标点配对（成对，左符进入、右符配对输出）
const char kAutoPairLeft[]  = "([{\"'";
const char kAutoPairRight[] = ")]}\"'";

bool isLetterCode(char c) { return (c >= 'a' && c <= 'z'); }
bool isDigit(char c) { return (c >= '0' && c <= '9'); }

} // namespace

struct Session::Impl {
    // 引擎（构造顺序：wbzx/py 先，wbpy 依赖前两者）
    std::unique_ptr<WbzxEngine> wbzx;
    std::unique_ptr<PyEngine> py;
    std::unique_ptr<Wbpy> wbpy;
    std::unique_ptr<En> en;

    int engineIdx = 0; // 0 wbzx, 1 wbpy, 2 py, 3 en
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
    bool englishMode_ = false; // 中英文态（false=中文输入）
    bool commitExactPending_ = false;

    // 智能标点配对状态
    bool smartPairOpen_ = false;
    char smartPairOpenChar_ = 0;

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
        out.englishMode = englishMode_;
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
}

Session::~Session() = default;

bool Session::loadSettings(const std::string &dataDir) {
    (void)dataDir;
    settings::instance().reload();
    impl_->pageSize_ = std::max(1, settings::instance().get_candiWordCount());
    return true;
}

void Session::reset() { impl_->resetSession(); }

bool Session::chinesePunc() const { return settings::instance().get_chinesePunc(); }
bool Session::fullShape() const { return settings::instance().get_fullWidthFlg(); }
bool Session::simpTrad() const { return settings::instance().get_simpTradFlg(); }
std::string Session::engineName() const {
    switch (impl_->engineIdx) {
    case Impl::kWbpy: return "wbpy";
    case Impl::kPy:   return "py";
    case Impl::kEn:   return "en";
    default:          return "wbzx";
    }
}

void Session::toggleEnglish() { impl_->englishMode_ = !impl_->englishMode_; impl_->resetSession(); }
void Session::toggleFullShape() { settings::instance().set_fullWidthFlg(!fullShape()); }
void Session::toggleChinesePunc() { settings::instance().set_chinesePunc(!chinesePunc()); }
void Session::toggleSimpTrad() { settings::instance().set_simpTradFlg(!simpTrad()); }
void Session::cycleEngine() {
    impl_->engineIdx = (impl_->engineIdx + 1) % 4;
    impl_->resetSession();
}

char vkToCodeChar(int vk, bool shift) {
    if (vk >= 'A' && vk <= 'Z') return static_cast<char>(vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9') {
        if (!shift) return static_cast<char>(vk);
        static const char kNumShift[10] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
        return kNumShift[vk - '0'];
    }
    // 符号键（shift 上档）
    static char kShifted[256] = {0};
    static bool inited = false;
    if (!inited) {
        inited = true;
        kShifted['`'] = '~'; kShifted['1'] = '!'; kShifted['2'] = '@'; kShifted['3'] = '#';
        kShifted['4'] = '$'; kShifted['5'] = '%'; kShifted['6'] = '^'; kShifted['7'] = '&';
        kShifted['8'] = '*'; kShifted['9'] = '('; kShifted['0'] = ')'; kShifted['-'] = '_';
        kShifted['='] = '+'; kShifted['['] = '{'; kShifted[']'] = '}'; kShifted['\\'] = '|';
        kShifted[';'] = ':'; kShifted['\''] = '"'; kShifted[','] = '<'; kShifted['.'] = '>';
        kShifted['/'] = '?';
    }
    if (vk == '`' || vk == '-' || vk == '=' || vk == '[' || vk == ']' || vk == '\\' ||
        vk == ';' || vk == '\'' || vk == ',' || vk == '.' || vk == '/') {
        return shift ? kShifted[vk] : static_cast<char>(vk);
    }
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
    out.englishMode = im.englishMode_;

    // Ctrl 快捷键（全局，无论中英文态）
    if (ctrl && !alt && !shift) {
        switch (vk) {
        case 'M': toggleCharset(); out.handled = true; break;
        case ',': /* 设置面板（前端处理） */ out.handled = true; break;
        case ' ': // 中英文切换（备用，默认右 Shift）
            toggleEnglish(); out.handled = true; break;
        default: break;
        }
        if (out.handled) { out.state = im.state; return out; }
    }

    // 中英文切换：右 Shift（默认 CN_EN=RShift）
    if (shift && vk == VK_RSHIFT && !ctrl && !alt) {
        toggleEnglish();
        out.handled = true; out.state = im.state; out.englishMode = im.englishMode_;
        return out;
    }

    // 英文态：整键交给应用处理，输入法不吞键（自己插字符会破坏快捷键与自动补全）
    if (im.englishMode_ && im.state == SessionState::Idle) {
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
        // 回车：有候选时上屏当前高亮候选（↑/↓ 选中的那个），否则清空编码
        if (!im.cand_.texts.empty()) {
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
        // 翻页 - / =
        if (c == '-') { if (im.pageIndex_ > 0) { --im.pageIndex_; im.candCursor_ = 0; out.handled = true; } }
        else if (c == '=') { if (im.pageIndex_ + 1 < im.pageCount()) { ++im.pageIndex_; im.candCursor_ = 0; out.handled = true; } }
        else if (c && isLetterCode(c)) {
            // 编码输入
            im.inputCode_.push_back(c);
            im.state = SessionState::Composing;
            out.handled = true;

            // 临时拼音引导 z
            if (tmpPinyin && im.isWubiFamily()) { /* 保留：z 引导临时拼音 */ }
            im.refresh();

            // 自动上屏：2 码唯一 / 满 4 码唯一
            if (im.cand_.texts.size() == 1) {
                const std::string code = im.inputCode_;
                const size_t minLen = (im.engineIdx == Impl::kWbzx) ? 4 : 2;
                if (code.size() >= minLen || im.cur()->isExactDictionaryKey(code)) {
                    out.commitText = im.applyTrad(im.cand_.texts[0]);
                    im.resetSession();
                    out.handled = true; out.state = im.state;
                    return out;
                }
            }
        } else if (c && isDigit(c)) {
            // 空闲态的数字不做标点转换，交给应用
            return out;
        } else if (c) {
            // 标点：有候选→顶字上屏；无候选→标点转换
            std::string candText;
            if (!im.cand_.texts.empty()) {
                int b = 0, e = 0; im.pageRange(b, e);
                candText = im.applyTrad(im.cand_.texts[b]);
            }
            im.resetSession();

            std::string punc = chinesePunc() && kCnPunc.count(c) ? kCnPunc.at(c) : std::string(1, c);
            // 数字后自动半角（HalfCharFollowNum='.'）：数字后 '.' 输出半角
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

    // 空闲 + 英文态：全部放行
    if (im.englishMode_) return false;

    // 空闲：只有小写字母起编码；Shift+字母留给应用（大写输入/快捷键）
    if (vk >= 'A' && vk <= 'Z') return !shift;

    // 空闲：数字不归输入法
    if (vk >= '0' && vk <= '9') return false;

    // 空闲：中文标点转换
    switch (vk) {
    case VK_OEM_COMMA: case VK_OEM_PERIOD: case VK_OEM_1: case VK_OEM_7:
    case VK_OEM_2: case VK_OEM_3: case VK_OEM_4: case VK_OEM_5: case VK_OEM_6:
    case VK_OEM_MINUS: case VK_OEM_PLUS:
        return true;
    default:
        return false;
    }
}

SessionOutcome Session::selectCandidate(int index) {
    auto &im = *impl_;
    SessionOutcome out;
    if (index >= 0 && index < static_cast<int>(im.cand_.texts.size())) {
        out.commitText = im.applyTrad(im.cand_.texts[index]);
    }
    im.resetSession();
    out.state = im.state;
    out.englishMode = im.englishMode_;
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
