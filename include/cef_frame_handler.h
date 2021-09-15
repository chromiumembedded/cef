// Copyright (c) 2021 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ---------------------------------------------------------------------------
//
// The contents of this file must follow a specific format in order to
// support the CEF translator tool. See the translator.README.txt file in the
// tools directory for more information.
//

#ifndef CEF_INCLUDE_CEF_FRAME_HANDLER_H_
#define CEF_INCLUDE_CEF_FRAME_HANDLER_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"

///
// Implement this interface to handle events related to CefFrame life span. The
// order of callbacks is:
//
// (1) During initial CefBrowserHost creation and navigation of the main frame:
// - CefFrameHandler::OnFrameCreated => The initial main frame object has been
//   created. Any commands will be queued until the frame is attached.
// - CefFrameHandler::OnMainFrameChanged => The initial main frame object has
//   been assigned to the browser.
// - CefLifeSpanHandler::OnAfterCreated => The browser is now valid and can be
//   used.
// - CefFrameHandler::OnFrameAttached => The initial main frame object is now
//   connected to its peer in the renderer process. Commands can be routed.
//
// (2) During further CefBrowserHost navigation/loading of the main frame and/or
// sub-frames:
// - CefFrameHandler::OnFrameCreated => A new main frame or sub-frame object has
//   been created. Any commands will be queued until the frame is attached.
// - CefFrameHandler::OnFrameAttached => A new main frame or sub-frame object is
//   now connected to its peer in the renderer process. Commands can be routed.
// - CefFrameHandler::OnFrameDetached => An existing main frame or sub-frame
//   object has lost its connection to the renderer process. If multiple objects
//   are detached at the same time then notifications will be sent for any
//   sub-frame objects before the main frame object. Commands can no longer be
//   routed and will be discarded.
// - CefFrameHandler::OnMainFrameChanged => A new main frame object has been
//   assigned to the browser. This will only occur with cross-origin navigation
//   or re-navigation after renderer process termination (due to crashes, etc).
//
// (3) During final CefBrowserHost destruction of the main frame:
// - CefFrameHandler::OnFrameDetached => Any sub-frame objects have lost their
//   connection to the renderer process. Commands can no longer be routed and
//   will be discarded.
// - CefLifeSpanHandler::OnBeforeClose => The browser has been destroyed.
// - CefFrameHandler::OnFrameDetached => The main frame object have lost its
//   connection to the renderer process. Notifications will be sent for any
//   sub-frame objects before the main frame object. Commands can no longer be
//   routed and will be discarded.
// - CefFrameHandler::OnMainFrameChanged => The final main frame object has been
//   removed from the browser.
//
// Cross-origin navigation and/or loading receives special handling.
//
// When the main frame navigates to a different origin the OnMainFrameChanged
// callback (2) will be executed with the old and new main frame objects.
//
// When a new sub-frame is loaded in, or an existing sub-frame is navigated to,
// a different origin from the parent frame, a temporary sub-frame object will
// first be created in the parent's renderer process. That temporary sub-frame
// will then be discarded after the real cross-origin sub-frame is created in
// the new/target renderer process. The client will receive cross-origin
// navigation callbacks (2) for the transition from the temporary sub-frame to
// the real sub-frame. The temporary sub-frame will not recieve or execute
// commands during this transitional period (any sent commands will be
// discarded).
//
// When a new popup browser is created in a different origin from the parent
// browser, a temporary main frame object for the popup will first be created in
// the parent's renderer process. That temporary main frame will then be
// discarded after the real cross-origin main frame is created in the new/target
// renderer process. The client will recieve creation and initial navigation
// callbacks (1) for the temporary main frame, followed by cross-origin
// navigation callbacks (2) for the transition from the temporary main frame to
// the real main frame. The temporary main frame may receive and execute
// commands during this transitional period (any sent commands may be executed,
// but the behavior is potentially undesirable since they execute in the parent
// browser's renderer process and not the new/target renderer process).
//
// Callbacks will not be executed for placeholders that may be created during
// pre-commit navigation for sub-frames that do not yet exist in the renderer
// process. Placeholders will have CefFrame::GetIdentifier() == -4.
//
// The methods of this class will be called on the UI thread unless otherwise
// indicated.
///
/*--cef(source=client)--*/
class CefFrameHandler : public virtual CefBaseRefCounted {
 public:
  ///
  // Called when a new frame is created. This will be the first notification
  // that references |frame|. Any commands that require transport to the
  // associated renderer process (LoadRequest, SendProcessMessage, GetSource,
  // etc.) will be queued until OnFrameAttached is called for |frame|.
  ///
  /*--cef()--*/
  virtual void OnFrameCreated(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame) {}

  ///
  // Called when a frame can begin routing commands to/from the associated
  // renderer process. |reattached| will be true if the frame was re-attached
  // after exiting the BackForwardCache. Any commands that were queued have now
  // been dispatched.
  ///
  /*--cef()--*/
  virtual void OnFrameAttached(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               bool reattached) {}

  ///
  // Called when a frame loses its connection to the renderer process and will
  // be destroyed. Any pending or future commands will be discarded and
  // CefFrame::IsValid() will now return false for |frame|. If called after
  // CefLifeSpanHandler::OnBeforeClose() during browser destruction then
  // CefBrowser::IsValid() will return false for |browser|.
  ///
  /*--cef()--*/
  virtual void OnFrameDetached(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame) {}

  ///
  // Called when the main frame changes due to (a) initial browser creation, (b)
  // final browser destruction, (c) cross-origin navigation or (d) re-navigation
  // after renderer process termination (due to crashes, etc). |old_frame| will
  // be NULL and |new_frame| will be non-NULL when a main frame is assigned to
  // |browser| for the first time. |old_frame| will be non-NULL and |new_frame|
  // will be NULL and  when a main frame is removed from |browser| for the last
  // time. Both |old_frame| and |new_frame| will be non-NULL for cross-origin
  // navigations or re-navigation after renderer process termination. This
  // method will be called after OnFrameCreated() for |new_frame| and/or after
  // OnFrameDetached() for |old_frame|. If called after
  // CefLifeSpanHandler::OnBeforeClose() during browser destruction then
  // CefBrowser::IsValid() will return false for |browser|.
  ///
  /*--cef(optional_param=old_frame,optional_param=new_frame)--*/
  virtual void OnMainFrameChanged(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> old_frame,
                                  CefRefPtr<CefFrame> new_frame) {}
};

#endif  // CEF_INCLUDE_CEF_FRAME_HANDLER_H_
