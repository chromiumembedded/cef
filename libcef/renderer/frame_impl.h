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
class WebURLLoader;
class WebURLLoaderFactory;
}  // namespace blink

class GURL;

class CefBrowserImpl;

// Implementation of CefFrame. CefFrameImpl objects are owned by the
// CefBrowerImpl and will be detached when the browser is notified that the
// associated renderer WebFrame will close.
class CefFrameImpl : public CefFrame, public cef::mojom::RenderFrame {
 public:
  CefFrameImpl(CefBrowserImpl* browser,
               blink::WebLocalFrame* frame,
               int64_t frame_id);

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
  int64 GetIdentifier() override;
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

  // Used by CefRenderURLRequest.
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader();
  std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
  CreateResourceLoadInfoNotifierWrapper();

  // Forwarded from CefRenderFrameObserver.
  void OnAttached();
  void OnWasShown();
  void OnDidFinishLoad();
  void OnDraggableRegionsChanged();
  void OnContextCreated();
  void OnDetached();

  blink::WebLocalFrame* web_frame() const { return frame_; }

 private:
  // Execute an action on the associated WebLocalFrame. This will queue the
  // action if the JavaScript context is not yet created.
  using LocalFrameAction =
      base::OnceCallback<void(blink::WebLocalFrame* frame)>;
  void ExecuteOnLocalFrame(const std::string& function_name,
                           LocalFrameAction action);

  // Initiate the connection to the BrowserFrame channel.
  void ConnectBrowserFrame();

  // Returns the remote BrowserFrame object.
  using BrowserFrameType = mojo::Remote<cef::mojom::BrowserFrame>;
  const BrowserFrameType& GetBrowserFrame(bool expect_acked = true);

  // Called if the BrowserFrame connection attempt times out.
  void OnBrowserFrameTimeout();

  // Called if/when the BrowserFrame channel is disconnected. This may occur due
  // to frame navigation, destruction, or insertion into the bfcache (when the
  // browser-side frame representation is destroyed and closes the connection).
  void OnBrowserFrameDisconnect();

  // Send an action to the remote BrowserFrame. This will queue the action if
  // the remote frame is not yet attached.
  using BrowserFrameAction = base::OnceCallback<void(const BrowserFrameType&)>;
  void SendToBrowserFrame(const std::string& function_name,
                          BrowserFrameAction action);

  // cef::mojom::RenderFrame methods:
  void FrameAttachedAck() override;
  void SendMessage(const std::string& name, base::Value arguments) override;
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

  CefBrowserImpl* browser_;
  blink::WebLocalFrame* frame_;
  const int64 frame_id_;

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

  base::OneShotTimer browser_connect_timer_;

  std::queue<std::pair<std::string, BrowserFrameAction>>
      queued_browser_actions_;

  std::unique_ptr<blink::WebURLLoaderFactory> url_loader_factory_;

  mojo::Receiver<cef::mojom::RenderFrame> receiver_{this};

  mojo::Remote<cef::mojom::BrowserFrame> browser_frame_;

  base::WeakPtrFactory<CefFrameImpl> weak_ptr_factory_{this};

  IMPLEMENT_REFCOUNTING(CefFrameImpl);
};

#endif  // CEF_LIBCEF_RENDERER_FRAME_IMPL_H_
