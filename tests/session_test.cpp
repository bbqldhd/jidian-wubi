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

    // 退格清空
    o = s.processKey(0x08 /*VK_BACK*/, false, false, false);
    assert(o.handled);

    // 标点（中文标点态）：'。' 应为全角
    o = s.processKey('.', false, false, false);
    // 无候选时上屏中文句号
    assert(!o.commitText.empty());

    // 回车复位
    o = s.processKey(0x0D /*VK_RETURN*/, false, false, false);
    assert(o.handled);

    // 3) 中英文切换（右 Shift）
    o = s.processKey(0xA1 /*VK_RSHIFT*/, true, false, false);
    assert(o.handled);

    std::puts("session smoke test: PASS");
    return 0;
}
