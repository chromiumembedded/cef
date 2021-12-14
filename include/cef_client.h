// Copyright (c) 2012 Marshall A. Greenblatt. All rights reserved.
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

#ifndef CEF_INCLUDE_CEF_CLIENT_H_
#define CEF_INCLUDE_CEF_CLIENT_H_
#pragma once

#include "include/cef_audio_handler.h"
#include "include/cef_base.h"
#include "include/cef_clipboard_handler.h"
#include "include/cef_context_menu_handler.h"
#include "include/cef_dialog_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_download_handler.h"
#include "include/cef_drag_handler.h"
#include "include/cef_find_handler.h"
#include "include/cef_focus_handler.h"
#include "include/cef_frame_handler.h"
#include "include/cef_jsdialog_handler.h"
#include "include/cef_keyboard_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_print_handler.h"
#include "include/cef_process_message.h"
#include "include/cef_render_handler.h"
#include "include/cef_request_handler.h"

///
// Implement this interface to provide handler implementations.
///
/*--cef(source=client,no_debugct_check)--*/
class CefClient : public virtual CefBaseRefCounted {
 public:
  ///
  // Return the handler for audio rendering events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefAudioHandler> GetAudioHandler() { return nullptr; }

  ///
  // Return the handler for context menus. If no handler is provided the default
  // implementation will be used.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() {
    return nullptr;
  }

  ///
  // Return the handler for dialogs. If no handler is provided the default
  // implementation will be used.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefDialogHandler> GetDialogHandler() { return nullptr; }

  ///
  // Return the handler for browser display state events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() { return nullptr; }

  ///
  // Return the handler for download events. If no handler is returned downloads
  // will not be allowed.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefDownloadHandler> GetDownloadHandler() { return nullptr; }

  ///
  // Return the handler for drag events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefDragHandler> GetDragHandler() { return nullptr; }

  ///
  // Return the handler for find result events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefFindHandler> GetFindHandler() { return nullptr; }

  ///
  // Return the handler for focus events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefFocusHandler> GetFocusHandler() { return nullptr; }

  ///
  // Return the handler for events related to CefFrame lifespan. This method
  // will be called once during CefBrowser creation and the result will be
  // cached for performance reasons.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefFrameHandler> GetFrameHandler() { return nullptr; }

  ///
  // Return the handler for JavaScript dialogs. If no handler is provided the
  // default implementation will be used.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() { return nullptr; }

  ///
  // Return the handler for keyboard events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() { return nullptr; }

  ///
  // Return the handler for browser life span events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() { return nullptr; }

  ///
  // Return the handler for browser load status events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() { return nullptr; }

  ///
  // Return the handler for printing on Linux. If a print handler is not
  // provided then printing will not be supported on the Linux platform.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefPrintHandler> GetPrintHandler() { return nullptr; }

  ///
  // Return the handler for off-screen rendering events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefRenderHandler> GetRenderHandler() { return nullptr; }

  ///
  // Return the handler for browser request events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefRequestHandler> GetRequestHandler() { return nullptr; }

  ///
  // Return the handler for clipboard events.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefClipboardHandler> GetClipboardHandler() {
    return nullptr;
  }

  ///
  // Called when a new message is received from a different process. Return true
  // if the message was handled or false otherwise.  It is safe to keep a
  // reference to |message| outside of this callback.
  ///
  /*--cef()--*/
  virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        CefProcessId source_process,
                                        CefRefPtr<CefProcessMessage> message) {
    return false;
  }
};

#endif  // CEF_INCLUDE_CEF_CLIENT_H_
