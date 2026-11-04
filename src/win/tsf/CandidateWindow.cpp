// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

#include "CandidateWindow.h"

#include <gdiplus.h>
#include <windowsx.h>

#include <algorithm>
#include <mutex>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

namespace {

HINSTANCE g_hinst = nullptr;
const wchar_t *kClassName = L"FreeIME11CandidateWnd";

// GDI+ 惰性初始化：DllMain 里不能调 GdiplusStartup（可能死锁）
ULONG_PTR gdiplusToken = 0;
std::once_flag gdiplusOnce;

void ensureGdiplus() {
    std::call_once(gdiplusOnce, [] {
        GdiplusStartupInput input;
        GdiplusStartup(&gdiplusToken, &input, nullptr);
    });
}

int scaleOf(int base, int pct) { return MulDiv(base, pct, 100); }

// 用于从模块内地址取 HINSTANCE
void moduleAnchor() {}

bool ptInRect(const RECT &r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

}  // namespace

CandidateWindow &CandidateWindow::instance() {
    static CandidateWindow w;
    return w;
}

void CandidateWindow::setInstanceHandle(HINSTANCE hinst) { g_hinst = hinst; }

void CandidateWindow::setHandlers(SelectHandler onSelect, PageHandler onPage) {
    onSelect_ = std::move(onSelect);
    onPage_ = std::move(onPage);
}

int CandidateWindow::dpiScale() const {
    HDC dc = GetDC(nullptr);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);
    return dpi > 0 ? MulDiv(100, dpi, 96) : 100;
}

void CandidateWindow::ensureCreated() {
    if (hwnd_ && IsWindow(hwnd_)) return;
    hwnd_ = nullptr;

    HINSTANCE hinst = g_hinst;
    if (!hinst) {
        // 兜底：用本模块地址取句柄
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&moduleAnchor), &hinst);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &CandidateWindow::wndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);  // 已注册时返回 0，忽略即可

    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED, kClassName,
                            L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, hinst, this);
}

bool CandidateWindow::visible() const { return hwnd_ && IsWindowVisible(hwnd_) != FALSE; }

void CandidateWindow::hide() {
    hover_ = kNone;
    trackingLeave_ = false;
    // 只有「输入确实结束」才会走到这里；把编码清空，避免窗口重新显示时带着旧内容
    preedit_.clear();
    if (hwnd_ && IsWindowVisible(hwnd_)) ShowWindow(hwnd_, SW_HIDE);
}

LRESULT CALLBACK CandidateWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CandidateWindow *self = reinterpret_cast<CandidateWindow *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto *cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
        self = static_cast<CandidateWindow *>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) return self->handleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CandidateWindow::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEACTIVATE:
        // 点击候选窗绝不能抢焦点：一旦文档失焦，TSF 会终止组合，
        // 候选窗跟着消失、后续按键也没人处理，看起来就是「候选框丢了」。
        return MA_NOACTIVATE;

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE: {
        setHover(hitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        trackMouseLeave();
        return 0;
    }

    case WM_MOUSELEAVE:
        trackingLeave_ = false;
        setHover(kNone);
        return 0;

    case WM_LBUTTONDOWN:
        // 吞掉按下消息，避免 DefWindowProc 走窗口激活/拖动逻辑
        return 0;

    case WM_LBUTTONUP: {
        const int idx = hitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (idx >= 0) {
            if (onSelect_) onSelect_(idx);
        } else if (idx == kPrevPage) {
            if (onPage_) onPage_(-1);
        } else if (idx == kNextPage) {
            if (onPage_) onPage_(1);
        }
        return 0;
    }

    case WM_SETCURSOR: {
        POINT pt{};
        if (GetCursorPos(&pt) && hwnd_) {
            ScreenToClient(hwnd_, &pt);
            const int idx = hitTest(pt.x, pt.y);
            const bool clickable = idx >= 0 || idx == kPrevPage || idx == kNextPage;
            SetCursor(LoadCursorW(nullptr, clickable ? IDC_HAND : IDC_ARROW));
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

void CandidateWindow::trackMouseLeave() {
    if (trackingLeave_ || !hwnd_) return;
    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd_;
    if (TrackMouseEvent(&tme)) trackingLeave_ = true;
}

void CandidateWindow::setHover(int index) {
    if (hover_ == index) return;
    hover_ = index;
    if (visible()) render();
}

int CandidateWindow::hitTest(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return kNone;

    for (size_t i = 0; i < itemRects_.size(); ++i) {
        if (ptInRect(itemRects_[i], x, y)) return static_cast<int>(i);
    }
    if (hasArrows_) {
        if (hasPrev_ && ptInRect(prevRect_, x, y)) return kPrevPage;
        if (hasNext_ && ptInRect(nextRect_, x, y)) return kNextPage;
    }
    return kNone;
}

void CandidateWindow::show(const std::vector<std::wstring> &cands, int cursor, bool hasPrev, bool hasNext,
                           const std::wstring &aux, POINT caret, const std::wstring &preedit) {
    preedit_ = preedit;
    // 未完成输入时（例如这一码暂时查不到候选、或用户只打了一半），即使候选为空也要
    // 保留顶部输入框；只有「编码已清空且没有候选」才是真正该收起的时刻。
    if (cands.empty() && preedit_.empty()) {
        hide();
        return;
    }
    ensureCreated();
    if (!hwnd_) return;

    cands_ = cands;
    cursor_ = (cursor >= 0 && cursor < static_cast<int>(cands.size())) ? cursor : 0;
    hasPrev_ = hasPrev;
    hasNext_ = hasNext;
    aux_ = aux;
    caret_ = caret;
    scale_ = dpiScale();
    if (hover_ >= static_cast<int>(cands_.size())) hover_ = kNone;

    render();

    // 兜底：确保窗口真的可见（层级窗口被系统隐藏后不会自己回来）
    if (!IsWindowVisible(hwnd_)) ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

void CandidateWindow::render() {
    ensureGdiplus();
    if (!hwnd_) return;

    const int s = scale_;

    // 字体
    Gdiplus::FontFamily yahei(L"Microsoft YaHei UI");
    Gdiplus::FontFamily segoe(L"Segoe UI");
    Gdiplus::FontFamily *family = yahei.IsAvailable() ? &yahei : &segoe;
    Gdiplus::Font fontText(family, static_cast<REAL>(scaleOf(15, s)), FontStyleRegular, UnitPixel);
    Gdiplus::Font fontNum(family, static_cast<REAL>(scaleOf(11, s)), FontStyleRegular, UnitPixel);

    const int pad = scaleOf(9, s);
    const int gap = scaleOf(14, s);
    const int itemPadX = scaleOf(6, s);
    const int radius = scaleOf(6, s);
    const int arrowW = scaleOf(26, s);
    const int inputH = scaleOf(24, s);   // 顶部编码输入框的行高
    const int rowGap = scaleOf(7, s);    // 输入框与候选行之间的间距

    Bitmap measure(1, 1, PixelFormat32bppARGB);
    Graphics gm(&measure);
    gm.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    // 量一下编码串宽度，决定输入框至少要多宽
    int codeW = 0;
    if (!preedit_.empty()) {
        RectF rCode;
        gm.MeasureString(preedit_.c_str(), -1, &fontText, PointF(0, 0), &rCode);
        codeW = static_cast<int>(rCode.Width + 0.5f);
    }

    // 每个候选：序号 + 文本
    struct Cell {
        std::wstring num;
        std::wstring text;
        int numW = 0;
        int textW = 0;
        int w = 0;
    };
    std::vector<Cell> cells;
    cells.reserve(cands_.size());

    int contentW = 0;
    int lineH = scaleOf(20, s);
    for (size_t i = 0; i < cands_.size(); ++i) {
        Cell c;
        c.num = std::to_wstring(i + 1);
        c.text = cands_[i];

        RectF rNum, rText;
        gm.MeasureString(c.num.c_str(), -1, &fontNum, PointF(0, 0), &rNum);
        gm.MeasureString(c.text.c_str(), -1, &fontText, PointF(0, 0), &rText);
        c.numW = static_cast<int>(rNum.Width + 0.5f);
        c.textW = static_cast<int>(rText.Width + 0.5f);
        c.w = itemPadX * 2 + c.numW + scaleOf(3, s) + c.textW;

        lineH = std::max(lineH, static_cast<int>(rText.Height + 0.5f));
        contentW += c.w;
        if (i) contentW += gap;
        cells.push_back(c);
    }

    const bool hasCands = !cands_.empty();
    const bool paging = hasPrev_ || hasNext_;
    const int arrowsW = paging ? (hasPrev_ ? arrowW : 0) + (hasNext_ ? arrowW : 0) : 0;

    // 顶部输入框至少要有这么宽；候选行更宽时以候选行为准。
    // 没有候选时仍要画出输入框，因此高度里始终包含 inputH。
    const int inputNeed = pad * 2 + codeW + scaleOf(30, s);
    const int candNeed = pad * 2 + contentW + arrowsW;
    const int width = std::max(candNeed, inputNeed);
    const int height = pad * 2 + inputH + (hasCands ? rowGap + lineH : 0);

    // 限制不超过屏幕宽度
    POINT pt = caret_;
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    RECT work{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    if (GetMonitorInfoW(mon, &mi)) work = mi.rcWork;

    int w = width;
    if (w > work.right - work.left - scaleOf(20, s)) w = work.right - work.left - scaleOf(20, s);

    int x = pt.x;
    int y = pt.y + scaleOf(6, s);
    if (x + w > work.right - scaleOf(4, s)) x = work.right - scaleOf(4, s) - w;
    if (x < work.left + scaleOf(4, s)) x = work.left + scaleOf(4, s);
    if (y + height > work.bottom - scaleOf(4, s)) {
        // 下方放不下就放到插入符上方
        y = pt.y - height - scaleOf(28, s);
        if (y < work.top + scaleOf(4, s)) y = work.top + scaleOf(4, s);
    }

    width_ = w;
    height_ = height;

    // 布局结果（客户区坐标）：命中测试与绘制共用。
    // 顶部输入框占满内宽，候选行排在它下面。
    inputRect_ = RECT{pad, pad, w - pad, pad + inputH};
    const int rowY = pad + inputH + rowGap;

    itemRects_.clear();
    itemRects_.reserve(cells.size());
    {
        int cx = pad;
        for (size_t i = 0; i < cells.size(); ++i) {
            RECT r{cx, rowY, cx + cells[i].w, rowY + lineH};
            itemRects_.push_back(r);
            cx += cells[i].w;
            if (i + 1 < cells.size()) cx += gap;
        }
    }
    prevRect_ = RECT{0, 0, 0, 0};
    nextRect_ = RECT{0, 0, 0, 0};
    hasArrows_ = paging && hasCands;
    if (hasArrows_) {
        int ax = w - pad - arrowsW;
        if (hasPrev_) {
            prevRect_ = RECT{ax, rowY, ax + arrowW, rowY + lineH};
            ax += arrowW;
        }
        if (hasNext_) nextRect_ = RECT{ax, rowY, ax + arrowW, rowY + lineH};
    }

    // 用 DIB Section 直接绘制，保证 32bpp alpha 不被 GDI+ 转换丢失
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -height;   // 自上而下
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
    ZeroMemory(bits, static_cast<size_t>(w) * height * 4);

    Gdiplus::Bitmap canvas(w, height, PixelFormat32bppARGB);
    {
        Graphics g(&canvas);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.SetCompositingMode(CompositingModeSourceOver);

        // 阴影
        {
            GraphicsPath sp;
            const int d = radius * 2;
            const RectF sr(static_cast<REAL>(scaleOf(1, s)), static_cast<REAL>(scaleOf(3, s)),
                           static_cast<REAL>(w - scaleOf(2, s)), static_cast<REAL>(height - scaleOf(4, s)));
            sp.AddArc(sr.X, sr.Y, (REAL)d, (REAL)d, 180, 90);
            sp.AddArc(sr.GetRight() - d, sr.Y, (REAL)d, (REAL)d, 270, 90);
            sp.AddArc(sr.GetRight() - d, sr.GetBottom() - d, (REAL)d, (REAL)d, 0, 90);
            sp.AddArc(sr.X, sr.GetBottom() - d, (REAL)d, (REAL)d, 90, 90);
            sp.CloseFigure();
            SolidBrush sb(Color(26, 0, 0, 0));
            g.FillPath(&sb, &sp);
        }

        // 背景 + 边框
        {
            GraphicsPath bp;
            const int d = radius * 2;
            const RectF br(0.5f, 0.5f, static_cast<REAL>(w - 1), static_cast<REAL>(height - 4));
            bp.AddArc(br.X, br.Y, (REAL)d, (REAL)d, 180, 90);
            bp.AddArc(br.GetRight() - d, br.Y, (REAL)d, (REAL)d, 270, 90);
            bp.AddArc(br.GetRight() - d, br.GetBottom() - d, (REAL)d, (REAL)d, 0, 90);
            bp.AddArc(br.X, br.GetBottom() - d, (REAL)d, (REAL)d, 90, 90);
            bp.CloseFigure();
            SolidBrush bg(Color(255, 255, 255, 255));
            g.FillPath(&bg, &bp);
            Pen pen(Color(255, 213, 217, 222), 1.0f);
            g.DrawPath(&pen, &bp);
        }

        auto roundRect = [&](GraphicsPath &p, const RECT &r, int radiusPx) {
            const int d = radiusPx * 2;
            const RectF rr(static_cast<REAL>(r.left), static_cast<REAL>(r.top),
                           static_cast<REAL>(r.right - r.left), static_cast<REAL>(r.bottom - r.top));
            p.AddArc(rr.X, rr.Y, (REAL)d, (REAL)d, 180, 90);
            p.AddArc(rr.GetRight() - d, rr.Y, (REAL)d, (REAL)d, 270, 90);
            p.AddArc(rr.GetRight() - d, rr.GetBottom() - d, (REAL)d, (REAL)d, 0, 90);
            p.AddArc(rr.X, rr.GetBottom() - d, (REAL)d, (REAL)d, 90, 90);
            p.CloseFigure();
        };

        // 顶部「编码输入框」：未完成的外码显示在这里，文档里保持空白。
        // 沿用候选窗自身的视觉语言（白底圆角、细边框、蓝色强调），不套用参考样式。
        {
            GraphicsPath ip;
            roundRect(ip, inputRect_, scaleOf(4, s));
            SolidBrush ib(Color(255, 247, 250, 253));
            g.FillPath(&ib, &ip);
            Pen ipPen(Color(255, 213, 217, 222), 1.0f);
            g.DrawPath(&ipPen, &ip);
        }
        if (!preedit_.empty()) {
            StringFormat sf;
            sf.SetAlignment(StringAlignmentNear);
            sf.SetLineAlignment(StringAlignmentCenter);
            SolidBrush codeBrush(Color(255, 14, 60, 140));
            RectF rCode(static_cast<REAL>(inputRect_.left + itemPadX), static_cast<REAL>(inputRect_.top),
                        static_cast<REAL>(codeW + scaleOf(2, s)), static_cast<REAL>(inputH));
            g.DrawString(preedit_.c_str(), -1, &fontText, rCode, &sf, &codeBrush);
        }
        {
            // 插入符紧跟在编码之后，提示「还在输入、尚未完成」
            const int cx = inputRect_.left + itemPadX + codeW + scaleOf(2, s);
            SolidBrush caretBrush(Color(255, 43, 108, 246));
            g.FillRectangle(&caretBrush,
                            RectF(static_cast<REAL>(cx), static_cast<REAL>(inputRect_.top + scaleOf(5, s)),
                                  static_cast<REAL>(std::max(1, scaleOf(2, s))),
                                  static_cast<REAL>(inputH - scaleOf(10, s))));
        }

        // 候选
        for (size_t i = 0; i < cells.size(); ++i) {
            const bool hi = (static_cast<int>(i) == cursor_);
            const bool hovered = (static_cast<int>(i) == hover_);
            if (hi || hovered) {
                GraphicsPath hp;
                roundRect(hp, itemRects_[i], scaleOf(4, s));
                if (hi) {
                    SolidBrush hb(Color(255, 43, 108, 246));
                    g.FillPath(&hb, &hp);
                } else {
                    // 悬停：淡蓝底，与选中态区分开
                    SolidBrush hb(Color(255, 232, 240, 254));
                    g.FillPath(&hb, &hp);
                    Pen hpPen(Color(255, 182, 205, 245), 1.0f);
                    g.DrawPath(&hpPen, &hp);
                }
            }

            StringFormat sf;
            sf.SetAlignment(StringAlignmentNear);
            sf.SetLineAlignment(StringAlignmentCenter);

            SolidBrush numBrush(hi ? Color(235, 235, 245, 255)
                                   : (hovered ? Color(255, 70, 100, 160) : Color(255, 144, 150, 160)));
            SolidBrush textBrush(hi ? Color(255, 255, 255, 255)
                                    : (hovered ? Color(255, 14, 60, 140) : Color(255, 31, 35, 41)));

            const RECT &ir = itemRects_[i];
            RectF rNum(static_cast<REAL>(ir.left + itemPadX), static_cast<REAL>(ir.top),
                       static_cast<REAL>(cells[i].numW), static_cast<REAL>(lineH));
            g.DrawString(cells[i].num.c_str(), -1, &fontNum, rNum, &sf, &numBrush);

            RectF rText(static_cast<REAL>(ir.left + itemPadX + cells[i].numW + scaleOf(3, s)),
                        static_cast<REAL>(ir.top), static_cast<REAL>(cells[i].textW + scaleOf(2, s)),
                        static_cast<REAL>(lineH));
            g.DrawString(cells[i].text.c_str(), -1, &fontText, rText, &sf, &textBrush);
        }

        // 翻页箭头
        if (paging) {
            Gdiplus::Font fontArrow(L"Segoe UI Symbol", static_cast<REAL>(scaleOf(12, s)), FontStyleRegular,
                                    UnitPixel);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);

            if (hasPrev_) {
                const bool hovered = (hover_ == kPrevPage);
                if (hovered) {
                    GraphicsPath hp;
                    roundRect(hp, prevRect_, scaleOf(4, s));
                    SolidBrush hb(Color(255, 232, 240, 254));
                    g.FillPath(&hb, &hp);
                }
                SolidBrush b(hovered ? Color(255, 14, 60, 140) : Color(255, 107, 114, 128));
                RectF r(static_cast<REAL>(prevRect_.left), static_cast<REAL>(prevRect_.top),
                        static_cast<REAL>(prevRect_.right - prevRect_.left), static_cast<REAL>(lineH));
                g.DrawString(L"\x2039", -1, &fontArrow, r, &sf, &b);
            }
            if (hasNext_) {
                const bool hovered = (hover_ == kNextPage);
                if (hovered) {
                    GraphicsPath hp;
                    roundRect(hp, nextRect_, scaleOf(4, s));
                    SolidBrush hb(Color(255, 232, 240, 254));
                    g.FillPath(&hb, &hp);
                }
                SolidBrush b(hovered ? Color(255, 14, 60, 140) : Color(255, 107, 114, 128));
                RectF r(static_cast<REAL>(nextRect_.left), static_cast<REAL>(nextRect_.top),
                        static_cast<REAL>(nextRect_.right - nextRect_.left), static_cast<REAL>(lineH));
                g.DrawString(L"\x203A", -1, &fontArrow, r, &sf, &b);
            }
        }
    }

    // GDI+ 在 GDI DC 上不写 alpha，这里从 Bitmap 取像素，转预乘后写入 DIB
    {
        Rect lockRect(0, 0, w, height);
        BitmapData bd{};
        if (canvas.LockBits(&lockRect, ImageLockModeRead, PixelFormat32bppARGB, &bd) == Ok && bd.Scan0) {
            auto *src = static_cast<const BYTE *>(bd.Scan0);
            auto *dst = static_cast<BYTE *>(bits);
            for (int yy = 0; yy < height; ++yy) {
                const BYTE *srow = src + static_cast<size_t>(yy) * bd.Stride;
                BYTE *drow = dst + static_cast<size_t>(yy) * w * 4;
                for (int xx = 0; xx < w; ++xx) {
                    const BYTE b = srow[xx * 4 + 0];
                    const BYTE gg = srow[xx * 4 + 1];
                    const BYTE r = srow[xx * 4 + 2];
                    const BYTE a = srow[xx * 4 + 3];
                    drow[xx * 4 + 0] = static_cast<BYTE>(b * a / 255);
                    drow[xx * 4 + 1] = static_cast<BYTE>(gg * a / 255);
                    drow[xx * 4 + 2] = static_cast<BYTE>(r * a / 255);
                    drow[xx * 4 + 3] = a;
                }
            }
            canvas.UnlockBits(&bd);
        }
    }

    POINT dst{x, y};
    SIZE size{w, height};
    POINT src{0, 0};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd_, screenDc, &dst, &size, memDc, &src, 0, &blend, ULW_ALPHA);

    SelectObject(memDc, old);
    DeleteObject(dib);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, w, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}
