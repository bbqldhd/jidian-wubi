// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

// 候选窗自测（无需 TSF）：显示 / 顶部编码输入框 / 命中测试 / 悬停 / 点选 / 翻页 / 显隐
#include "../src/win/tsf/CandidateWindow.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

int main() {
    CandidateWindow &w = CandidateWindow::instance();

    int picked = -1;
    int paged = 0;
    w.setHandlers([&](int i) { picked = i; }, [&](int d) { paged = d; });

    // 未完成编码 wqvb 显示在顶部输入框里
    w.show({L"你好", L"您好", L"尼好"}, 0, true, true, L"", POINT{200, 200}, L"wqvb");
    assert(w.visible());
    assert(w.preedit() == L"wqvb");
    assert(w.hasPreedit());

    HWND h = w.hwnd();
    assert(h && IsWindow(h));

    // 点击候选窗不能激活窗口（否则文档失焦、组合被终止，候选框就“丢了”）
    assert(SendMessageW(h, WM_MOUSEACTIVATE, 0, 0) == MA_NOACTIVATE);

    RECT rc{};
    assert(GetClientRect(h, &rc));
    assert(rc.right > 40 && rc.bottom > 20);

    // 顶部输入框不可点（它不是候选，也不该吞掉空白点击）
    const RECT ir = w.inputRect();
    assert(ir.right > ir.left && ir.bottom > ir.top);
    assert(w.hitTest((ir.left + ir.right) / 2, (ir.top + ir.bottom) / 2) == CandidateWindow::kNone);

    // 扫描整个客户区找出每个候选 / 箭头所在坐标（候选行在输入框下方）
    int candX[3] = {-1, -1, -1};
    int prevX = -1;
    int nextX = -1;
    int candY = -1;
    int prevY = -1;
    int nextY = -1;
    for (int y = 0; y < rc.bottom; ++y) {
        for (int x = 0; x < rc.right; ++x) {
            const int id = w.hitTest(x, y);
            if (id >= 0 && id < 3 && candX[id] < 0) { candX[id] = x; candY = y; }
            if (id == CandidateWindow::kPrevPage && prevX < 0) { prevX = x; prevY = y; }
            if (id == CandidateWindow::kNextPage && nextX < 0) { nextX = x; nextY = y; }
        }
    }
    assert(candX[0] >= 0 && candX[1] >= 0 && candX[2] >= 0);
    assert(prevX >= 0 && nextX >= 0);
    assert(candY > ir.bottom);   // 候选确实排在输入框下面

    // 鼠标悬停：悬停态要能反映出来（渲染由窗口自己完成）
    SendMessageW(h, WM_MOUSEMOVE, 0, MAKELPARAM(candX[2], candY));
    assert(w.hoverIndex() == 2);
    SendMessageW(h, WM_MOUSELEAVE, 0, 0);
    assert(w.hoverIndex() == CandidateWindow::kNone);

    // 点选第二个候选
    SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(candX[1], candY));
    assert(picked == 1);

    // 翻页箭头
    SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(nextX, nextY));
    assert(paged == 1);
    SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(prevX, prevY));
    assert(paged == -1);

    // 空白处点击不应误选
    picked = -1;
    SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(rc.right - 1, 1));
    assert(picked == -1);

    // 关键行为：未完成输入 / 输入到一半 / 候选为空时，输入框保持可见，不自动收起
    w.show({}, 0, false, false, L"", POINT{260, 260}, L"as");
    assert(w.visible());
    assert(w.hasPreedit());
    assert(w.preedit() == L"as");
    assert(w.hitTest((w.inputRect().left + w.inputRect().right) / 2,
                     (w.inputRect().top + w.inputRect().bottom) / 2) == CandidateWindow::kNone);

    w.show({}, 0, false, false, L"", POINT{260, 260}, L"woxih");
    assert(w.visible());
    assert(w.preedit() == L"woxih");

    // 候选重新出现时，输入框内容同步刷新
    w.show({L"你好"}, 0, false, false, L"", POINT{260, 260}, L"ni");
    assert(w.visible());
    assert(w.preedit() == L"ni");

    // 只有编码清空 + 没有候选时才收起
    w.show({}, 0, false, false, L"", POINT{260, 260}, L"");
    assert(!w.visible());
    assert(!w.hasPreedit());

    // 显式 hide 也会清掉内容，避免下次显示带着旧编码
    w.show({}, 0, false, false, L"", POINT{260, 260}, L"as");
    assert(w.visible());
    w.hide();
    assert(!w.visible());
    assert(!w.hasPreedit());

    // 模式提示条：切 Shift 时弹一小条「五笔」，不显示候选、到点自动收起
    {
        w.showHint(L"五笔", 300);
        assert(w.visible());
        assert(w.hintActive());
        assert(w.preedit() == L"五笔");     // 借用编码输入框渲染
        RECT hr{};
        assert(GetClientRect(w.hwnd(), &hr));
        assert(hr.right > 20 && hr.bottom > 10);

        // 真编码进来要顶掉提示
        w.show({L"你好"}, 0, false, false, L"", POINT{200, 200}, L"wq");
        assert(!w.hintActive());
        assert(w.preedit() == L"wq");

        // 定时器到点后自动收起（泵一会儿消息，让 WM_TIMER 送达）
        w.showHint(L"拼音", 120);
        assert(w.hintActive());
        const DWORD until = GetTickCount() + 1500;
        while (GetTickCount() < until && w.hintActive()) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            Sleep(15);
        }
        assert(!w.hintActive());
        assert(!w.visible());
    }

    std::puts("candidate window test: PASS");
    return 0;
}
