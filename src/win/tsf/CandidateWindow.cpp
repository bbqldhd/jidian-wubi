#include "CandidateWindow.h"

#include <gdiplus.h>

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

COLORREF toColorRef(ARGB c) { return RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF); }

}  // namespace

CandidateWindow &CandidateWindow::instance() {
    static CandidateWindow w;
    return w;
}

void CandidateWindow::setInstanceHandle(HINSTANCE hinst) { g_hinst = hinst; }

int CandidateWindow::dpiScale() const {
    HDC dc = GetDC(nullptr);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);
    return dpi > 0 ? MulDiv(100, dpi, 96) : 100;
}

void CandidateWindow::ensureCreated() {
    if (hwnd_) return;

    HINSTANCE hinst = g_hinst;
    if (!hinst) {
        // 兜底：用本模块地址取句柄
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&moduleAnchor), &hinst);
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;  // 不接受输入，无需处理消息
    wc.hInstance = hinst;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);  // 已注册时返回 0，忽略即可

    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED, kClassName,
                            L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, hinst, nullptr);
}

bool CandidateWindow::visible() const { return hwnd_ && IsWindowVisible(hwnd_) != FALSE; }

void CandidateWindow::hide() {
    if (hwnd_ && IsWindowVisible(hwnd_)) ShowWindow(hwnd_, SW_HIDE);
}

void CandidateWindow::show(const std::vector<std::wstring> &cands, int cursor, bool hasPrev, bool hasNext,
                           const std::wstring &aux, POINT caret) {
    if (cands.empty()) {
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

    render();
}

void CandidateWindow::render() {
    ensureGdiplus();

    const int s = scale_;

    // 字体
    Gdiplus::FontFamily family(L"Microsoft YaHei UI");
    if (!family.IsAvailable()) {
        Gdiplus::FontFamily fallback(L"Segoe UI");
        // 这里无法改 family 引用，直接继续用默认字体族亦可
    }
    Gdiplus::Font fontText(&family, static_cast<REAL>(scaleOf(15, s) * 96.0 / 96.0), FontStyleRegular, UnitPixel);
    Gdiplus::Font fontNum(&family, static_cast<REAL>(scaleOf(11, s)), FontStyleRegular, UnitPixel);

    const int pad = scaleOf(9, s);
    const int gap = scaleOf(14, s);
    const int itemPadX = scaleOf(6, s);
    const int radius = scaleOf(6, s);
    const int arrowW = scaleOf(26, s);

    Bitmap measure(1, 1, PixelFormat32bppARGB);
    Graphics gm(&measure);
    gm.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

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

    const bool paging = hasPrev_ || hasNext_;
    const int arrowsW = paging ? (hasPrev_ ? arrowW : 0) + (hasNext_ ? arrowW : 0) : 0;

    const int width = pad * 2 + contentW + arrowsW;
    const int height = pad * 2 + lineH;

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

        // 候选
        int cx = pad;
        const int cy = pad;
        for (size_t i = 0; i < cells.size(); ++i) {
            const bool hi = (static_cast<int>(i) == cursor_);
            if (hi) {
                GraphicsPath hp;
                const int d = scaleOf(4, s) * 2;
                const RectF hr(static_cast<REAL>(cx), static_cast<REAL>(cy), static_cast<REAL>(cells[i].w),
                               static_cast<REAL>(lineH));
                hp.AddArc(hr.X, hr.Y, (REAL)d, (REAL)d, 180, 90);
                hp.AddArc(hr.GetRight() - d, hr.Y, (REAL)d, (REAL)d, 270, 90);
                hp.AddArc(hr.GetRight() - d, hr.GetBottom() - d, (REAL)d, (REAL)d, 0, 90);
                hp.AddArc(hr.X, hr.GetBottom() - d, (REAL)d, (REAL)d, 90, 90);
                hp.CloseFigure();
                SolidBrush hb(Color(255, 43, 108, 246));
                g.FillPath(&hb, &hp);
            }

            StringFormat sf;
            sf.SetAlignment(StringAlignmentNear);
            sf.SetLineAlignment(StringAlignmentCenter);

            SolidBrush numBrush(hi ? Color(235, 235, 245, 255) : Color(255, 144, 150, 160));
            SolidBrush textBrush(hi ? Color(255, 255, 255, 255) : Color(255, 31, 35, 41));

            RectF rNum(static_cast<REAL>(cx + itemPadX), static_cast<REAL>(cy), static_cast<REAL>(cells[i].numW),
                       static_cast<REAL>(lineH));
            g.DrawString(cells[i].num.c_str(), -1, &fontNum, rNum, &sf, &numBrush);

            RectF rText(static_cast<REAL>(cx + itemPadX + cells[i].numW + scaleOf(3, s)), static_cast<REAL>(cy),
                        static_cast<REAL>(cells[i].textW + scaleOf(2, s)), static_cast<REAL>(lineH));
            g.DrawString(cells[i].text.c_str(), -1, &fontText, rText, &sf, &textBrush);

            cx += cells[i].w;
            if (i + 1 < cells.size()) cx += gap;
        }

        // 翻页箭头
        if (paging) {
            Gdiplus::Font fontArrow(L"Segoe UI Symbol", static_cast<REAL>(scaleOf(12, s)), FontStyleRegular,
                                    UnitPixel);
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            int ax = w - pad - arrowsW;
            if (hasPrev_) {
                SolidBrush b(Color(255, 107, 114, 128));
                RectF r(static_cast<REAL>(ax), static_cast<REAL>(cy), static_cast<REAL>(arrowW),
                        static_cast<REAL>(lineH));
                g.DrawString(L"\x2039", -1, &fontArrow, r, &sf, &b);
                ax += arrowW;
            }
            if (hasNext_) {
                SolidBrush b(Color(255, 107, 114, 128));
                RectF r(static_cast<REAL>(ax), static_cast<REAL>(cy), static_cast<REAL>(arrowW),
                        static_cast<REAL>(lineH));
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
