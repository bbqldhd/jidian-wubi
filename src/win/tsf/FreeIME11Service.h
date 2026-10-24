// FreeIME 11 —— TSF 文本服务（Windows 11，兼容 MinGW msctf.h 基础接口）
// 将可移植 Session 会话层接入 Text Services Framework。
// 候选：在组合文本中内嵌显示（MinGW 头不含 ITfCandidateListUIElement；
//       使用 MSVC + 完整 Windows SDK 时可切换为系统集成候选 UI）。
#pragma once

#include <windows.h>
#include <msctf.h>
#include <textstor.h>

#include <memory>
#include <string>
#include <vector>

#include "../../session/session.h"

// GUID 定义（全部重新生成，与旧版 10.8.9 无冲突，可并存安装）
#define FREEWB_TEXTSERVICE_CLSID   L"{B6F2C8A1-7D4E-4F1A-9C3B-2E5D8A7F6E10}"
#define FREEWB_PROFILE_GUID        L"{C9D4E2B7-5A3F-4B8C-9D1E-6F0A2C4B8D53}"
#define FREEWB_CATEGORY_KEYBOARD   L"{34745C63-B2F0-4784-8B67-5E12C8701A31}"
#define FREEWB_DISPLAYATTR_INPUT   L"{D1E2F3A4-8B6C-4D5E-9F0A-1B2C3D4E5F60}"

#ifndef TF_CLIENTID_NULL
#define TF_CLIENTID_NULL 0
#endif

class FreeIME11Service final
    : public ITfTextInputProcessor,
      public ITfKeyEventSink,
      public ITfThreadMgrEventSink,
      public ITfEditSession,
      public ITfCompositionSink {
public:
    FreeIME11Service();
    virtual ~FreeIME11Service();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr *pThreadMgr, TfClientId tfClientId) override;
    STDMETHODIMP Deactivate() override;

    // ITfKeyEventSink
    STDMETHODIMP OnKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnTestKeyDown(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext *pic, WPARAM wParam, LPARAM lParam, BOOL *pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext *pic, REFGUID rguid, BOOL *pfEaten) override;
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr *pdim) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr *pdim) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr *pdimFocus, ITfDocumentMgr *pdimPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext *pic) override;
    STDMETHODIMP OnPopContext(ITfContext *pic) override;

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) override;

private:
    enum EditSessionType {
        ES_Commit,
        ES_UpdatePreedit,
        ES_Clear,
    };

    BOOL isComposing() const;
    HRESULT requestEdit(EditSessionType type, const std::string &textUtf8, const std::string &preeditUtf8,
                        const std::vector<std::wstring> &cands);
    HRESULT commitText(TfEditCookie ec, const std::string &utf8);
    HRESULT updateComposition(TfEditCookie ec, const std::string &preeditUtf8, const std::vector<std::wstring> &cands);
    HRESULT terminateComposition(TfEditCookie ec);

    LONG refCount_ = 1;
    TfClientId tfClientId_ = TF_CLIENTID_NULL;
    ITfThreadMgr *threadMgr_ = nullptr;
    ITfContext *context_ = nullptr;
    ITfComposition *composition_ = nullptr;
    ITfContextComposition *contextComposition_ = nullptr;
    DWORD cookieKeyEventSink_ = TF_INVALID_COOKIE;
    DWORD cookieThreadMgrEventSink_ = TF_INVALID_COOKIE;

    bool compositionStarted_ = false;

    EditSessionType editType_ = ES_Clear;
    std::string editCommitText_;
    std::string editPreeditText_;
    std::vector<std::wstring> editCands_;

    std::unique_ptr<freeime11::Session> session_;
};
