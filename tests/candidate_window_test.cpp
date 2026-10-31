// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

// 候选窗自测（无需 TSF）：显示/命中测试/悬停/点选/翻页箭头
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

    w.show({L"你好", L"您好", L"尼好"}, 0, true, true, L"", POINT{200, 200});
    assert(w.visible());

    HWND h = w.hwnd();
    assert(h && IsWindow(h));

    // 点击候选窗不能激活窗口（否则文档失焦、组合被终止，候选框就“丢了”）
    assert(SendMessageW(h, WM_MOUSEACTIVATE, 0, 0) == MA_NOACTIVATE);

    RECT rc{};
    assert(GetClientRect(h, &rc));
    assert(rc.right > 40 && rc.bottom > 20);

    // 横扫中线，找出每个候选/箭头所在的 x
    const int midY = (rc.bottom - rc.top) / 2;
    int candX[3] = {-1, -1, -1};
    int prevX = -1;
    int nextX = -1;
    for (int x = 0; x < rc.right; ++x) {
        const int id = w.hitTest(x, midY);
        if (id >= 0 && id < 3 && candX[id] < 0) candX[id] = x;
        if (id == CandidateWindow::kPrevPage && prevX < 0) prevX = x;
        if (id == CandidateWindow::kNextPage && nextX < 0) nextX = x;
    }
    assert(candX[0] >= 0 && candX[1] >= 0 && candX[2] >= 0);
    assert(prevX >= 0 && nextX >= 0);

    // 鼠标悬停：悬停态要能反映出来（渲染由窗口自己完成）
    SendMessageW(h, WM_MOUSEMOVE, 0, MAKELPARAM(candX[2], midY));
    assert(w.hoverIndex() == 2);
    SendMessageW(h, WM_MOUSELEAVE, 0, 0);
    assert(w.hoverIndex() == CandidateWindow::kNone);

    // 点选第二个候选
    SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(candX[1], midY));
    assert(picked == 1);

    // 翻页箭头
    SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(nextX, midY));
    assert(paged == 1);
    SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(prevX, midY));
    assert(paged == -1);

    // 空白处点击不应误选
    picked = -1;
    SendMessageW(h, WM_LBUTTONUP, 0, MAKELPARAM(rc.right - 1, 1));
    assert(picked == -1);

    w.hide();
    assert(!w.visible());

    std::puts("candidate window test: PASS");
    return 0;
}
