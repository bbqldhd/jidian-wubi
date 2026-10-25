// FreeIME 11 —— 自绘候选窗口
//
// MinGW 的 msctf.h 不含 ITfCandidateListUIElement，无法使用系统集成候选 UI，
// 因此这里自己画一个分层弹出窗口：无激活、点穿透、跟随光标。
#pragma once

#include <windows.h>

#include <string>
#include <vector>

class CandidateWindow {
public:
    static CandidateWindow &instance();

    // 由 DllMain 注入模块句柄（窗口类需要）
    static void setInstanceHandle(HINSTANCE hinst);

    // 显示候选；caret 为屏幕坐标（插入符左下方）
    void show(const std::vector<std::wstring> &cands, int cursor, bool hasPrev, bool hasNext,
              const std::wstring &aux, POINT caret);

    void hide();
    bool visible() const;

private:
    CandidateWindow() = default;
    ~CandidateWindow() = default;

    void ensureCreated();
    void render();
    int dpiScale() const;   // 返回百分比（96dpi = 100）

    HWND hwnd_ = nullptr;
    std::vector<std::wstring> cands_;
    std::wstring aux_;
    int cursor_ = 0;
    bool hasPrev_ = false;
    bool hasNext_ = false;
    POINT caret_{0, 0};
    int scale_ = 100;
};
