// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// FreeIME 11 —— 自绘候选窗口
//
// MinGW 的 msctf.h 不含 ITfCandidateListUIElement，无法使用系统集成候选 UI，
// 因此这里自己画一个分层弹出窗口：不抢焦点、跟随光标，支持鼠标悬停与点选。
#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

class CandidateWindow {
public:
    // 鼠标点选候选（参数为当前页内下标，0 起）
    using SelectHandler = std::function<void(int)>;
    // 点击翻页箭头（-1 上一页 / +1 下一页）
    using PageHandler = std::function<void(int)>;

    static CandidateWindow &instance();

    // 由 DllMain 注入模块句柄（窗口类需要）
    static void setInstanceHandle(HINSTANCE hinst);

    // 注册鼠标回调（服务对象销毁前应清空）
    void setHandlers(SelectHandler onSelect, PageHandler onPage);

    // 显示候选；caret 为屏幕坐标（插入符左下方）。
    // preedit 是当前未完成的编码串：会画在窗口顶部的「编码输入框」里，而不是写进文档。
    // 只要 preedit 非空，即便本页一个候选都没有，窗口也保持可见（不自动收起）。
    void show(const std::vector<std::wstring> &cands, int cursor, bool hasPrev, bool hasNext,
              const std::wstring &aux, POINT caret, const std::wstring &preedit = std::wstring());

    void hide();
    bool visible() const;

    // 短暂提示（切换键盘模式用）：在插入符附近显示一小条文字，ms 毫秒后自动消失。
    // 复用顶部编码输入框的样式，所以看起来是一条轻量提示而不是候选框。
    void showHint(const std::wstring &text, int ms = 800);
    bool hintActive() const { return hintActive_; }

    // 编码输入框当前内容 / 是否还有未完成输入 / 输入框的客户区矩形（测试用）
    const std::wstring &preedit() const { return preedit_; }
    bool hasPreedit() const { return !preedit_.empty(); }
    RECT inputRect() const { return inputRect_; }

    // 命中测试（客户区坐标）
    // 返回 >=0 候选下标；kPrevPage/kNextPage 翻页箭头；kNone 空白
    enum HitArea { kNone = -1, kPrevPage = -2, kNextPage = -3 };
    int hitTest(int x, int y) const;

    // 测试/调试用
    HWND hwnd() const { return hwnd_; }
    int hoverIndex() const { return hover_; }

private:
    CandidateWindow() = default;
    ~CandidateWindow() = default;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void ensureCreated();
    void render();
    int dpiScale() const;   // 返回百分比（96dpi = 100）
    void setHover(int index);
    void trackMouseLeave();

    HWND hwnd_ = nullptr;
    std::vector<std::wstring> cands_;
    std::wstring aux_;
    std::wstring preedit_;      // 顶部输入框显示的未完成编码串
    int cursor_ = 0;
    bool hasPrev_ = false;
    bool hasNext_ = false;
    POINT caret_{0, 0};
    int scale_ = 100;

    // 布局结果（客户区坐标），用于命中测试
    std::vector<RECT> itemRects_;
    RECT inputRect_{};          // 顶部编码输入框
    RECT prevRect_{};
    RECT nextRect_{};
    bool hasArrows_ = false;
    int width_ = 0;
    int height_ = 0;

    int hover_ = kNone;
    bool trackingLeave_ = false;
    // 模式提示：借用编码输入框显示，hintActive_ 用来区分「真编码」和「提示」
    bool hintActive_ = false;

    SelectHandler onSelect_;
    PageHandler onPage_;
};
