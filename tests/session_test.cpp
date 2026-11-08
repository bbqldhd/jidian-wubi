// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

// FreeIME 11 —— 会话层冒烟测试（跨平台，无 TSF 依赖）
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "../src/session/session.h"

using namespace freeime11;

int main() {
    // 1) 虚拟键码 → 字符映射
    assert(vkToCodeChar('A', false) == 'a');
    assert(vkToCodeChar('A', true) == 'a');
    assert(vkToCodeChar('3', false) == '3');
    assert(vkToCodeChar('3', true) == '#');
    assert(vkToCodeChar(0xBD /*VK_OEM_MINUS*/, false) == '-');
    assert(vkToCodeChar(0xBD, true) == '_');
    assert(vkToCodeChar(0xBA /*VK_OEM_1*/, false) == ';');
    assert(vkToCodeChar(0xBA, true) == ':');
    // 撞号键必须不认：VK_DELETE=0x2E 和 '.'、VK_INSERT=0x2D 和 '-' 数值相同
    assert(vkToCodeChar(0x2E /*VK_DELETE*/, false) == 0);
    assert(vkToCodeChar(0x2D /*VK_INSERT*/, false) == 0);
    assert(vkToCodeChar(0x2C /*VK_SNAPSHOT*/, false) == 0);
    assert(vkToCodeChar(0x5B /*VK_LWIN*/, false) == 0);
    assert(vkToCodeChar(0x60 /*VK_NUMPAD0*/, false) == 0);
    assert(vkToCodeChar(0x0D, false) == 0); // 回车非可编码字符

    // 系统实际发的是 VK_OEM_*，不是 ASCII：这一步归一化错了，标点就全废
    assert(vkToCodeChar(0xBE /*VK_OEM_PERIOD*/, false) == '.');
    assert(vkToCodeChar(0xBE, true) == '>');
    assert(vkToCodeChar(0xBC /*VK_OEM_COMMA*/, false) == ',');
    assert(vkToCodeChar(0xBA /*VK_OEM_1*/, true) == ':');
    assert(vkToCodeChar(0xDE /*VK_OEM_7*/, false) == '\'');
    assert(vkToCodeChar(0xBD /*VK_OEM_MINUS*/, true) == '_');
    assert(vkToCodeChar(0xDB /*VK_OEM_4*/, true) == '{');

    // 2) 会话构造与基础按键流程
    Session s;
    SessionOutcome o;

    // 中文态输入编码 'r'（不依赖词典，引擎空候选也应稳定返回）
    o = s.processKey('R', false, false, false);
    assert(o.handled);

    // 退格：组字中才归输入法
    assert(s.wantsKey(0x08 /*VK_BACK*/, false, false, false));
    o = s.processKey(0x08 /*VK_BACK*/, false, false, false);
    assert(o.handled);

    // 标点：中文标点态下 '.' 上屏全角句号；关掉中文标点则整键放行给应用
    o = s.processKey(0xBE /*VK_OEM_PERIOD*/, false, false, false);
    if (s.chinesePunc()) {
        assert(o.handled);
        assert(o.commitText == "。");
    } else {
        assert(!o.handled);
        assert(o.commitText.empty());
    }

    // 空闲态：回车/空格/退格/Esc 必须放行给应用，不能被输入法吃掉
    assert(!s.wantsKey(0x0D /*VK_RETURN*/, false, false, false));
    assert(!s.wantsKey(0x20 /*0x20*/, false, false, false));
    assert(!s.wantsKey(0x08 /*VK_BACK*/, false, false, false));
    assert(!s.wantsKey(0x1B /*VK_ESCAPE*/, false, false, false));
    o = s.processKey(0x0D /*VK_RETURN*/, false, false, false);
    assert(!o.handled);

    // 空闲态：数字不归输入法（全角模式例外，那时要自己输出全角数字）
    assert(s.wantsKey('1', false, false, false) == s.fullShape());

    // 系统快捷键一律放行（否则 Ctrl+空格 切不了输入法）
    assert(!s.wantsKey(0x20 /*0x20*/, false, true, false));
    assert(!s.wantsKey('R', false, true, false));
    assert(!s.wantsKey(0x20 /*0x20*/, false, false, true));

    // 空闲态：小写字母起编码，Shift+字母放行
    assert(s.wantsKey('R', false, false, false));
    assert(!s.wantsKey('R', true, false, false));

    // 3) 左右 Shift 单击轮换四个键盘模式：五笔拼音 → 五笔 → 拼音 → 英文 → 五笔拼音
    //    （TSF 前端另有「按下不切、松开才切」的判定，这里守会话状态机）
    const std::string originalMode = s.engineName();
    {
        std::vector<std::string> seen;
        for (int i = 0; i < 4; ++i) {
            const int shiftVk = (i % 2 == 0) ? 0xA1 /*VK_RSHIFT*/ : 0xA0 /*VK_LSHIFT*/;
            o = s.processKey(shiftVk, true, false, false);
            assert(o.handled);
            seen.push_back(s.engineName());
        }
        assert(seen.back() == originalMode);                          // 四步一圈回到原点
        assert(std::count(seen.begin(), seen.end(), "en") == 1);      // 英文只出现一档
        assert(std::count(seen.begin(), seen.end(), "wbpy") >= 1);    // 五笔拼音在链上
    }

    // 英文档：整键放行，不吞键；再切一下回到五笔拼音
    {
        Session e;
        for (int i = 0; i < 4 && e.engineName() != "en"; ++i) e.cycleMode();
        assert(e.engineName() == "en");
        assert(e.englishMode());
        assert(!e.wantsKey('R', false, false, false));
        const SessionOutcome r = e.processKey('R', false, false, false);
        assert(!r.handled);

        e.cycleMode();
        assert(e.engineName() == "wbpy");
        assert(!e.englishMode());
    }

    // 4) 选词上屏后会话必须彻底复位：编码/preedit 一律清空，下一键重新起编码
    //    （TSF 侧另有“组合文本必须手动删除”的修复，这里守住状态机这一半）
    {
        Session sel;
        SessionOutcome r;
        for (char ch : std::string("wq")) {
            r = sel.processKey(static_cast<int>(ch - 'a' + 'A'), false, false, false);
        }
        r = sel.processKey('1', false, false, false);
        assert(!r.commitText.empty());          // 数字键选中首候选
        assert(r.preedit.empty());              // 不能残留编码
        assert(r.state == SessionState::Idle);  // 回到空闲态
        assert(r.cands.empty());

        // 紧接着输入必须从全新编码开始
        r = sel.processKey('W', false, false, false);
        assert(r.preedit == "w");
    }

    // 5) 鼠标/数字选中候选的另一条入口：selectCandidate 同样要清空
    {
        Session sel;
        for (char ch : std::string("wq")) {
            sel.processKey(static_cast<int>(ch - 'a' + 'A'), false, false, false);
        }
        SessionOutcome r = sel.selectCandidate(0);
        assert(!r.commitText.empty());
        assert(r.preedit.empty());
        assert(r.state == SessionState::Idle);
    }

    // 6) 候选导航：↑/↓ 移动高亮，PageUp/PageDown 翻页，空格上屏高亮项
    {
        Session nav;
        SessionOutcome r;
        // 找一个能翻页的编码（候选多于一页）
        const char *codes[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", "W", "Q", "E", "R", "T",
                               "Y", "U", "I", "O", "P", "M", "N", "B", "V", "C", "X", "Z"};
        bool found = false;
        for (const char *code : codes) {
            nav.reset();
            r = nav.processKey(code[0], false, false, false);
            if (!r.cands.empty() && r.candHasNext) { found = true; break; }
        }
        assert(found);
        assert(nav.wantsKey(0x28, false, false, false));   // 组字中方向键归输入法
        assert(nav.wantsKey(0x21, false, false, false));

        const std::string secondText = r.cands[1];

        // ↓ 高亮到第二个，preedit 不变
        SessionOutcome down = nav.processKey(0x28, false, false, false);
        assert(down.candCursor == 1);
        assert(down.preedit == r.preedit);

        // ↑ 回到第一个
        SessionOutcome up = nav.processKey(0x26, false, false, false);
        assert(up.candCursor == 0);

        // 第二页
        SessionOutcome page2 = nav.processKey(0x22, false, false, false);
        assert(page2.candHasPrev);
        assert(!page2.cands.empty());
        assert(page2.cands != r.cands);   // 换了一页（首项可能重码，比较整页）
        assert(page2.candCursor == 0);

        // 回第一页
        SessionOutcome page1 = nav.processKey(0x21, false, false, false);
        assert(!page1.candHasPrev);
        assert(page1.cands == r.cands);

        // 空格应上屏当前高亮（第二个），而不是永远首候选
        nav.processKey(0x28, false, false, false);
        SessionOutcome space = nav.processKey(0x20, false, false, false);
        assert(space.commitText == secondText);
        assert(space.preedit.empty());
        assert(space.state == SessionState::Idle);
    }

    // 7) 鼠标点选：页内下标按当前页换算成全局下标
    {
        Session mouse;
        SessionOutcome r;
        const char *codes[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", "W", "Q", "E", "R", "T",
                               "Y", "U", "I", "O", "P", "M", "N", "B", "V", "C", "X", "Z"};
        bool found = false;
        for (const char *code : codes) {
            mouse.reset();
            r = mouse.processKey(code[0], false, false, false);
            if (!r.cands.empty() && r.candHasNext) { found = true; break; }
        }
        assert(found);

        SessionOutcome page2 = mouse.processKey(0x22, false, false, false);
        assert(page2.candHasPrev && !page2.cands.empty());
        const std::string page2Second = page2.cands.size() > 1 ? page2.cands[1] : page2.cands[0];

        SessionOutcome pick = mouse.selectCandidateOnPage(page2.cands.size() > 1 ? 1 : 0);
        assert(pick.commitText == page2Second);
        assert(pick.preedit.empty());
        assert(pick.state == SessionState::Idle);
    }

    // 8) 回车上屏「原文」：候选框输入框里的编码串原样打进文档，不是候选词
    {
        Session t;
        t.processKey('W', false, false, false);
        t.processKey('Q', false, false, false);
        assert(t.rawPreedit() == "wq");
        const SessionOutcome o = t.processKey(0x0D /*VK_RETURN*/, false, false, false);
        assert(o.handled);
        assert(o.commitText == "wq");     // 原文，不是候选词
        assert(o.preedit.empty());
        assert(o.state == SessionState::Idle);
        assert(t.rawPreedit().empty());
    }

    // 9) 组字中按 Shift：原文上屏（与回车同效果）+ 切模式
    {
        Session t;
        const std::string before = t.engineName();
        t.processKey('W', false, false, false);
        t.processKey('Q', false, false, false);
        const SessionOutcome o = t.processKey(0xA0 /*VK_LSHIFT*/, true, false, false);
        assert(o.handled);
        assert(o.commitText == "wq");
        assert(o.state == SessionState::Idle);
        assert(t.engineName() != before);   // 模式确实切了
        assert(t.rawPreedit().empty());
    }

    // 10) 空闲态回车还是交给应用（不吞键）
    {
        Session t;
        const SessionOutcome o = t.processKey(0x0D /*VK_RETURN*/, false, false, false);
        assert(!o.handled);
        assert(o.commitText.empty());
    }

    std::puts("session smoke test: PASS");
    return 0;
}
