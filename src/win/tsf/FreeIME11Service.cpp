// SPDX-License-Identifier: MulanPub-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PubL v2.
// You can use this software according to the terms and conditions of the Mulan PubL v2.
// You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PubL v2 for more details.

// FreeIME 11 —— TSF 文本服务实现（MinGW 基础接口兼容）
#include "FreeIME11Service.h"

#include "CandidateWindow.h"
#include "ToolbarWindow.h"

#include <shellapi.h>

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <new>

#include "../../utils/utils.h"

namespace {

// 按键追踪：只有设了环境变量 FREEIME11_TRACE=1 才写 %TEMP%\freeime11-keys.log。
// TSF 的问题（某个应用根本不投递 Shift、候选框丢失等）在外部看不出来，只能靠这个。
void traceKey(const char *fmt, ...) {
    static FILE *fp = nullptr;
    static bool inited = false;
    if (!inited) {
        inited = true;
        if (::GetEnvironmentVariableW(L"FREEIME11_TRACE", nullptr, 0) > 0) {
            wchar_t path[MAX_PATH]{};
            if (::GetTempPathW(MAX_PATH, path)) {
                wcscat_s(path, L"freeime11-keys.log");
                fp = _wfopen(path, L"a");
            }
        }
    }
    if (!fp) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    std::fputc('\n', fp);
    std::fflush(fp);
}

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

// 取本模块句柄用的地址锚点（GetModuleHandleEx 需要模块内地址）
void modulePathAnchor() {}

// 左/右 Shift 都算“Shift 键本体”，VK_SHIFT 是通用码（有些应用只发通用码）
bool isShiftVk(int vk) { return vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT; }

} // namespace

// ---------------------------------------------------------------- EditRequest

FreeIME11Service::EditRequest::EditRequest(FreeIME11Service *owner, ITfContext *context, EditSessionType type,
                                           std::string commitUtf8, std::string preeditUtf8,
                                           std::vector<std::wstring> cands, int cursor, bool hasPrev, bool hasNext,
                                           std::string aux, ITfComposition *target)
    : owner_(owner), type_(type), commitUtf8_(std::move(commitUtf8)), preeditUtf8_(std::move(preeditUtf8)),
      cands_(std::move(cands)), cursor_(cursor), hasPrev_(hasPrev), hasNext_(hasNext), aux_(std::move(aux)),
      target_(target), context_(context) {
    if (owner_) owner_->AddRef();
    if (target_) target_->AddRef();
    if (context_) context_->AddRef();
}

FreeIME11Service::EditRequest::~EditRequest() {
    if (context_) context_->Release();
    if (target_) target_->Release();
    if (owner_) owner_->Release();
}

STDMETHODIMP FreeIME11Service::EditRequest::QueryInterface(REFIID riid, void **ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
        *ppvObj = static_cast<ITfEditSession *>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) FreeIME11Service::EditRequest::AddRef() { return ::InterlockedIncrement(&ref_); }

STDMETHODIMP_(ULONG) FreeIME11Service::EditRequest::Release() {
    const LONG r = ::InterlockedDecrement(&ref_);
    if (r == 0) delete this;
    return r;
}

STDMETHODIMP FreeIME11Service::EditRequest::DoEditSession(TfEditCookie ec) {
    if (owner_)
        owner_->applyEdit(ec, type_, commitUtf8_, preeditUtf8_, cands_, cursor_, hasPrev_, hasNext_, aux_, target_,
                          context_);
    return S_OK;
}

// ---------------------------------------------------------------- 构造/析构

FreeIME11Service::FreeIME11Service() : session_(new freeime11::Session()) {
    session_->loadSettings(freewb::userFreewbPath());
}

FreeIME11Service::~FreeIME11Service() {
    // 候选窗/工具栏都是进程级单例，销毁前必须摘掉指向本对象的回调，避免悬空指针
    CandidateWindow::instance().setHandlers(nullptr, nullptr);
    ToolbarWindow::instance().setHandler(nullptr);
    if (composition_) composition_->Release();
    if (contextComposition_) contextComposition_->Release();
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

    // 激活时焦点通常已经建立，之后不会再收到 OnSetFocus。
    // 这里主动接管一次当前焦点上下文，否则 context_ 为空 → 按键被吃掉却无处上屏，
    // 表现就是“刚切过来打字没反应，点一下编辑框才好”。
    ITfDocumentMgr *pdim = nullptr;
    if (SUCCEEDED(threadMgr_->GetFocus(&pdim)) && pdim) {
        ITfContext *pic = nullptr;
        if (SUCCEEDED(pdim->GetTop(&pic)) && pic) {
            adoptContext(pic);
            pic->Release();
        }
        pdim->Release();
    }

    // 候选窗鼠标点选 / 翻页箭头
    CandidateWindow::instance().setHandlers(
        [this](int index) { onCandidateSelected(index); },
        [this](int delta) { onCandidatePage(delta); });

    // 状态工具栏：注册回调，是否显示由“有没有编辑上下文”决定
    ToolbarWindow::instance().setHandler([this](ToolbarAction a) { onToolbarAction(a); });
    syncToolbar();
    updateToolbarVisibility();
    return S_OK;
}

STDMETHODIMP FreeIME11Service::Deactivate() {
    CandidateWindow::instance().hide();
    ToolbarWindow::instance().hide();
    toolbarDismissed_ = false;  // 下次切回输入法时重新允许弹出
    // 切走输入法时如果还在组字，先把编码串从文档里清掉（EndComposition 不会删除文字）
    if (context_ && composition_) clearCompositionOn(context_);

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

    if (composition_) { composition_->Release(); composition_ = nullptr; compositionStarted_ = false; }
    if (contextComposition_) { contextComposition_->Release(); contextComposition_ = nullptr; }
    if (context_) { context_->Release(); context_ = nullptr; }
    threadMgr_ = nullptr;
    tfClientId_ = TF_CLIENTID_NULL;
    return S_OK;
}

// ITfKeyEventSink
STDMETHODIMP FreeIME11Service::OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
    *pfEaten = FALSE;
    const int vk = static_cast<int>(wParam);
    traceKey("DOWN  vk=%02X shift=%d pending=%d", vk, isShiftDown() ? 1 : 0, shiftTapPending_ ? 1 : 0);
    // 忽略系统重复键，但候选导航键允许长按连续移动
    if (lParam & 0x40000000) {
        const bool nav = (vk == VK_UP || vk == VK_DOWN || vk == VK_PRIOR || vk == VK_NEXT);
        if (!nav) return S_OK;
    }

    // 始终以实际收到按键的 context 为准：切换应用/切换输入法后 OnSetFocus 可能没送到，
    // 旧的 context_ 会指向已经失焦的文档，导致编辑请求失败（按键吞掉却没反应）。
    if (pic) adoptContext(pic);

    const bool shift = isShiftDown(), ctrl = isCtrlDown(), alt = isAltDown();

    // Shift 只吃下、不切换：真正的判定在松开时做，Shift+字母/方向键不会误切中英
    if (isShiftVk(vk)) {
        /* 标记必须在「do」阶段也打一次：有些应用（Win11 记事本这类打包应用）
           压根不调用 OnTestKeyDown，只走 OnKeyDown，标记打不上就等于 Shift 没反应。
           长按的重复键不能把已被其它键取消的标记重新点亮，所以看 lParam 的 repeat 位。 */
        if (!(lParam & 0x40000000)) shiftTapPending_ = true;
        *pfEaten = TRUE;
        return S_OK;
    }

    // 这里能看到所有按键（包括最终被放行的），任何其它键都取消「Shift 单击」判定
    shiftTapPending_ = false;

    // 不该归输入法的键直接放行：Ctrl/Alt/Win 组合、空闲态的空格/回车/退格/数字等
    if (!session_->wantsKey(vk, shift, ctrl, alt)) return S_OK;

    freeime11::SessionOutcome out = session_->processKey(vk, shift, ctrl, alt);
    dispatchOutcome(out);
    syncToolbar();

    if (out.handled) *pfEaten = TRUE;
    return S_OK;
}

void FreeIME11Service::dispatchOutcome(const freeime11::SessionOutcome &out) {
    if (out.commitText.empty() && out.preedit.empty() && !out.handled) return;

    if (!out.commitText.empty()) {
        // 先收候选窗：编辑会话是异步的，等它执行完再收会有明显延迟
        CandidateWindow::instance().hide();
        requestEdit(ES_Commit, out.commitText, "", {});
        return;
    }

    if (!out.preedit.empty()) {
        std::vector<std::wstring> wide;
        wide.reserve(out.cands.size());
        for (auto &c : out.cands) wide.push_back(Utf8ToWide(c));
        requestEdit(ES_UpdatePreedit, "", out.preedit, wide, out.candCursor, out.candHasPrev, out.candHasNext,
                    out.auxText);
        return;
    }

    // 会话只吞了键、没给出新的编码串（Ctrl 快捷键、在线造词引导等）时，
    // 只要会话里还有未完成输入，就保持编码框原样 —— 不收起、不关闭、不重置输入状态。
    if (out.handled && session_->hasPendingInput() && CandidateWindow::instance().hasPreedit()) return;

    requestEdit(ES_Clear, "", "", {});
}

void FreeIME11Service::onCandidateSelected(int index) {
    // 点选候选：窗口先收起（编辑会话是异步的），再走与键盘选词相同的上屏流程
    freeime11::SessionOutcome out = session_->selectCandidateOnPage(index);
    if (!out.commitText.empty()) CandidateWindow::instance().hide();
    dispatchOutcome(out);
}

void FreeIME11Service::onCandidatePage(int delta) {
    // 翻页：编辑会话会带着新一页候选重新调用 show()，悬停态由窗口自己按新布局重算
    dispatchOutcome(session_->pageCandidates(delta));
}

// ---------------------------------------------------------------- 工具栏

void FreeIME11Service::syncToolbar() {
    ToolbarState st;
    st.fullShape = session_->fullShape();
    st.chinesePunc = session_->chinesePunc();
    st.simpTrad = session_->simpTrad();
    st.engine = session_->engineName();
    ToolbarWindow::instance().setState(st);
}

// 键盘模式的中文名（切模式时弹提示用）
std::wstring modeLabelWide(const freeime11::Session &s) {
    const std::string name = s.engineName();
    if (name == "wbzx") return L"五笔";
    if (name == "py") return L"拼音";
    if (name == "en") return L"英文";
    return L"五笔拼音";
}
void FreeIME11Service::cycleModeFromShortcut() {
    // 组字中按 Shift：先把「候选框输入框里的原文」上屏（与回车同效果），再切模式。
    // commitText 会把组合区间里的编码串换成正文并结束组合，所以这一条不能再叠加清除，
    // 否则编辑会话是异步排队的，后到的清除会把刚上屏的原文抹掉。
    const std::string raw = session_->rawPreedit();
    if (!raw.empty()) {
        requestEdit(ES_Commit, raw, "", {});
    } else if (composition_) {
        // 没有原文可上屏，但组合还在：把文档里那串编码清掉
        CandidateWindow::instance().hide();
        requestEdit(ES_Clear, "", "", {});
    }
    session_->cycleMode();
    syncToolbar();
    traceKey("CYCLE -> %s", session_->engineName().c_str());
    // 四模式链里「五笔拼音 → 五笔」都是打中文，不给反馈就像按了没反应，这里弹一条短提示
    CandidateWindow::instance().showHint(modeLabelWide(*session_));
}

void FreeIME11Service::onToolbarAction(ToolbarAction action) {
    switch (action) {
    case ToolbarAction::Mode: {
        /* 正在组字时，输入框里的原文先上屏（与回车同效果），再切模式。
           顺序不能反：cycleMode() 会复位会话，原文就没了。
           commitText 会把组合区间里的编码串换成正文并结束组合，
           所以这里不能再叠加一次清除（编辑会话异步排队，后到的清除会抹掉原文）。 */
        const std::string raw = session_->rawPreedit();
        if (!raw.empty()) {
            requestEdit(ES_Commit, raw, "", {});
        } else if (composition_) {
            CandidateWindow::instance().hide();
            requestEdit(ES_Clear, "", "", {});
        }
        session_->cycleMode();
        break;
    }
    case ToolbarAction::FullShape:
        session_->toggleFullShape();
        break;
    case ToolbarAction::ChinesePunc:
        session_->toggleChinesePunc();
        break;
    case ToolbarAction::SimpTrad:
        session_->toggleSimpTrad();
        break;
    case ToolbarAction::Settings:
        launchSettings();
        return;
    case ToolbarAction::Close:
        // 只收起本次激活，下次切回输入法会重新出现
        toolbarDismissed_ = true;
        ToolbarWindow::instance().hide();
        return;
    default:
        return;
    }

    syncToolbar();
    if (action == ToolbarAction::Mode) {
        CandidateWindow::instance().showHint(modeLabelWide(*session_));
    }
}

void FreeIME11Service::launchSettings() const {
    HMODULE mod = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(&modulePathAnchor), &mod) ||
        !mod) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    if (!::GetModuleFileNameW(mod, path, MAX_PATH)) return;
    wchar_t *slash = wcsrchr(path, L'\\');
    if (!slash) return;
    *(slash + 1) = L'\0';
    wcscat_s(path, L"freeime11-settings.exe");
    ::ShellExecuteW(nullptr, L"open", path, nullptr, nullptr, SW_SHOWNORMAL);
}

STDMETHODIMP FreeIME11Service::OnKeyUp(ITfContext *, WPARAM wParam, LPARAM, BOOL *pfEaten) {
    *pfEaten = FALSE;
    const int vk = static_cast<int>(wParam);
    if (!isShiftVk(vk)) return S_OK;

    traceKey("UP    vk=%02X pending=%d ctrl=%d alt=%d", static_cast<int>(wParam), shiftTapPending_ ? 1 : 0,
             isCtrlDown() ? 1 : 0, isAltDown() ? 1 : 0);
    *pfEaten = TRUE;
    // 只有“按下后没碰过任何其它键”的 Shift 才算一次模式切换（左右 Shift 都算）
    const bool tap = shiftTapPending_ && !isCtrlDown() && !isAltDown();
    shiftTapPending_ = false;
    if (tap) cycleModeFromShortcut();
    return S_OK;
}

STDMETHODIMP FreeIME11Service::OnTestKeyDown(ITfContext *, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) {
    *pfEaten = FALSE;
    const int vk = static_cast<int>(wParam);
    traceKey("TEST  vk=%02X repeat=%d", vk, (lParam & 0x40000000) ? 1 : 0);
    const bool shift = isShiftDown(), ctrl = isCtrlDown(), alt = isAltDown();

    // Shift 必须被“吃下”，否则 TSF 根本不会回调 OnKeyDown/OnKeyUp，
    // 也就永远等不到“松开”这个时机去做切换（这正是之前在按下阶段切换失效的原因）。
    if (isShiftVk(vk) && !ctrl && !alt) {
        // 长按的重复键不能把已被其它键取消的标记重新点亮
        if (!(lParam & 0x40000000)) shiftTapPending_ = true;
        *pfEaten = TRUE;
        return S_OK;
    }

    // 这里能看到所有按键（包括最终被放行的），任何其它键都取消“Shift 单击”判定
    shiftTapPending_ = false;

    *pfEaten = session_->wantsKey(vk, shift, ctrl, alt) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP FreeIME11Service::OnTestKeyUp(ITfContext *, WPARAM wParam, LPARAM, BOOL *pfEaten) {
    *pfEaten = FALSE;
    const int vk = static_cast<int>(wParam);
    // 与按下时保持一致地吃下，才能稳定收到 OnKeyUp
    if (isShiftVk(vk) && !isCtrlDown() && !isAltDown()) *pfEaten = TRUE;
    return S_OK;
}
STDMETHODIMP FreeIME11Service::OnPreservedKey(ITfContext *, REFGUID, BOOL *) { return E_NOTIMPL; }
STDMETHODIMP FreeIME11Service::OnSetFocus(BOOL) { return S_OK; }

// ITfThreadMgrEventSink
STDMETHODIMP FreeIME11Service::OnInitDocumentMgr(ITfDocumentMgr *) { return S_OK; }
STDMETHODIMP FreeIME11Service::OnUninitDocumentMgr(ITfDocumentMgr *) { return S_OK; }

STDMETHODIMP FreeIME11Service::OnSetFocus(ITfDocumentMgr *pdimFocus, ITfDocumentMgr *) {
    CandidateWindow::instance().hide();
    // 切走前把旧文档里的组合文本清干净（EndComposition 本身不会删除文字）
    if (context_ && composition_) clearCompositionOn(context_);

    ITfContext *pic = nullptr;
    if (pdimFocus && SUCCEEDED(pdimFocus->GetTop(&pic)) && pic) {
        adoptContext(pic);  // 内部会按上下文刷新工具栏可见性
        pic->Release();
    } else {
        // 焦点离开所有文档（点桌面、切到无输入框的窗口）：释放上下文并收起工具栏。
        // 这里必须真的清掉 context_，否则回到同一个文档时 adoptContext 会提前返回，
        // 工具栏再也不会重新出现。
        adoptContext(nullptr);
    }
    shiftTapPending_ = false;
    session_->reset();
    return S_OK;
}

STDMETHODIMP FreeIME11Service::OnPushContext(ITfContext *pic) {
    if (pic) adoptContext(pic);
    return S_OK;
}

STDMETHODIMP FreeIME11Service::OnPopContext(ITfContext *pic) {
    CandidateWindow::instance().hide();
    if (context_ == pic) {
        context_->Release();
        context_ = nullptr;
    }
    // 弹出（例如关闭模态框）后必须重新接管当前焦点上下文，否则之后所有编辑请求都失败，
    // 输入法看起来就是“卡住不响应”，直到用户重新点一下编辑框。
    if (threadMgr_) {
        ITfDocumentMgr *pdim = nullptr;
        if (SUCCEEDED(threadMgr_->GetFocus(&pdim)) && pdim) {
            ITfContext *focus = nullptr;
            if (SUCCEEDED(pdim->GetTop(&focus)) && focus) {
                adoptContext(focus);
                focus->Release();
            }
            pdim->Release();
        }
    }
    session_->reset();
    return S_OK;
}

// ITfCompositionSink
STDMETHODIMP FreeIME11Service::OnCompositionTerminated(TfEditCookie, ITfComposition *pComposition) {
    // 我们自己结束组合时会先把 composition_ 摘钩，此时不需要动会话状态：
    // 用户可能已经在排队中的下一键里开始了新编码，重置会把那串编码吞掉。
    if (composition_ == pComposition) {
        composition_->Release();
        composition_ = nullptr;
        compositionStarted_ = false;
        // 编码框里还有未完成内容时不能重置会话，否则用户打到一半的编码会凭空消失。
        if (!CandidateWindow::instance().hasPreedit()) session_->reset();
    }
    // 同理：还有未完成输入就保持输入框可见，组合被应用单方面终止也不自动收起。
    // 正常的「输入结束/焦点切换/上屏」路径都会先调用 hide() 清空编码，这里才会收起。
    if (!CandidateWindow::instance().hasPreedit()) CandidateWindow::instance().hide();
    return S_OK;
}

BOOL FreeIME11Service::isComposing() const { return composition_ != nullptr; }

// ---------------------------------------------------------------- 编辑请求

HRESULT FreeIME11Service::requestEdit(EditSessionType type, const std::string &textUtf8, const std::string &preeditUtf8,
                                      const std::vector<std::wstring> &cands, int cursor, bool hasPrev,
                                      bool hasNext, const std::string &aux) {
    if (!context_) return E_FAIL;

    // ES_Clear 需要清掉“发起这一次请求时的”组合：异步执行时 composition_ 可能已经被换掉了
    ITfComposition *target = (type == ES_Clear) ? composition_ : nullptr;

    EditRequest *req = new (std::nothrow) EditRequest(this, context_, type, textUtf8, preeditUtf8, cands, cursor,
                                                     hasPrev, hasNext, aux, target);
    if (!req) return E_OUTOFMEMORY;

    HRESULT hrSession = E_FAIL;
    HRESULT hr = context_->RequestEditSession(tfClientId_, req, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hrSession);
    req->Release();
    return FAILED(hr) ? hr : hrSession;
}

void FreeIME11Service::applyEdit(TfEditCookie ec, EditSessionType type, const std::string &commitUtf8,
                                 const std::string &preeditUtf8, const std::vector<std::wstring> &cands, int cursor,
                                 bool hasPrev, bool hasNext, const std::string &aux, ITfComposition *target,
                                 ITfContext *context) {
    if (!context_) return;
    // 排队期间焦点可能已经换到别的窗口：上屏/改预编辑都带着「当时的插入符位置」语义，
    // 拿到新窗口里执行会把字插错地方，直接丢弃。清空类请求带的是自己的组合对象，仍然要执行，
    // 否则原来那串编码会留在旧文档里。
    if (context != context_ && type != ES_Clear) return;
    
    switch (type) {
    case ES_Commit:
        commitText(ec, commitUtf8);
        break;
    case ES_UpdatePreedit:
        updateComposition(ec, preeditUtf8, cands, cursor, hasPrev, hasNext, aux);
        break;
    case ES_Clear:
        CandidateWindow::instance().hide();
        closeComposition(ec, target);
        break;
    }
}

void FreeIME11Service::adoptContext(ITfContext *pic) {
    if (pic == context_) return;

    // 上下文变了：先把旧文档里的组合文本清干净（否则编码串会留在原来的编辑框里）
    if (context_ && composition_) clearCompositionOn(context_);

    // 旧的组合对象不应再被操作（TSF 会随焦点切换终止组合），
    // 这里只释放引用，避免后续编辑写到已经失焦的文档里。
    if (composition_) {
        composition_->Release();
        composition_ = nullptr;
        compositionStarted_ = false;
    }
    if (contextComposition_) {
        contextComposition_->Release();
        contextComposition_ = nullptr;
    }
    if (context_) {
        context_->Release();
        context_ = nullptr;
    }
    compositionUnsupported_ = false;
    // 上下文被重新接管时不要顺手收起编码框：只要还有未完成输入就保持可见、不重置。
    if (!CandidateWindow::instance().hasPreedit()) CandidateWindow::instance().hide();

    if (pic) {
        context_ = pic;
        context_->AddRef();
        context_->QueryInterface(IID_ITfContextComposition, (void **)&contextComposition_);
    }
    // 接管上下文就意味着“输入法在这个窗口可用”，此时才显示工具栏；
    // 没有上下文的激活（后台线程、非焦点线程）不显示，避免多出几条重复的工具栏。
    updateToolbarVisibility();
}

void FreeIME11Service::updateToolbarVisibility() {
    if (context_ && !toolbarDismissed_) {
        if (!ToolbarWindow::instance().visible()) {
            syncToolbar();
            ToolbarWindow::instance().show();
        }
    } else {
        ToolbarWindow::instance().hide();
    }
}

void FreeIME11Service::clearCompositionOn(ITfContext *ctx) {
    if (!ctx || !composition_) return;

    ITfComposition *target = composition_;
    composition_ = nullptr;
    compositionStarted_ = false;

    EditRequest *req = new (std::nothrow) EditRequest(this, ctx, ES_Clear, "", "", {}, 0, false, false,
                                                      std::string(), target);
    target->Release(); // 请求对象已持有引用
    if (!req) return;

    HRESULT hrSession = E_FAIL;
    HRESULT hr = ctx->RequestEditSession(tfClientId_, req, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
    if (FAILED(hr) || FAILED(hrSession)) {
        // 部分应用的文档不允许同步编辑会话，退化为异步清理
        ctx->RequestEditSession(tfClientId_, req, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hrSession);
    }
    req->Release();
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
    // 上屏后候选窗必须立刻收起，否则选完词候选窗会一直挂在输入框下面
    CandidateWindow::instance().hide();

    std::wstring w = Utf8ToWide(utf8);
    bool committedInComposition = false;

    if (composition_) {
        ITfRange *range = nullptr;
        if (SUCCEEDED(composition_->GetRange(&range)) && range) {
            // 把组合区间里的编码串直接替换成最终文本，再结束组合：
            // EndComposition 不删除区间文字，文字因此原样保留为正文；
            // 先换字再结束（而不是先清空、结束后再插入）可以避免空组合，
            // 也避免部分应用把「组合外插入」当成新的未完成输入继续显示。
            HRESULT hrSet = w.empty() ? S_OK
                                      : range->SetText(ec, 0, w.c_str(), static_cast<LONG>(w.size()));
            if (SUCCEEDED(hrSet)) {
                // 光标移到上屏文本之后，否则后续输入会插到刚上屏的字前面
                range->Collapse(ec, TF_ANCHOR_END);
                TF_SELECTION after{};
                after.range = range;
                after.style.ase = TF_AE_NONE;
                after.style.fInterimChar = FALSE;
                context_->SetSelection(ec, 1, &after);
                committedInComposition = true;
            } else {
                // 区间不可写：先把编码串清掉，再在插入符处插入正文
                range->SetText(ec, 0, L"", 0);
                range->Collapse(ec, TF_ANCHOR_START);
            }
            range->Release();
        }

        // 先摘钩再结束：避免 OnCompositionTerminated 回调解引用后这里二次释放
        ITfComposition *comp = composition_;
        composition_ = nullptr;
        compositionStarted_ = false;
        comp->EndComposition(ec);
        comp->Release();
    }

    if (!committedInComposition && !w.empty()) {
        ITfRange *range = nullptr;
        // 必须插在插入符处：GetStart 是文档开头，用它会把字打到文件最前面
        if (!selectionRange(ec, &range) || !range) {
            if (FAILED(context_->GetStart(ec, &range)) || !range) return E_FAIL;
        }

        range->SetText(ec, 0, w.c_str(), static_cast<LONG>(w.size()));

        // 清掉「组字中」属性，否则上屏后的文字仍可能被应用当作未完成的组合
        ITfProperty *prop = nullptr;
        if (SUCCEEDED(context_->GetProperty(GUID_PROP_COMPOSING, &prop)) && prop) {
            prop->Clear(ec, range);
            prop->Release();
        }

        range->Collapse(ec, TF_ANCHOR_END);
        TF_SELECTION after{};
        after.range = range;
        after.style.ase = TF_AE_NONE;
        after.style.fInterimChar = FALSE;
        context_->SetSelection(ec, 1, &after);
        range->Release();
    }
    return S_OK;
}

HRESULT FreeIME11Service::updateComposition(TfEditCookie ec, const std::string &preeditUtf8,
                                            const std::vector<std::wstring> &cands, int cursor, bool hasPrev,
                                            bool hasNext, const std::string &aux) {
    std::wstring preedit = Utf8ToWide(preeditUtf8);
    if (preedit.empty()) {
        terminateComposition(ec);
        return S_OK;
    }

    // 组字：只在文档里占一个「空」组合，用来锁定插入符位置并与应用的输入状态协作。
    // 编码串不再写进组合区间（那会显示在原本用于输入文字的位置），改由候选窗顶部的输入框呈现。
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

    // 组合区间保持为空：既不把 preedit 写进文档，也不给它打「组字中」属性，
    // 否则上屏后的正文可能残留组合样式。上屏时直接把这个空区间替换成最终文本即可。

    // 候选窗（含顶部编码输入框）：跟随插入符。
    // 即使这一页一个候选都没有，只要还有未完成的编码就照样显示，不自动收起、不重置会话。
    POINT pt{0, 0};
    if (!caretScreenPos(ec, pt)) {
        // 兜底：鼠标位置，至少不会跑到左上角
        GetCursorPos(&pt);
    }
    CandidateWindow::instance().show(cands, cursor, hasPrev, hasNext, Utf8ToWide(aux), pt, preedit);
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
            // 空组合时部分应用会返回全零矩形；此时退回 GUI 插入符，别把输入框甩到屏幕左上角
            if (SUCCEEDED(hr) && !(rc.left == 0 && rc.top == 0 && rc.right == 0 && rc.bottom == 0)) {
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
    if (!composition_) return S_OK;

    ITfComposition *comp = composition_;
    composition_ = nullptr;
    compositionStarted_ = false;
    closeComposition(ec, comp);
    comp->Release();
    return S_OK;
}

// 清空组合区间并结束组合。EndComposition 不会删除文字，必须自己 SetText("")。
// 不改变引用计数：target 为空时用当前 composition_（引用仍由调用方/成员持有）。
void FreeIME11Service::closeComposition(TfEditCookie ec, ITfComposition *target) {
    ITfComposition *comp = target ? target : composition_;
    if (!comp) return;

    if (comp == composition_) {
        composition_ = nullptr;
        compositionStarted_ = false;
    }

    ITfRange *range = nullptr;
    if (SUCCEEDED(comp->GetRange(&range)) && range) {
        range->SetText(ec, 0, L"", 0);
        range->Collapse(ec, TF_ANCHOR_START);
        range->Release();
    }
    comp->EndComposition(ec);
}
