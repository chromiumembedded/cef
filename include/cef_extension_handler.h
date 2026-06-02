// Copyright (c) 2026 Marshall A. Greenblatt. All rights reserved.
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

#ifndef CEF_INCLUDE_CEF_EXTENSION_HANDLER_H_
#define CEF_INCLUDE_CEF_EXTENSION_HANDLER_H_
#pragma once

#include "include/cef_api_hash.h"
#include "include/cef_base.h"
#include "include/cef_browser.h"
#include "include/cef_extension.h"

#if CEF_API_ADDED(CEF_NEXT)

///
/// Implemented by the client to observe the lifecycle of extensions loaded into
/// a CefRequestContext. Register via CefRequestContext::SetExtensionsHandler
/// for global notifications, or supply per-extension handlers to
/// LoadUnpackedExtension / InstallExtension / InstallUnpackedExtension. The
/// methods of this class
/// will be called on the browser process UI thread.
///
/*--cef(source=client,added=next)--*/
class CefExtensionHandler : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called when an extension has been loaded into the request context and is
  /// now enabled. For a context-wide handler this is also delivered for each
  /// extension already loaded at the time SetExtensionsHandler was called.
  ///
  /*--cef()--*/
  virtual void OnExtensionLoaded(CefRefPtr<CefExtension> extension) {}

  ///
  /// Called when an extension is unloaded (disabled or uninstalled). Will be
  /// balanced with a prior OnExtensionLoaded for the same extension id.
  ///
  /*--cef()--*/
  virtual void OnExtensionUnloaded(CefRefPtr<CefExtension> extension) {}

  ///
  /// Called when an extension load/install attempt fails. |error_code| is the
  /// underlying CEF error code (currently ERR_FAILED for all failures).
  ///
  /*--cef()--*/
  virtual void OnExtensionLoadFailed(cef_errorcode_t error_code) {}

  ///
  /// Called when an extension updates its toolbar action (icon, title, badge
  /// text, etc.) via chrome.action.* APIs. |browser| is the affected tab's
  /// browser, or NULL for a change that applies to all tabs (e.g. a default-
  /// icon change).
  ///
  /*--cef(optional_param=browser)--*/
  virtual void OnExtensionActionUpdated(CefRefPtr<CefExtension> extension,
                                        CefRefPtr<CefBrowser> browser) {}
};

#endif  // CEF_API_ADDED(CEF_NEXT)

#endif  // CEF_INCLUDE_CEF_EXTENSION_HANDLER_H_
