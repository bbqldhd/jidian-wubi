// SPDX-License-Identifier: MulanPSL-2.0
// Copyright (c) 2026 Charry
// Freeime11 is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the Mulan PSL v2.
// You may obtain a copy of Mulan PSL v2 at: http://license.coscl.org.cn/MulanPSL2
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
// EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
// MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
// See the Mulan PSL v2 for more details.

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
#include "ToolbarWindow.h"

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

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition) override;

private:
    enum EditSessionType {
        ES_Commit,
        ES_UpdatePreedit,
        ES_Clear,
    };

    // 每次编辑请求都建一个独立的编辑会话对象，把数据快照带进去。
    // 旧实现把 this 同时当 ITfEditSession 用：context 里排队的旧会话执行时读到的是
    // 后来覆盖的字段，可能重复上屏、也可能把已经清掉的编码重新写回文档（残留）。
    class EditRequest final : public ITfEditSession {
    public:
        EditRequest(FreeIME11Service *owner, EditSessionType type, std::string commitUtf8,
                    std::string preeditUtf8, std::vector<std::wstring> cands, int cursor, bool hasPrev,
                    bool hasNext, std::string aux, ITfComposition *target = nullptr);
        ~EditRequest();

        STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj) override;
        STDMETHODIMP_(ULONG) AddRef() override;
        STDMETHODIMP_(ULONG) Release() override;
        STDMETHODIMP DoEditSession(TfEditCookie ec) override;

    private:
        LONG ref_ = 1;
        FreeIME11Service *owner_ = nullptr;
        EditSessionType type_ = ES_Clear;
        std::string commitUtf8_;
        std::string preeditUtf8_;
        std::vector<std::wstring> cands_;
        int cursor_ = 0;
        bool hasPrev_ = false;
        bool hasNext_ = false;
        std::string aux_;
        // ES_Clear 专用：发起请求时的组合对象。异步执行时全局状态可能已经变了，
        // 带上引用才能保证一定清掉当初那串编码。
        ITfComposition *target_ = nullptr;
    };

    BOOL isComposing() const;
    // 把会话结果落到编辑会话（按键与鼠标点选共用）
    void dispatchOutcome(const freeime11::SessionOutcome &out);
    // 候选窗鼠标回调
    void onCandidateSelected(int index);
    void onCandidatePage(int delta);
    // 工具栏按钮回调
    void onToolbarAction(ToolbarAction action);
    // 把会话状态推给工具栏（中/英、全/半、标点、简繁、方案）
    void syncToolbar();
    // 打开设置程序（与 DLL 同目录的 freeime11-settings.exe）
    void launchSettings() const;
    // 工具栏只在“有可用编辑上下文”时显示，避免每个线程各弹一条
    void updateToolbarVisibility();
    // Shift 单击（其间没按别的键）切中英
    void toggleEnglishFromShortcut();
    HRESULT requestEdit(EditSessionType type, const std::string &textUtf8, const std::string &preeditUtf8,
                        const std::vector<std::wstring> &cands, int cursor = 0, bool hasPrev = false,
                        bool hasNext = false, const std::string &aux = std::string());
    void applyEdit(TfEditCookie ec, EditSessionType type, const std::string &commitUtf8,
                   const std::string &preeditUtf8, const std::vector<std::wstring> &cands, int cursor, bool hasPrev,
                   bool hasNext, const std::string &aux, ITfComposition *target);
    // 清空并结束指定组合（不改变引用计数）；target 为空时用当前 composition_
    void closeComposition(TfEditCookie ec, ITfComposition *target);
    HRESULT commitText(TfEditCookie ec, const std::string &utf8);
    HRESULT updateComposition(TfEditCookie ec, const std::string &preeditUtf8, const std::vector<std::wstring> &cands,
                              int cursor, bool hasPrev, bool hasNext, const std::string &aux);
    HRESULT terminateComposition(TfEditCookie ec);
    // 切换/接管上下文：始终以实际收到按键的 context 为准
    void adoptContext(ITfContext *pic);
    // 在指定上下文里同步清掉残留组合（焦点切换前调用；同步失败退化异步）
    void clearCompositionOn(ITfContext *ctx);
    // 取插入符位置（屏幕坐标，左下方）；失败返回 false
    bool caretScreenPos(TfEditCookie ec, POINT &pt) const;
    // 取插入符所在范围（调用方负责 Release）；失败返回 false
    bool selectionRange(TfEditCookie ec, ITfRange **ppRange) const;

    LONG refCount_ = 1;
    TfClientId tfClientId_ = TF_CLIENTID_NULL;
    ITfThreadMgr *threadMgr_ = nullptr;
    ITfContext *context_ = nullptr;
    ITfComposition *composition_ = nullptr;
    ITfContextComposition *contextComposition_ = nullptr;
    DWORD cookieKeyEventSink_ = TF_INVALID_COOKIE;
    DWORD cookieThreadMgrEventSink_ = TF_INVALID_COOKIE;

    bool compositionStarted_ = false;
    // 该上下文不支持 TSF 组合（终端、部分 Electron 应用）：降级为只出候选窗、直接上屏
    bool compositionUnsupported_ = false;

    // 用户点了工具栏的「×」：本次激活内不再自动弹出
    bool toolbarDismissed_ = false;

    // Shift 按下后是否还没按过别的键（决定松开时算不算一次“单击”）
    bool shiftTapPending_ = false;

    std::unique_ptr<freeime11::Session> session_;
};
