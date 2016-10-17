// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/frame_host_impl.h"
#include "include/cef_request.h"
#include "include/cef_stream.h"
#include "include/cef_v8.h"
#include "libcef/common/cef_messages.h"
#include "libcef/browser/browser_host_impl.h"

namespace {

// Implementation of CommandResponseHandler for calling a CefStringVisitor.
class StringVisitHandler : public CefResponseManager::Handler {
 public:
  explicit StringVisitHandler(CefRefPtr<CefStringVisitor> visitor)
      : visitor_(visitor) {
  }
  void OnResponse(const Cef_Response_Params& params) override {
    visitor_->Visit(params.response);
  }
 private:
  CefRefPtr<CefStringVisitor> visitor_;

  IMPLEMENT_REFCOUNTING(StringVisitHandler);
};

// Implementation of CommandResponseHandler for calling ViewText().
class ViewTextHandler : public CefResponseManager::Handler {
 public:
  explicit ViewTextHandler(CefRefPtr<CefFrameHostImpl> frame)
      : frame_(frame) {
  }
  void OnResponse(const Cef_Response_Params& params) override {
    CefRefPtr<CefBrowser> browser = frame_->GetBrowser();
    if (browser.get()) {
      static_cast<CefBrowserHostImpl*>(browser.get())->ViewText(
          params.response);
    }
  }
 private:
  CefRefPtr<CefFrameHostImpl> frame_;

  IMPLEMENT_REFCOUNTING(ViewTextHandler);
};

}  // namespace

CefFrameHostImpl::CefFrameHostImpl(CefBrowserHostImpl* browser,
                                   int64 frame_id,
                                   bool is_main_frame,
                                   const CefString& url,
                                   const CefString& name,
                                   int64 parent_frame_id)
    : frame_id_(frame_id),
      is_main_frame_(is_main_frame),
      browser_(browser),
      is_focused_(is_main_frame_),  // The main frame always starts focused.
      url_(url),
      name_(name),
      parent_frame_id_(parent_frame_id == kUnspecifiedFrameId ?
                       kInvalidFrameId : parent_frame_id) {
}

CefFrameHostImpl::~CefFrameHostImpl() {
}

bool CefFrameHostImpl::IsValid() {
  base::AutoLock lock_scope(state_lock_);
  return (browser_ != NULL);
}

void CefFrameHostImpl::Undo() {
  SendCommand("Undo", NULL);
}

void CefFrameHostImpl::Redo() {
  SendCommand("Redo", NULL);
}

void CefFrameHostImpl::Cut() {
  SendCommand("Cut", NULL);
}

void CefFrameHostImpl::Copy() {
  SendCommand("Copy", NULL);
}

void CefFrameHostImpl::Paste() {
  SendCommand("Paste", NULL);
}

void CefFrameHostImpl::Delete() {
  SendCommand("Delete", NULL);
}

void CefFrameHostImpl::SelectAll() {
  SendCommand("SelectAll", NULL);
}

void CefFrameHostImpl::ViewSource() {
  SendCommand("GetSource", new ViewTextHandler(this));
}

void CefFrameHostImpl::GetSource(CefRefPtr<CefStringVisitor> visitor) {
  SendCommand("GetSource", new StringVisitHandler(visitor));
}

void CefFrameHostImpl::GetText(CefRefPtr<CefStringVisitor> visitor) {
  SendCommand("GetText", new StringVisitHandler(visitor));
}

void CefFrameHostImpl::LoadRequest(CefRefPtr<CefRequest> request) {
  CefRefPtr<CefBrowserHostImpl> browser;
  int64 frame_id;

  {
    base::AutoLock lock_scope(state_lock_);
    browser = browser_;
    frame_id = (is_main_frame_ ? kMainFrameId : frame_id_);
  }

  if (browser.get() && frame_id != kInvalidFrameId)
    browser->LoadRequest(frame_id, request);
}

void CefFrameHostImpl::LoadURL(const CefString& url) {
  CefRefPtr<CefBrowserHostImpl> browser;
  int64 frame_id;

  {
    base::AutoLock lock_scope(state_lock_);
    browser = browser_;
    frame_id = (is_main_frame_ ? kMainFrameId : frame_id_);
  }

  if (browser.get() && frame_id != kInvalidFrameId) {
    browser->LoadURL(frame_id, url, content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  }
}

void CefFrameHostImpl::LoadString(const CefString& string,
                                  const CefString& url) {
  CefRefPtr<CefBrowserHostImpl> browser;
  int64 frame_id;

  {
    base::AutoLock lock_scope(state_lock_);
    browser = browser_;
    frame_id = (is_main_frame_ ? kMainFrameId : frame_id_);
  }

  if (browser.get() && frame_id != kInvalidFrameId)
    browser->LoadString(frame_id, string, url);
}

void CefFrameHostImpl::ExecuteJavaScript(const CefString& jsCode,
                                         const CefString& scriptUrl,
                                         int startLine) {
  SendJavaScript(jsCode, scriptUrl, startLine);
}

bool CefFrameHostImpl::IsMain() {
  return is_main_frame_;
}

bool CefFrameHostImpl::IsFocused() {
  base::AutoLock lock_scope(state_lock_);
  return is_focused_;
}

CefString CefFrameHostImpl::GetName() {
  base::AutoLock lock_scope(state_lock_);
  return name_;
}

int64 CefFrameHostImpl::GetIdentifier() {
  return frame_id_;
}

CefRefPtr<CefFrame> CefFrameHostImpl::GetParent() {
  CefRefPtr<CefBrowserHostImpl> browser;
  int64 parent_frame_id;

  {
    base::AutoLock lock_scope(state_lock_);
    if (is_main_frame_ || parent_frame_id_ == kInvalidFrameId)
      return NULL;
    browser = browser_;
    parent_frame_id = parent_frame_id_;
  }

  if (browser.get())
    return browser->GetFrame(parent_frame_id);

  return NULL;
}

CefString CefFrameHostImpl::GetURL() {
  base::AutoLock lock_scope(state_lock_);
  return url_;
}

CefRefPtr<CefBrowser> CefFrameHostImpl::GetBrowser() {
  base::AutoLock lock_scope(state_lock_);
  return browser_;
}

void CefFrameHostImpl::SetFocused(bool focused) {
  base::AutoLock lock_scope(state_lock_);
  is_focused_ = focused;
}

void CefFrameHostImpl::SetAttributes(const CefString& url,
                                     const CefString& name,
                                     int64 parent_frame_id) {
  base::AutoLock lock_scope(state_lock_);
  if (!url.empty() && url != url_)
    url_ = url;
  if (!name.empty() && name != name_)
    name_ = name;
  if (parent_frame_id != kUnspecifiedFrameId)
    parent_frame_id_ = parent_frame_id;
}

CefRefPtr<CefV8Context> CefFrameHostImpl::GetV8Context() {
  NOTREACHED() << "GetV8Context cannot be called from the browser process";
  return NULL;
}

void CefFrameHostImpl::VisitDOM(CefRefPtr<CefDOMVisitor> visitor) {
  NOTREACHED() << "VisitDOM cannot be called from the browser process";
}

void CefFrameHostImpl::SendJavaScript(
    const std::string& jsCode,
    const std::string& scriptUrl,
    int startLine) {
  if (jsCode.empty())
    return;
  if (startLine <= 0) {
    // A value of 0 is v8::Message::kNoLineNumberInfo in V8. There is code in
    // V8 that will assert on that value (e.g. V8StackTraceImpl::Frame::Frame
    // if a JS exception is thrown) so make sure |startLine| > 0.
    startLine = 1;
  }

  CefRefPtr<CefBrowserHostImpl> browser;
  int64 frame_id;

  {
    base::AutoLock lock_scope(state_lock_);
    browser = browser_;
    frame_id = (is_main_frame_ ? kMainFrameId : frame_id_);
  }

  if (browser.get() && frame_id != kInvalidFrameId)
    browser->SendCode(frame_id, true, jsCode, scriptUrl, startLine, NULL);
}

void CefFrameHostImpl::Detach() {
  base::AutoLock lock_scope(state_lock_);
  browser_ = NULL;
}

void CefFrameHostImpl::SendCommand(
    const std::string& command,
    CefRefPtr<CefResponseManager::Handler> responseHandler) {
  CefRefPtr<CefBrowserHostImpl> browser;
  int64 frame_id;

  {
    base::AutoLock lock_scope(state_lock_);
    browser = browser_;
    // Commands can only be sent to known frame ids.
    frame_id = frame_id_;
  }

  if (browser.get() && frame_id != kInvalidFrameId)
    browser->SendCommand(frame_id, command, responseHandler);
}
