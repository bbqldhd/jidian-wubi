// FreeIME 11 —— 会话层冒烟测试（跨平台，无 TSF 依赖）
#include <cassert>
#include <cstdio>

#include "../src/session/session.h"

using namespace freeime11;

int main() {
    // 1) 虚拟键码 → 字符映射
    assert(vkToCodeChar('A', false) == 'a');
    assert(vkToCodeChar('A', true) == 'a');
    assert(vkToCodeChar('3', false) == '3');
    assert(vkToCodeChar('3', true) == '#');
    assert(vkToCodeChar('-', false) == '-');
    assert(vkToCodeChar('-', true) == '_');
    assert(vkToCodeChar(';', false) == ';');
    assert(vkToCodeChar(';', true) == ':');
    assert(vkToCodeChar(0x0D, false) == 0); // 回车非可编码字符

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

    // 标点（中文标点态）：'。' 应为全角
    o = s.processKey('.', false, false, false);
    // 无候选时上屏中文句号
    assert(!o.commitText.empty());

    // 空闲态：回车/空格/退格/Esc 必须放行给应用，不能被输入法吃掉
    assert(!s.wantsKey(0x0D /*VK_RETURN*/, false, false, false));
    assert(!s.wantsKey(0x20 /*0x20*/, false, false, false));
    assert(!s.wantsKey(0x08 /*VK_BACK*/, false, false, false));
    assert(!s.wantsKey(0x1B /*VK_ESCAPE*/, false, false, false));
    o = s.processKey(0x0D /*VK_RETURN*/, false, false, false);
    assert(!o.handled);

    // 空闲态：数字不归输入法
    assert(!s.wantsKey('1', false, false, false));

    // 系统快捷键一律放行（否则 Ctrl+空格 切不了输入法）
    assert(!s.wantsKey(0x20 /*0x20*/, false, true, false));
    assert(!s.wantsKey('R', false, true, false));
    assert(!s.wantsKey(0x20 /*0x20*/, false, false, true));

    // 空闲态：小写字母起编码，Shift+字母放行
    assert(s.wantsKey('R', false, false, false));
    assert(!s.wantsKey('R', true, false, false));

    // 3) 中英文切换（右 Shift）
    o = s.processKey(0xA1 /*VK_RSHIFT*/, true, false, false);
    assert(o.handled);

    // 英文态：整键放行，不吞键
    assert(!s.wantsKey('R', false, false, false));

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

    std::puts("session smoke test: PASS");
    return 0;
}
