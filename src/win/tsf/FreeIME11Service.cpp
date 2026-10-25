// FreeIME 11 —— TSF 文本服务实现（MinGW 基础接口兼容）
#include "FreeIME11Service.h"

#include "CandidateWindow.h"

#include <cassert>

#include "../../utils/utils.h"

namespace {

std::wstring Utf8ToWide(const std::string &s) {
    if (s.empty()) return std::wstring();
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w(n, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

bool isShiftDown() { return (::GetKeyState(VK_SHIFT) & 0x8000) != 0; }
bool isCtrlDown() { return (::GetKeyState(VK_CONTROL) & 0x8000) != 0; }
bool isAltDown() { return (::GetKeyState(VK_MENU) & 0x8000) != 0; }

} // namespace

FreeIME11Service::FreeIME11Service() : session_(new freeime11::Session()) {
    session_->loadSettings(freewb::userFreewbPath());
}

FreeIME11Service::~FreeIME11Service() {
    if (context_) context_->Release();
}

// IUnknown
STDMETHODIMP FreeIME11Service::QueryInterface(REFIID riid, void **ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor)) {
        *ppvObj = static_cast<ITfTextInputProcessor *>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink *>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink *>(this);
    } else if (IsEqualIID(riid, IID_ITfEditSession)) {
        *ppvObj = static_cast<ITfEditSession *>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink *>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) FreeIME11Service::AddRef() { return ::InterlockedIncrement(&refCount_); }
STDMETHODIMP_(ULONG) FreeIME11Service::Release() {
    const LONG r = ::InterlockedDecrement(&refCount_);
    if (r == 0) delete this;
    return r;
}

// ITfTextInputProcessor
STDMETHODIMP FreeIME11Service::Activate(ITfThreadMgr *pThreadMgr, TfClientId tfClientId) {
    threadMgr_ = pThreadMgr;
    tfClientId_ = tfClientId;

    ITfKeystrokeMgr *pKeystrokeMgr = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr))) {
        pKeystrokeMgr->AdviseKeyEventSink(tfClientId_, static_cast<ITfKeyEventSink *>(this), TRUE);
        pKeystrokeMgr->Release();
    }

    ITfSource *pSource = nullptr;
    if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, (void **)&pSource))) {
        pSource->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink *>(this), &cookieThreadMgrEventSink_);
        pSource->Release();
    }
    return S_OK;
}

STDMETHODIMP FreeIME11Service::Deactivate() {
    CandidateWindow::instance().hide();
    ITfKeystrokeMgr *pKeystrokeMgr = nullptr;
    if (threadMgr_ && SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, (void **)&pKeystrokeMgr))) {
        pKeystrokeMgr->UnadviseKeyEventSink(tfClientId_);
        pKeystrokeMgr->Release();
    }
    cookieKeyEventSink_ = TF_INVALID_COOKIE;

    ITfSource *pSource = nullptr;
    if (threadMgr_ && SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, (void **)&pSource))) {
        if (cookieThreadMgrEventSink_ != TF_INVALID_COOKIE)
            pSource->UnadviseSink(cookieThreadMgrEventSink_);
        pSource->Release();
    }
    cookieThreadMgrEventSink_ = TF_INVALID_COOKIE;

    if (context_) { context_->Release(); context_ = nullptr; }
    threadMgr_ = nullptr;
    tfClientId_ = TF_CLIENTID_NULL;
    return S_OK;
}

// ITfKeyEventSink
STDMETHODIMP FreeIME11Service::OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
    *pfEaten = FALSE;
    if (lParam & 0x40000000) return S_OK; // 忽略重复

    const bool shift = isShiftDown(), ctrl = isCtrlDown(), alt = isAltDown();
    freeime11::SessionOutcome out = session_->processKey(static_cast<int>(wParam), shift, ctrl, alt);

    if (out.commitText.empty() && out.preedit.empty() && !out.handled) {
        return S_OK;
    }

    if (!out.commitText.empty()) {
        requestEdit(ES_Commit, out.commitText, "", {});
    } else if (!out.preedit.empty()) {
        std::vector<std::wstring> wide;
        wide.reserve(out.cands.size());
        for (auto &c : out.cands) wide.push_back(Utf8ToWide(c));
        requestEdit(ES_UpdatePreedit, "", out.preedit, wide, out.candCursor, out.candHasPrev, out.candHasNext,
                    out.auxText);
    } else {
        requestEdit(ES_Clear, "", "", {});
    }

    if (out.handled) *pfEaten = TRUE;
    return S_OK;
}

STDMETHODIMP FreeIME11Service::OnKeyUp(ITfContext *, WPARAM, LPARAM, BOOL *) { return S_OK; }

STDMETHODIMP FreeIME11Service::OnTestKeyDown(ITfContext *, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
    *pfEaten = FALSE;
    const int vk = static_cast<int>(wParam);
    const bool shift = isShiftDown();
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9') || vk == VK_SPACE || vk == VK_BACK ||
        vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_OEM_MINUS || vk == VK_OEM_PLUS ||
        vk == VK_OEM_1 || vk == VK_OEM_7 || vk == VK_OEM_PERIOD || vk == VK_OEM_COMMA ||
        vk == VK_OEM_2 || vk == VK_OEM_3 || vk == VK_OEM_4 || vk == VK_OEM_5 || vk == VK_OEM_6) {
        *pfEaten = TRUE;
    }
    if (shift && vk == VK_RSHIFT) *pfEaten = TRUE;
    return S_OK;
}

STDMETHODIMP FreeIME11Service::OnTestKeyUp(ITfContext *, WPARAM, LPARAM, BOOL *) { return S_OK; }
STDMETHODIMP FreeIME11Service::OnPreservedKey(ITfContext *, REFGUID, BOOL *) { return E_NOTIMPL; }
STDMETHODIMP FreeIME11Service::OnSetFocus(BOOL) { return S_OK; }

// ITfThreadMgrEventSink
STDMETHODIMP FreeIME11Service::OnInitDocumentMgr(ITfDocumentMgr *) { return S_OK; }
STDMETHODIMP FreeIME11Service::OnUninitDocumentMgr(ITfDocumentMgr *) { return S_OK; }

STDMETHODIMP FreeIME11Service::OnSetFocus(ITfDocumentMgr *pdimFocus, ITfDocumentMgr *) {
    CandidateWindow::instance().hide();
    requestEdit(ES_Clear, "", "", {});
    if (pdimFocus) {
        ITfContext *pic = nullptr;
        if (SUCCEEDED(pdimFocus->GetTop(&pic)) && pic) {
            if (context_) context_->Release();
            context_ = pic;
            if (contextComposition_) contextComposition_->Release();
            contextComposition_ = nullptr;
            context_->QueryInterface(IID_ITfContextComposition, (void **)&contextComposition_);
            compositionUnsupported_ = false;
            session_->reset();
        }
    }
    return S_OK;
}

STDMETHODIMP FreeIME11Service::OnPushContext(ITfContext *pic) {
    if (context_) context_->Release();
    context_ = pic;
    if (context_) context_->AddRef();
    return S_OK;
}

STDMETHODIMP FreeIME11Service::OnPopContext(ITfContext *pic) {
    CandidateWindow::instance().hide();
    if (context_ == pic) {
        context_->Release();
        context_ = nullptr;
    }
    return S_OK;
}

// ITfEditSession
STDMETHODIMP FreeIME11Service::DoEditSession(TfEditCookie ec) {
    switch (editType_) {
    case ES_Commit:
        commitText(ec, editCommitText_);
        break;
    case ES_UpdatePreedit:
        updateComposition(ec, editPreeditText_, editCands_);
        break;
    case ES_Clear:
        terminateComposition(ec);
        break;
    }
    return S_OK;
}

// ITfCompositionSink
STDMETHODIMP FreeIME11Service::OnCompositionTerminated(TfEditCookie, ITfComposition *pComposition) {
    if (composition_ == pComposition) {
        composition_->Release();
        composition_ = nullptr;
        compositionStarted_ = false;
    }
    return S_OK;
}

BOOL FreeIME11Service::isComposing() const { return composition_ != nullptr; }

HRESULT FreeIME11Service::requestEdit(EditSessionType type, const std::string &textUtf8, const std::string &preeditUtf8,
                                      const std::vector<std::wstring> &cands, int cursor, bool hasPrev,
                                      bool hasNext, const std::string &aux) {
    if (!context_) return E_FAIL;
    editType_ = type;
    editCommitText_ = textUtf8;
    editPreeditText_ = preeditUtf8;
    editCands_ = cands;
    editCursor_ = cursor;
    editHasPrev_ = hasPrev;
    editHasNext_ = hasNext;
    editAux_ = aux;

    HRESULT hr = E_FAIL;
    ITfEditSession *pSession = nullptr;
    if (SUCCEEDED(QueryInterface(IID_ITfEditSession, (void **)&pSession))) {
        hr = context_->RequestEditSession(tfClientId_, pSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
        pSession->Release();
    }
    return hr;
}

// 取插入符所在范围（调用方负责 Release）。失败返回 false。
bool FreeIME11Service::selectionRange(TfEditCookie ec, ITfRange **ppRange) const {
    *ppRange = nullptr;
    if (!context_) return false;
    TF_SELECTION sel{};
    ULONG n = 0;
    if (FAILED(context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &n)) || n != 1 || !sel.range)
        return false;
    *ppRange = sel.range;
    return true;
}

HRESULT FreeIME11Service::commitText(TfEditCookie ec, const std::string &utf8) {
    terminateComposition(ec);
    if (utf8.empty()) return S_OK;

    std::wstring w = Utf8ToWide(utf8);

    // 必须插在插入符处：GetStart 是文档开头，用它会把字打到文件最前面
    ITfRange *range = nullptr;
    if (!selectionRange(ec, &range)) {
        if (FAILED(context_->GetStart(ec, &range)) || !range) return E_FAIL;
    }

    range->SetText(ec, 0, w.c_str(), static_cast<LONG>(w.size()));
    // 把光标移到插入内容之后，否则后续输入会覆盖刚上屏的字
    range->Collapse(ec, TF_ANCHOR_END);
    TF_SELECTION after{};
    after.range = range;
    after.style.ase = TF_AE_NONE;
    after.style.fInterimChar = FALSE;
    context_->SetSelection(ec, 1, &after);

    range->Release();
    return S_OK;
}

HRESULT FreeIME11Service::updateComposition(TfEditCookie ec, const std::string &preeditUtf8,
                                            const std::vector<std::wstring> &cands) {
    std::wstring preedit = Utf8ToWide(preeditUtf8);
    if (preedit.empty()) {
        terminateComposition(ec);
        return S_OK;
    }

    // 组字：优先在插入符处建立 TSF 组合；不支持组合的应用（终端/部分 Electron）降级为「只出候选窗」
    if (!isComposing() && !compositionUnsupported_) {
        ITfRange *range = nullptr;
        if (!selectionRange(ec, &range)) {
            if (FAILED(context_->GetStart(ec, &range)) || !range) return E_FAIL;
        }
        range->Collapse(ec, TF_ANCHOR_END);
        HRESULT hrStart = E_FAIL;
        if (contextComposition_) {
            hrStart = contextComposition_->StartComposition(ec, range, static_cast<ITfCompositionSink *>(this),
                                                            &composition_);
        }
        range->Release();
        if (FAILED(hrStart)) {
            // 该上下文不支持组合：不再尝试，改为只在候选窗里显示，提交时直接插入
            compositionUnsupported_ = true;
            composition_ = nullptr;
        } else {
            compositionStarted_ = true;
        }
    }

    if (isComposing()) {
        ITfRange *range = nullptr;
        if (SUCCEEDED(composition_->GetRange(&range)) && range) {
            range->SetText(ec, 0, preedit.c_str(), static_cast<LONG>(preedit.size()));
            // 组合属性标记
            ITfProperty *prop = nullptr;
            if (SUCCEEDED(context_->GetProperty(GUID_PROP_COMPOSING, &prop))) {
                VARIANT var;
                var.vt = VT_I4;
                var.lVal = 1;
                prop->SetValue(ec, range, &var);
                prop->Release();
            }
            range->Release();
        }
    }

    // 候选窗：跟随插入符
    if (!cands.empty()) {
        POINT pt{0, 0};
        if (!caretScreenPos(ec, pt)) {
            // 兜底：鼠标位置，至少不会跑到左上角
            GetCursorPos(&pt);
        }
        CandidateWindow::instance().show(cands, editCursor_, editHasPrev_, editHasNext_, Utf8ToWide(editAux_), pt);
    } else {
        CandidateWindow::instance().hide();
    }
    return S_OK;
}

// 插入符屏幕坐标（左下方）。优先用 TSF 的 GetTextExt，失败再退回 GUI 线程的 caret。
bool FreeIME11Service::caretScreenPos(TfEditCookie ec, POINT &pt) const {
    if (context_) {
        TF_SELECTION sel{};
        ULONG n = 0;
        if (SUCCEEDED(context_->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &n)) && n == 1 && sel.range) {
            RECT rc{};
            BOOL clipped = FALSE;
            HRESULT hr = E_FAIL;
            ITfContextView *view = nullptr;
            if (SUCCEEDED(context_->GetActiveView(&view)) && view) {
                hr = view->GetTextExt(ec, sel.range, &rc, &clipped);
                if (FAILED(hr)) {
                    RECT vr{};
                    if (SUCCEEDED(view->GetScreenExt(&vr))) {
                        rc = vr;
                        hr = S_OK;
                    }
                }
                view->Release();
            }
            sel.range->Release();
            if (SUCCEEDED(hr)) {
                pt.x = rc.left;
                pt.y = rc.bottom;
                return true;
            }
        }
    }

    // 兜底：当前线程焦点窗口的插入符
    GUITHREADINFO gti{};
    gti.cbSize = sizeof(gti);
    if (GetGUIThreadInfo(GetCurrentThreadId(), &gti) && gti.hwndCaret) {
        POINT p{gti.rcCaret.left, gti.rcCaret.bottom};
        if (ClientToScreen(gti.hwndCaret, &p)) {
            pt = p;
            return true;
        }
    }
    return false;
}

HRESULT FreeIME11Service::terminateComposition(TfEditCookie ec) {
    CandidateWindow::instance().hide();
    if (isComposing()) {
        composition_->EndComposition(ec);
        composition_->Release();
        composition_ = nullptr;
        compositionStarted_ = false;
    }
    return S_OK;
}
