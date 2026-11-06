// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

#include "ToolbarWindow.h"

#include <gdiplus.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <vector>

using namespace Gdiplus;

namespace {

HINSTANCE g_hinst = nullptr;
const wchar_t *kClassName = L"FreeIME11ToolbarWnd";
const wchar_t *kRegKey = L"Software\\FreeIME11\\Toolbar";

ULONG_PTR gdiplusToken = 0;
std::once_flag gdiplusOnce;

void ensureGdiplus() {
    std::call_once(gdiplusOnce, [] {
        GdiplusStartupInput input;
        GdiplusStartup(&gdiplusToken, &input, nullptr);
    });
}

int scaleOf(int base, int pct) { return MulDiv(base, pct, 100); }

void moduleAnchor() {}

bool ptInRect(const RECT &r, int x, int y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }

// 每个按钮可能出现的最宽文案（用于固定宽度，避免切换时整条工具栏跳动）
const wchar_t *kLabelSets[static_cast<int>(ToolbarAction::Count)][4] = {
    {L"五笔", L"五拼", L"拼音", L"英文"},
    {L"半", L"全", nullptr, nullptr},
    {L"，。", L",.", nullptr, nullptr},
    {L"简", L"繁", nullptr, nullptr},
    {L"设置", nullptr, nullptr, nullptr},
    {L"×", nullptr, nullptr, nullptr},
};

}  // namespace

ToolbarWindow &ToolbarWindow::instance() {
    static ToolbarWindow w;
    return w;
}

void ToolbarWindow::setInstanceHandle(HINSTANCE hinst) { g_hinst = hinst; }

void ToolbarWindow::setHandler(ActionHandler handler) { handler_ = std::move(handler); }

// ---------------------------------------------------------------- 状态与外观

std::wstring ToolbarWindow::labelOf(int index) const {
    switch (static_cast<ToolbarAction>(index)) {
    case ToolbarAction::Mode:
        // 一个按钮显示当前键盘模式，点一下按「五笔拼音 → 五笔 → 拼音 → 英文」循环
        if (state_.engine == "wbpy") return L"五拼";
        if (state_.engine == "py") return L"拼音";
        if (state_.engine == "en") return L"英文";
        return L"五笔";
    case ToolbarAction::FullShape: return state_.fullShape ? L"全" : L"半";
    case ToolbarAction::ChinesePunc: return state_.chinesePunc ? L"，。" : L",.";
    case ToolbarAction::SimpTrad: return state_.simpTrad ? L"繁" : L"简";
    case ToolbarAction::Settings: return L"设置";
    case ToolbarAction::Close: return L"×";
    default: return L"";
    }
}

bool ToolbarWindow::isActive(int index) const {
    switch (static_cast<ToolbarAction>(index)) {
    case ToolbarAction::Mode: return state_.engine == "en";
    case ToolbarAction::FullShape: return state_.fullShape;
    case ToolbarAction::ChinesePunc: return state_.chinesePunc;
    case ToolbarAction::SimpTrad: return state_.simpTrad;
    default: return false;
    }
}

void ToolbarWindow::setState(const ToolbarState &state) {
    if (state.fullShape == state_.fullShape && state.chinesePunc == state_.chinesePunc &&
        state.simpTrad == state_.simpTrad && state.engine == state_.engine) {
        return;
    }
    state_ = state;
    if (visible()) render();
}

// ---------------------------------------------------------------- 窗口

int ToolbarWindow::dpiScale() const {
    HDC dc = GetDC(nullptr);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);
    return dpi > 0 ? MulDiv(100, dpi, 96) : 100;
}

void ToolbarWindow::ensureCreated() {
    if (hwnd_ && IsWindow(hwnd_)) return;
    hwnd_ = nullptr;

    HINSTANCE hinst = g_hinst;
    if (!hinst) {
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&moduleAnchor), &hinst);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &ToolbarWindow::wndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);  // 已注册返回 0，忽略

    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED, kClassName, L"",
                            WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, hinst, this);
}

bool ToolbarWindow::visible() const { return hwnd_ && IsWindowVisible(hwnd_) != FALSE; }

void ToolbarWindow::show() {
    ensureCreated();
    if (!hwnd_) return;

    if (!positionLoaded_) {
        loadPosition();
        positionLoaded_ = true;
    }
    scale_ = dpiScale();
    clampToWorkArea();

    render();
    if (!IsWindowVisible(hwnd_)) ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

void ToolbarWindow::hide() {
    hover_ = -1;
    trackingLeave_ = false;
    pressed_ = false;
    dragging_ = false;
    if (hwnd_ && IsWindowVisible(hwnd_)) ShowWindow(hwnd_, SW_HIDE);
}

void ToolbarWindow::refresh() {
    if (visible()) render();
}

void ToolbarWindow::moveTo(int x, int y) {
    pos_.x = x;
    pos_.y = y;
    // 显式定位优先于记住的位置：避免首次 show() 时被 loadPosition 覆盖
    positionLoaded_ = true;
    if (hwnd_) {
        SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
}

// ---------------------------------------------------------------- 位置持久化

void ToolbarWindow::loadPosition() {
    bool loaded = false;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD x = 0, y = 0, sz = sizeof(DWORD), type = 0;
        const bool okX =
            RegQueryValueExW(key, L"PosX", nullptr, &type, reinterpret_cast<LPBYTE>(&x), &sz) == ERROR_SUCCESS;
        sz = sizeof(DWORD);
        const bool okY =
            RegQueryValueExW(key, L"PosY", nullptr, &type, reinterpret_cast<LPBYTE>(&y), &sz) == ERROR_SUCCESS;
        RegCloseKey(key);
        if (okX && okY) {
            pos_.x = static_cast<int>(x);
            pos_.y = static_cast<int>(y);
            loaded = true;
        }
    }
    if (loaded) return;

    // 默认：主屏工作区右下角（任务栏上方）
    RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi)) work = mi.rcWork;
    const int s = dpiScale();
    pos_.x = work.right - scaleOf(240, s) - scaleOf(12, s);
    pos_.y = work.bottom - scaleOf(40, s) - scaleOf(12, s);
}

void ToolbarWindow::savePosition() const {
    HKEY key = nullptr;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, &disp) !=
        ERROR_SUCCESS) {
        return;
    }
    const DWORD x = static_cast<DWORD>(pos_.x);
    const DWORD y = static_cast<DWORD>(pos_.y);
    RegSetValueExW(key, L"PosX", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&x), sizeof(x));
    RegSetValueExW(key, L"PosY", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&y), sizeof(y));
    RegCloseKey(key);
}

void ToolbarWindow::clampToWorkArea() {
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    if (GetMonitorInfoW(MonitorFromPoint(POINT{pos_.x, pos_.y}, MONITOR_DEFAULTTONEAREST), &mi)) work = mi.rcWork;

    const int w = std::max(width_, scaleOf(120, scale_));
    const int h = std::max(height_, scaleOf(30, scale_));
    const int margin = scaleOf(2, scale_);
    if (pos_.x + w > work.right - margin) pos_.x = work.right - margin - w;
    if (pos_.x < work.left + margin) pos_.x = work.left + margin;
    if (pos_.y + h > work.bottom - margin) pos_.y = work.bottom - margin - h;
    if (pos_.y < work.top + margin) pos_.y = work.top + margin;
}

// ---------------------------------------------------------------- 布局

void ToolbarWindow::layout() {
    const int s = scale_ > 0 ? scale_ : 100;

    Gdiplus::FontFamily yahei(L"Microsoft YaHei UI");
    Gdiplus::FontFamily segoe(L"Segoe UI");
    Gdiplus::FontFamily *family = yahei.IsAvailable() ? &yahei : &segoe;
    Gdiplus::Font font(family, static_cast<REAL>(scaleOf(13, s)), FontStyleRegular, UnitPixel);

    Bitmap measure(1, 1, PixelFormat32bppARGB);
    Graphics gm(&measure);
    gm.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    const int pad = scaleOf(4, s);
    const int gap = scaleOf(3, s);
    const int padX = scaleOf(7, s);
    const int btnH = scaleOf(26, s);
    const int minW = scaleOf(26, s);

    int x = pad;
    int total = pad * 2;
    for (int i = 0; i < static_cast<int>(ToolbarAction::Count); ++i) {
        int maxTextW = 0;
        for (int k = 0; k < 4 && kLabelSets[i][k]; ++k) {
            RectF r;
            gm.MeasureString(kLabelSets[i][k], -1, &font, PointF(0, 0), &r);
            maxTextW = std::max(maxTextW, static_cast<int>(r.Width + 0.5f));
        }
        const int w = std::max(minW, maxTextW + padX * 2);
        itemRects_[i] = RECT{x, pad, x + w, pad + btnH};
        x += w;
        if (i + 1 < static_cast<int>(ToolbarAction::Count)) x += gap;
        total += w + (i + 1 < static_cast<int>(ToolbarAction::Count) ? gap : 0);
    }

    width_ = total;
    height_ = pad * 2 + btnH;
}

// ---------------------------------------------------------------- 绘制

void ToolbarWindow::render() {
    ensureGdiplus();
    if (!hwnd_) return;

    const int s = scale_ > 0 ? scale_ : 100;
    layout();

    const int w = width_;
    const int h = height_;
    const int radius = scaleOf(6, s);

    Gdiplus::FontFamily yahei(L"Microsoft YaHei UI");
    Gdiplus::FontFamily segoe(L"Segoe UI");
    Gdiplus::FontFamily *family = yahei.IsAvailable() ? &yahei : &segoe;
    Gdiplus::Font font(family, static_cast<REAL>(scaleOf(13, s)), FontStyleRegular, UnitPixel);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    void *bits = nullptr;
    HBITMAP dib = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib) {
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return;
    }
    HGDIOBJ old = SelectObject(memDc, dib);
    ZeroMemory(bits, static_cast<size_t>(w) * h * 4);

    Gdiplus::Bitmap canvas(w, h, PixelFormat32bppARGB);
    {
        Graphics g(&canvas);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.SetCompositingMode(CompositingModeSourceOver);

        auto roundRect = [&](GraphicsPath &p, const RectF &rr, int radiusPx) {
            const int d = radiusPx * 2;
            p.AddArc(rr.X, rr.Y, (REAL)d, (REAL)d, 180, 90);
            p.AddArc(rr.GetRight() - d, rr.Y, (REAL)d, (REAL)d, 270, 90);
            p.AddArc(rr.GetRight() - d, rr.GetBottom() - d, (REAL)d, (REAL)d, 0, 90);
            p.AddArc(rr.X, rr.GetBottom() - d, (REAL)d, (REAL)d, 90, 90);
            p.CloseFigure();
        };

        // 阴影
        {
            GraphicsPath sp;
            roundRect(sp, RectF(static_cast<REAL>(scaleOf(1, s)), static_cast<REAL>(scaleOf(2, s)),
                                static_cast<REAL>(w - scaleOf(2, s)), static_cast<REAL>(h - scaleOf(4, s))),
                      radius);
            SolidBrush sb(Color(28, 0, 0, 0));
            g.FillPath(&sb, &sp);
        }

        // 背景 + 边框
        {
            GraphicsPath bp;
            roundRect(bp, RectF(0.5f, 0.5f, static_cast<REAL>(w - 1), static_cast<REAL>(h - 4)), radius);
            SolidBrush bg(Color(255, 255, 255, 255));
            g.FillPath(&bg, &bp);
            Pen pen(Color(255, 213, 217, 222), 1.0f);
            g.DrawPath(&pen, &bp);
        }

        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);

        for (int i = 0; i < static_cast<int>(ToolbarAction::Count); ++i) {
            const RECT &r = itemRects_[i];
            const bool hovered = (hover_ == i);
            const bool active = isActive(i);
            const bool iconLike = (i == static_cast<int>(ToolbarAction::Close));

            if (hovered || active) {
                GraphicsPath hp;
                roundRect(hp,
                          RectF(static_cast<REAL>(r.left), static_cast<REAL>(r.top),
                                static_cast<REAL>(r.right - r.left), static_cast<REAL>(r.bottom - r.top)),
                          scaleOf(4, s));
                if (active) {
                    SolidBrush hb(Color(255, 232, 240, 254));
                    g.FillPath(&hb, &hp);
                    Pen hpPen(Color(255, 182, 205, 245), 1.0f);
                    g.DrawPath(&hpPen, &hp);
                } else {
                    SolidBrush hb(Color(255, 242, 245, 250));
                    g.FillPath(&hb, &hp);
                }
            }

            Color textColor;
            if (active) {
                textColor = Color(255, 43, 108, 246);
            } else if (iconLike) {
                textColor = hovered ? Color(255, 200, 60, 60) : Color(255, 120, 126, 136);
            } else {
                textColor = hovered ? Color(255, 20, 24, 32) : Color(255, 48, 54, 64);
            }

            SolidBrush brush(textColor);
            const std::wstring label = labelOf(i);
            RectF tr(static_cast<REAL>(r.left), static_cast<REAL>(r.top), static_cast<REAL>(r.right - r.left),
                     static_cast<REAL>(r.bottom - r.top));
            g.DrawString(label.c_str(), -1, &font, tr, &sf, &brush);
        }
    }

    // GDI+ 在 GDI DC 上不写 alpha：手工预乘后写入 DIB
    {
        Rect lockRect(0, 0, w, h);
        BitmapData bd{};
        if (canvas.LockBits(&lockRect, ImageLockModeRead, PixelFormat32bppARGB, &bd) == Ok && bd.Scan0) {
            auto *src = static_cast<const BYTE *>(bd.Scan0);
            auto *dst = static_cast<BYTE *>(bits);
            for (int yy = 0; yy < h; ++yy) {
                const BYTE *srow = src + static_cast<size_t>(yy) * bd.Stride;
                BYTE *drow = dst + static_cast<size_t>(yy) * w * 4;
                for (int xx = 0; xx < w; ++xx) {
                    const BYTE b = srow[xx * 4 + 0];
                    const BYTE gg = srow[xx * 4 + 1];
                    const BYTE rr = srow[xx * 4 + 2];
                    const BYTE a = srow[xx * 4 + 3];
                    drow[xx * 4 + 0] = static_cast<BYTE>(b * a / 255);
                    drow[xx * 4 + 1] = static_cast<BYTE>(gg * a / 255);
                    drow[xx * 4 + 2] = static_cast<BYTE>(rr * a / 255);
                    drow[xx * 4 + 3] = a;
                }
            }
            canvas.UnlockBits(&bd);
        }
    }

    POINT dst{pos_.x, pos_.y};
    SIZE size{w, h};
    POINT src{0, 0};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd_, screenDc, &dst, &size, memDc, &src, 0, &blend, ULW_ALPHA);

    SelectObject(memDc, old);
    DeleteObject(dib);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    SetWindowPos(hwnd_, HWND_TOPMOST, pos_.x, pos_.y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

// ---------------------------------------------------------------- 命中与交互

int ToolbarWindow::hitTest(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return -1;
    for (int i = 0; i < static_cast<int>(ToolbarAction::Count); ++i) {
        if (ptInRect(itemRects_[i], x, y)) return i;
    }
    return -1;
}

RECT ToolbarWindow::buttonRect(int index) const {
    if (index < 0 || index >= static_cast<int>(ToolbarAction::Count)) return RECT{0, 0, 0, 0};
    return itemRects_[index];
}

void ToolbarWindow::trackMouseLeave() {
    if (trackingLeave_ || !hwnd_) return;
    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd_;
    if (TrackMouseEvent(&tme)) trackingLeave_ = true;
}

void ToolbarWindow::setHover(int index) {
    if (hover_ == index) return;
    hover_ = index;
    if (visible()) render();
}

LRESULT CALLBACK ToolbarWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ToolbarWindow *self = reinterpret_cast<ToolbarWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto *cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
        self = static_cast<ToolbarWindow *>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) return self->handleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT ToolbarWindow::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEACTIVATE:
        // 点工具栏不能抢走文档焦点，否则 TSF 会终止组合
        return MA_NOACTIVATE;

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        if (pressed_) {
            POINT cur{};
            if (GetCursorPos(&cur)) {
                const int dx = cur.x - pressScreen_.x;
                const int dy = cur.y - pressScreen_.y;
                const int threshold = scaleOf(4, scale_);
                if (!dragging_ && (std::abs(dx) > threshold || std::abs(dy) > threshold)) dragging_ = true;
                if (dragging_) {
                    moveTo(pressOrigin_.x + dx, pressOrigin_.y + dy);
                    return 0;
                }
            }
        }
        setHover(hitTest(x, y));
        trackMouseLeave();
        return 0;
    }

    case WM_MOUSELEAVE:
        trackingLeave_ = false;
        if (!pressed_) setHover(-1);
        return 0;

    case WM_LBUTTONDOWN: {
        pressed_ = true;
        dragging_ = false;
        GetCursorPos(&pressScreen_);
        pressOrigin_ = pos_;
        SetCapture(hwnd);
        return 0;
    }

    case WM_LBUTTONUP: {
        const bool wasDragging = dragging_;
        pressed_ = false;
        dragging_ = false;
        if (GetCapture() == hwnd) ReleaseCapture();

        if (wasDragging) {
            clampToWorkArea();
            moveTo(pos_.x, pos_.y);
            savePosition();
            return 0;
        }

        const int idx = hitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (idx >= 0 && handler_) handler_(static_cast<ToolbarAction>(idx));
        return 0;
    }

    case WM_CAPTURECHANGED:
        pressed_ = false;
        dragging_ = false;
        return 0;

    case WM_SETCURSOR: {
        POINT pt{};
        if (GetCursorPos(&pt) && hwnd_) {
            ScreenToClient(hwnd_, &pt);
            const int idx = hitTest(pt.x, pt.y);
            SetCursor(LoadCursorW(nullptr, idx >= 0 ? IDC_HAND : IDC_SIZEALL));
            return TRUE;
        }
        break;
    }

    case WM_DESTROY:
        if (hwnd == hwnd_) hwnd_ = nullptr;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
