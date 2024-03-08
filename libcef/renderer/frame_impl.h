// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_FRAME_IMPL_H_
#define CEF_LIBCEF_RENDERER_FRAME_IMPL_H_
#pragma once

#include <queue>
#include <string>

#include "include/cef_frame.h"
#include "include/cef_v8.h"
#include "libcef/renderer/blink_glue.h"

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "cef/libcef/common/mojom/cef.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class ListValue;
}

namespace blink {
class ResourceLoadInfoNotifierWrapper;
class WebLocalFrame;
}  // namespace blink

class GURL;

class CefBrowserImpl;

// Implementation of CefFrame. CefFrameImpl objects are owned by the
// CefBrowerImpl and will be detached when the browser is notified that the
// associated renderer WebFrame will close.
class CefFrameImpl
    : public CefFrame,
      public cef::mojom::RenderFrame,
      public blink_glue::CefExecutionContextLifecycleStateObserver {
 public:
  CefFrameImpl(CefBrowserImpl* browser, blink::WebLocalFrame* frame);

  CefFrameImpl(const CefFrameImpl&) = delete;
  CefFrameImpl& operator=(const CefFrameImpl&) = delete;

  ~CefFrameImpl() override;

  // CefFrame implementation.
  bool IsValid() override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void Paste() override;
  void Delete() override;
  void SelectAll() override;
  void ViewSource() override;
  void GetSource(CefRefPtr<CefStringVisitor> visitor) override;
  void GetText(CefRefPtr<CefStringVisitor> visitor) override;
  void LoadRequest(CefRefPtr<CefRequest> request) override;
  void LoadURL(const CefString& url) override;
  void ExecuteJavaScript(const CefString& jsCode,
                         const CefString& scriptUrl,
                         int startLine) override;
  bool IsMain() override;
  bool IsFocused() override;
  CefString GetName() override;
  CefString GetIdentifier() override;
  CefRefPtr<CefFrame> GetParent() override;
  CefString GetURL() override;
  CefRefPtr<CefBrowser> GetBrowser() override;
  CefRefPtr<CefV8Context> GetV8Context() override;
  void VisitDOM(CefRefPtr<CefDOMVisitor> visitor) override;
  CefRefPtr<CefURLRequest> CreateURLRequest(
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefURLRequestClient> client) override;
  void SendProcessMessage(CefProcessId target_process,
                          CefRefPtr<CefProcessMessage> message) override;

  // Forwarded from CefRenderFrameObserver.
  void OnAttached();
  void OnWasShown();
  void OnDidCommitProvisionalLoad();
  void OnDidFinishLoad();
  void OnDraggableRegionsChanged();
  void OnContextCreated(v8::Local<v8::Context> context);
  void OnContextReleased();
  void OnDetached();

  blink::WebLocalFrame* web_frame() const { return frame_; }

 private:
  // Execute an action on the associated WebLocalFrame. This will queue the
  // action if the JavaScript context is not yet created.
  using LocalFrameAction =
      base::OnceCallback<void(blink::WebLocalFrame* frame)>;
  void ExecuteOnLocalFrame(const std::string& function_name,
                           LocalFrameAction action);

  enum class ConnectReason {
    RENDER_FRAME_CREATED,
    WAS_SHOWN,
    RETRY,
  };

  // Initiate the connection to the BrowserFrame channel.
  void ConnectBrowserFrame(ConnectReason reason);

  // Returns the remote BrowserFrame object.
  using BrowserFrameType = mojo::Remote<cef::mojom::BrowserFrame>;
  const BrowserFrameType& GetBrowserFrame(bool expect_acked = true);

  // Called if the BrowserFrame connection attempt times out.
  void OnBrowserFrameTimeout();

  // Called if the BrowserFrame connection is disconnected.
  void OnBrowserFrameDisconnect(uint32_t custom_reason,
                                const std::string& description);
  // Called if the RenderFrame connection is disconnected.
  void OnRenderFrameDisconnect(uint32_t custom_reason,
                               const std::string& description);

  enum class DisconnectReason {
    DETACHED,
    BROWSER_FRAME_DETACHED,
    CONNECT_TIMEOUT,
    RENDER_FRAME_DISCONNECT,
    BROWSER_FRAME_DISCONNECT,
  };

  // Called if/when a disconnect occurs. This may occur due to frame navigation,
  // destruction, or insertion into the bfcache (when the browser-side frame
  // representation is destroyed and closes the connection).
  void OnDisconnect(DisconnectReason reason, const std::string& description);

  // Send an action to the remote BrowserFrame. This will queue the action if
  // the remote frame is not yet attached.
  using BrowserFrameAction = base::OnceCallback<void(const BrowserFrameType&)>;
  void SendToBrowserFrame(const std::string& function_name,
                          BrowserFrameAction action);

  void MaybeInitializeScriptContext();

  // cef::mojom::RenderFrame methods:
  void FrameAttachedAck() override;
  void FrameDetached() override;
  void SendMessage(const std::string& name,
                   base::Value::List arguments) override;
  void SendSharedMemoryRegion(const std::string& name,
                              base::WritableSharedMemoryRegion region) override;
  void SendCommand(const std::string& command) override;
  void SendCommandWithResponse(
      const std::string& command,
      cef::mojom::RenderFrame::SendCommandWithResponseCallback callback)
      override;
  void SendJavaScript(const std::u16string& jsCode,
                      const std::string& scriptUrl,
                      int32_t startLine) override;
  void LoadRequest(cef::mojom::RequestParamsPtr params) override;
  void DidStopLoading() override;
  void MoveOrResizeStarted() override;

  // blink_glue::CefExecutionContextLifecycleStateObserver methods:
  void ContextLifecycleStateChanged(
      blink::mojom::blink::FrameLifecycleState state) override;

  CefBrowserImpl* browser_;
  blink::WebLocalFrame* frame_;
  const std::string frame_debug_str_;

  bool did_commit_provisional_load_ = false;
  bool did_initialize_script_context_ = false;

  bool context_created_ = false;
  std::queue<std::pair<std::string, LocalFrameAction>> queued_context_actions_;

  // Number of times that browser reconnect has been attempted.
  size_t browser_connect_retry_ct_ = 0;

  // Current browser connection state.
  enum class ConnectionState {
    DISCONNECTED,
    CONNECTION_PENDING,
    CONNECTION_ACKED,
    RECONNECT_PENDING,
  } browser_connection_state_ = ConnectionState::DISCONNECTED;

  static std::string GetDisconnectDebugString(ConnectionState connection_state,
                                              bool frame_is_valid,
                                              DisconnectReason reason,
                                              const std::string& description);

  base::OneShotTimer browser_connect_timer_;

  std::queue<std::pair<std::string, BrowserFrameAction>>
      queued_browser_actions_;

  mojo::Receiver<cef::mojom::RenderFrame> receiver_{this};

  mojo::Remote<cef::mojom::BrowserFrame> browser_frame_;

  std::unique_ptr<blink_glue::CefObserverRegistration>
      execution_context_lifecycle_state_observer_;

  base::WeakPtrFactory<CefFrameImpl> weak_ptr_factory_{this};

  IMPLEMENT_REFCOUNTING(CefFrameImpl);
};

#endif  // CEF_LIBCEF_RENDERER_FRAME_IMPL_H_
