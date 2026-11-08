// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// 中文标点回归测试：
//   1) 常用标点（，。；：？！《》【】『』、￥…）都要转成全角/中文形态
//   2) 引号成对：第一次左引号，第二次右引号
//   3) 映射成自己的键（# % * `）不能吞，必须留给应用
//   4) 全角模式：数字、没有中文映射的标点也变全角
//   5) 组字中打标点：先顶字上屏，再输出标点
#include "../src/session/session.h"
#include "../src/utils/settings.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace freeime11;

namespace {

// 真实 Windows 虚拟键码。符号键不是 ASCII：VK_OEM_PERIOD=0xBE 而 '.'=0x2E，
// 用 ASCII 测等于自欺欺人（v11.2.3 的标点失效就是这么漏过去的）。
enum : int {
    VK_SEMI = 0xBA, VK_EQUAL = 0xBB, VK_COMMA = 0xBC, VK_MINUS = 0xBD,
    VK_PERIOD = 0xBE, VK_SLASH = 0xBF, VK_TILDE = 0xC0,
    VK_LBRACKET = 0xDB, VK_BACKSLASH = 0xDC, VK_RBRACKET = 0xDD, VK_QUOTE = 0xDE,
};

// 直接按键，返回上屏文本
std::string tap(Session &s, int vk, bool shift = false) {
    const SessionOutcome o = s.processKey(vk, shift, false, false);
    return o.handled ? o.commitText : std::string();
}

bool handled(Session &s, int vk, bool shift = false) { return s.processKey(vk, shift, false, false).handled; }

} // namespace

int main() {
    const bool oldCn = settings::instance().get_chinesePunc();
    const bool oldFull = settings::instance().get_fullWidthFlg();
    const bool oldPair = settings::instance().get_puncAutoPair();

    settings::instance().set_chinesePunc(true);
    settings::instance().set_fullWidthFlg(false);
    settings::instance().set_puncAutoPair(true);

    Session s;

    // 1) 基础映射（半角键盘 → 中文标点）
    assert(tap(s, VK_COMMA) == "，");
    assert(tap(s, VK_PERIOD) == "。");
    assert(tap(s, VK_SEMI) == "；");
    assert(tap(s, VK_SLASH) == "／");
    assert(tap(s, VK_BACKSLASH) == "、");
    assert(tap(s, VK_SEMI, true) == "：");   // Shift+; 是冒号
    assert(tap(s, VK_SLASH, true) == "？");   // Shift+/ 是问号
    assert(tap(s, VK_COMMA, true) == "《");   // Shift+, 是书名号左
    assert(tap(s, VK_PERIOD, true) == "》");
    assert(tap(s, VK_LBRACKET) == "【");
    assert(tap(s, VK_RBRACKET) == "】");
    assert(tap(s, VK_LBRACKET, true) == "『");
    assert(tap(s, VK_RBRACKET, true) == "』");
    assert(tap(s, VK_MINUS) == "－");
    assert(tap(s, VK_EQUAL) == "＝");
    assert(tap(s, '1', true) == "！");
    assert(tap(s, '4', true) == "￥");
    assert(tap(s, '9', true) == "（");
    assert(tap(s, '0', true) == "）");

    // 2) 引号成对：左 → 右 → 左；单双引号各自独立计数
    assert(tap(s, VK_QUOTE, true) == "\xE2\x80\x9C");   // “ （Shift+' 是双引号）
    assert(tap(s, VK_QUOTE, true) == "\xE2\x80\x9D");   // ”
    assert(tap(s, VK_QUOTE, true) == "\xE2\x80\x9C");   // “
    assert(tap(s, VK_QUOTE) == "\xE2\x80\x98");         // ‘
    assert(tap(s, VK_QUOTE) == "\xE2\x80\x99");         // ’
    // 关掉智能配对：固定左引号
    settings::instance().set_puncAutoPair(false);
    assert(tap(s, VK_QUOTE, true) == "\xE2\x80\x9C");
    assert(tap(s, VK_QUOTE, true) == "\xE2\x80\x9C");
    settings::instance().set_puncAutoPair(true);

    // 3) 映射成自己的键不接管：原样交给应用（这些都在数字键的上档，得按真实虚拟键发）
    assert(!handled(s, '3', true));   // #
    assert(!handled(s, '5', true));   // %
    assert(!handled(s, '8', true));   // *
    assert(!handled(s, VK_TILDE));
    // 空闲态 Shift+字母仍然属于应用（大写输入/快捷键）
    assert(!s.wantsKey('A', true, false, false));

    // 4) 全角模式：数字与没有中文映射的标点转全角
    settings::instance().set_chinesePunc(false);
    settings::instance().set_fullWidthFlg(true);
    assert(tap(s, '1') == "１");
    assert(tap(s, '3', true) == "＃");
    assert(tap(s, VK_MINUS, true) == "＿");
    assert(tap(s, VK_PERIOD) == "．");
    settings::instance().set_fullWidthFlg(false);
    // 两个开关都关：标点整键放行
    assert(!handled(s, VK_PERIOD));
    assert(!handled(s, VK_COMMA, true));

    // 5) 组字中打标点：先顶字上屏，再接标点（当前方案没有候选时就只剩标点）
    settings::instance().set_chinesePunc(true);
    {
        Session typing;
        SessionOutcome last;
        for (char ch : std::string("wqvb")) {
            last = typing.processKey(static_cast<int>(ch - 'a' + 'A'), false, false, false);
        }
        const std::string head = last.cands.empty() ? std::string() : last.cands[0];

        const SessionOutcome o = typing.processKey(VK_COMMA, false, false, false);
        assert(o.handled);
        assert(o.commitText == head + "，");
        assert(o.preedit.empty());
        assert(o.state == SessionState::Idle);
    }

    settings::instance().set_chinesePunc(oldCn);
    settings::instance().set_fullWidthFlg(oldFull);
    settings::instance().set_puncAutoPair(oldPair);

    std::puts("punctuation test: PASS");
    return 0;
}
