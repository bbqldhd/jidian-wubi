// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// 工具栏自测（无需 TSF）：显示/不抢焦点/命中/点击回调/固定列宽/位置记忆
#include "../src/win/tsf/ToolbarWindow.h"

#include <cassert>
#include <cstdio>

int main() {
    ToolbarWindow &t = ToolbarWindow::instance();

    int last = -1;
    t.setHandler([&](ToolbarAction a) { last = static_cast<int>(a); });

    t.setState(ToolbarState{false, true, false, "wbzx"});
    t.show();
    assert(t.visible());

    HWND h = t.hwnd();
    assert(h && IsWindow(h));

    // 点击工具栏不能抢焦点，否则文档失焦、组合被终止
    assert(SendMessageW(h, WM_MOUSEACTIVATE, 0, 0) == MA_NOACTIVATE);

    RECT rc{};
    assert(GetClientRect(h, &rc));
    assert(rc.right > 150 && rc.bottom > 20);

    // 每个按钮都能被命中，点击会派发对应动作
    for (int i = 0; i < t.buttonCount(); ++i) {
        const RECT b = t.buttonRect(i);
        assert(b.right > b.left && b.bottom > b.top);
        const int cx = (b.left + b.right) / 2;
        const int cy = (b.top + b.bottom) / 2;
        assert(t.hitTest(cx, cy) == i);

        last = -1;
        SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(cx, cy));
        assert(last == i);
    }

    // 边缘空白（内边距）不应误触
    last = -1;
    SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(1, 1));
    assert(last == -1);

    // 切换状态：内容变化但整体宽度不能跳（列宽按最宽文案固定）
    RECT before{};
    assert(GetWindowRect(h, &before));
    const int w0 = before.right - before.left;

    t.setState(ToolbarState{true, false, true, "py"});
    RECT after{};
    assert(GetWindowRect(h, &after));
    assert((after.right - after.left) == w0);
    assert(t.visible());

    // 位置：moveTo 后窗口跟随，且再次 show 不会跳回默认位置
    t.moveTo(300, 200);
    RECT moved{};
    assert(GetWindowRect(h, &moved));
    assert(moved.left == 300 && moved.top == 200);

    t.hide();
    assert(!t.visible());
    t.show();
    RECT again{};
    assert(GetWindowRect(h, &again));
    assert(again.left == 300 && again.top == 200);
    t.hide();

    std::puts("toolbar window test: PASS");
    return 0;
}
