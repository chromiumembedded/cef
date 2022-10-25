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

#ifndef CEF_INCLUDE_CEF_BROWSER_PROCESS_HANDLER_H_
#define CEF_INCLUDE_CEF_BROWSER_PROCESS_HANDLER_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"
#include "include/cef_preference.h"
#include "include/cef_values.h"

///
/// Class used to implement browser process callbacks. The methods of this class
/// will be called on the browser process main thread unless otherwise
/// indicated.
///
/*--cef(source=client,no_debugct_check)--*/
class CefBrowserProcessHandler : public virtual CefBaseRefCounted {
 public:
  ///
  /// Provides an opportunity to register custom preferences prior to
  /// global and request context initialization.
  ///
  /// If |type| is CEF_PREFERENCES_TYPE_GLOBAL the registered preferences can be
  /// accessed via CefPreferenceManager::GetGlobalPreferences after
  /// OnContextInitialized is called. Global preferences are registered a single
  /// time at application startup. See related cef_settings_t.cache_path and
  /// cef_settings_t.persist_user_preferences configuration.
  ///
  /// If |type| is CEF_PREFERENCES_TYPE_REQUEST_CONTEXT the preferences can be
  /// accessed via the CefRequestContext after
  /// CefRequestContextHandler::OnRequestContextInitialized is called. Request
  /// context preferences are registered each time a new CefRequestContext is
  /// created. It is intended but not required that all request contexts have
  /// the same registered preferences. See related
  /// cef_request_context_settings_t.cache_path and
  /// cef_request_context_settings_t.persist_user_preferences configuration.
  ///
  /// Do not keep a reference to the |registrar| object. This method is called
  /// on the browser process UI thread.
  ///
  /*--cef()--*/
  virtual void OnRegisterCustomPreferences(
      cef_preferences_type_t type,
      CefRawPtr<CefPreferenceRegistrar> registrar) {}

  ///
  /// Called on the browser process UI thread immediately after the CEF context
  /// has been initialized.
  ///
  /*--cef()--*/
  virtual void OnContextInitialized() {}

  ///
  /// Called before a child process is launched. Will be called on the browser
  /// process UI thread when launching a render process and on the browser
  /// process IO thread when launching a GPU process. Provides an opportunity to
  /// modify the child process command line. Do not keep a reference to
  /// |command_line| outside of this method.
  ///
  /*--cef()--*/
  virtual void OnBeforeChildProcessLaunch(
      CefRefPtr<CefCommandLine> command_line) {}

  ///
  /// Called from any thread when work has been scheduled for the browser
  /// process main (UI) thread. This callback is used in combination with
  /// cef_settings_t.external_message_pump and CefDoMessageLoopWork() in cases
  /// where the CEF message loop must be integrated into an existing application
  /// message loop (see additional comments and warnings on
  /// CefDoMessageLoopWork). This callback should schedule a
  /// CefDoMessageLoopWork() call to happen on the main (UI) thread. |delay_ms|
  /// is the requested delay in milliseconds. If |delay_ms| is <= 0 then the
  /// call should happen reasonably soon. If |delay_ms| is > 0 then the call
  /// should be scheduled to happen after the specified delay and any currently
  /// pending scheduled call should be cancelled.
  ///
  /*--cef()--*/
  virtual void OnScheduleMessagePumpWork(int64 delay_ms) {}

  ///
  /// Return the default client for use with a newly created browser window. If
  /// null is returned the browser will be unmanaged (no callbacks will be
  /// executed for that browser) and application shutdown will be blocked until
  /// the browser window is closed manually. This method is currently only used
  /// with the chrome runtime.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefClient> GetDefaultClient() { return nullptr; }
};

#endif  // CEF_INCLUDE_CEF_BROWSER_PROCESS_HANDLER_H_
