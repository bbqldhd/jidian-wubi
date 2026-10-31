// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 沈阳蜂蒿文化传媒有限公司 (Charry)
// FreeIME 11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

// FreeIME 11 —— 输入法状态工具栏
//
// 一条可拖动的小横条，显示并切换 中/英、全/半、标点、简繁、输入方案，
// 另有「设置」与「收起」。与候选窗同源：GDI+ 分层窗口、不抢焦点。
#pragma once

#include <windows.h>

#include <functional>
#include <string>

// 工具栏按钮（顺序即显示顺序）
enum class ToolbarAction {
    English = 0,   // 中 / 英
    FullShape,     // 半 / 全
    ChinesePunc,   // ，。 / ,.
    SimpTrad,      // 简 / 繁
    Engine,        // 五笔 / 五拼 / 拼音 / 英文
    Settings,      // 打开设置程序
    Close,         // 收起工具栏（本次激活内有效）
    Count
};

// 工具栏显示所需的会话状态快照
struct ToolbarState {
    bool english = false;    // 英文态
    bool fullShape = false;  // 全角
    bool chinesePunc = true; // 中文标点
    bool simpTrad = false;   // 简入繁出
    std::string engine = "wbzx";
};

class ToolbarWindow {
public:
    using ActionHandler = std::function<void(ToolbarAction)>;

    static ToolbarWindow &instance();

    // 由 DllMain 注入模块句柄（窗口类需要）
    static void setInstanceHandle(HINSTANCE hinst);

    void setHandler(ActionHandler handler);

    // 更新状态：内容有变化才重绘，避免每次按键都重画
    void setState(const ToolbarState &state);
    const ToolbarState &state() const { return state_; }

    void show();
    void hide();
    bool visible() const;
    void refresh();

    // 拖动 / 位置（屏幕坐标，左上角）
    void moveTo(int x, int y);
    POINT position() const { return pos_; }

    // 命中测试（客户区坐标）：返回 ToolbarAction 的整数下标，-1 表示空白
    int hitTest(int x, int y) const;
    int buttonCount() const { return static_cast<int>(ToolbarAction::Count); }
    RECT buttonRect(int index) const;

    // 测试/调试用
    HWND hwnd() const { return hwnd_; }
    int hoverIndex() const { return hover_; }

private:
    ToolbarWindow() = default;
    ~ToolbarWindow() = default;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void ensureCreated();
    void render();
    void layout();                    // 计算尺寸与按钮矩形
    std::wstring labelOf(int index) const;
    bool isActive(int index) const;   // 该按钮是否处于“开启”状态（高亮）
    int dpiScale() const;
    void setHover(int index);
    void trackMouseLeave();

    void loadPosition();
    void savePosition() const;
    void clampToWorkArea();

    HWND hwnd_ = nullptr;
    ToolbarState state_;
    int scale_ = 100;

    int width_ = 0;
    int height_ = 0;
    RECT itemRects_[static_cast<int>(ToolbarAction::Count)]{};
    int textX_[static_cast<int>(ToolbarAction::Count)]{};

    POINT pos_{0, 0};
    bool positionLoaded_ = false;

    int hover_ = -1;
    bool trackingLeave_ = false;

    // 拖动
    bool pressed_ = false;
    bool dragging_ = false;
    POINT pressScreen_{0, 0};
    POINT pressOrigin_{0, 0};

    ActionHandler handler_;
};
