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

#ifndef CEF_INCLUDE_CEF_EXTENSION_H_
#define CEF_INCLUDE_CEF_EXTENSION_H_
#pragma once

#include "include/cef_api_hash.h"
#include "include/cef_base.h"
#include "include/cef_image.h"
#include "include/cef_values.h"

class CefBrowser;
class CefRequestContext;

#if CEF_API_ADDED(CEF_NEXT)

///
/// Object representing a Chromium extension that has been loaded into a
/// CefRequestContext. The metadata accessors return a snapshot captured when
/// this object was created and may be called on any thread; to observe changes
/// retrieve a new object via CefRequestContext::GetExtension. The mutating
/// methods (SetEnabled, Uninstall) may be called on any thread; work is
/// marshalled to the browser process UI thread.
///
/*--cef(source=library,added=next)--*/
class CefExtension : public virtual CefBaseRefCounted {
 public:
  ///
  /// Returns the unique extension identifier. This is the value used with the
  /// CefRequestContext extension methods.
  ///
  /*--cef()--*/
  virtual CefString GetIdentifier() = 0;

  ///
  /// Returns the absolute path to the extension directory on disk. This value
  /// will be prefixed with PK_DIR_USER_DATA if a relative path was passed to
  /// CefRequestContext::LoadUnpackedExtension.
  ///
  /*--cef()--*/
  virtual CefString GetPath() = 0;

  ///
  /// Returns the extension name from the manifest, or an empty string if not
  /// available.
  ///
  /*--cef()--*/
  virtual CefString GetName() = 0;

  ///
  /// Returns the extension version from the manifest (e.g. "1.2.3.4"), or an
  /// empty string if not available.
  ///
  /*--cef()--*/
  virtual CefString GetVersion() = 0;

  ///
  /// Returns the parsed manifest contents as a dictionary, or NULL if not
  /// available. The returned object is read-only.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefDictionaryValue> GetManifest() = 0;

  ///
  /// Returns true if this extension is currently enabled. A disabled extension
  /// remains installed but does not run.
  ///
  /*--cef()--*/
  virtual bool IsEnabled() = 0;

  ///
  /// Returns true if this extension should be shown to the user in an
  /// extensions management UI. Returns false for extensions that exist only as
  /// a Chrome implementation detail, such as component extensions (e.g. the Web
  /// Store and the PDF viewer) and themes. This mirrors the visibility policy
  /// used by Chrome's own chrome://extensions page.
  ///
  /*--cef()--*/
  virtual bool ShouldShowInExtensionsUI() = 0;

  ///
  /// Returns the toolbar action icon this extension currently displays for the
  /// active tab of |browser|, or NULL if the extension declares no action. This
  /// reflects per-tab icons the extension sets at runtime via
  /// chrome.action.setIcon() -- for example a framework devtools extension that
  /// swaps its grayed-out icon for a colored one once it detects its framework
  /// on the page -- and otherwise falls back to the manifest's default icon. If
  /// |browser| is NULL the action's default (all-tabs) icon is returned. Must
  /// be called on the browser process UI thread.
  ///
  /*--cef(optional_param=browser)--*/
  virtual CefRefPtr<CefImage> GetActionIcon(CefRefPtr<CefBrowser> browser) = 0;

  ///
  /// Returns true if this object is still valid, meaning the extension is
  /// installed in the owning request context and the context has not been
  /// destroyed. Mutating methods are no-ops once this returns false.
  ///
  /*--cef()--*/
  virtual bool IsValid() = 0;

  ///
  /// Returns true if this object is pointing to the same extension as |that|
  /// object.
  ///
  /*--cef()--*/
  virtual bool IsSame(CefRefPtr<CefExtension> that) = 0;

  ///
  /// Returns the request context that loaded this extension. Returns NULL for
  /// internal extensions or if the context has been destroyed.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefRequestContext> GetRequestContext() = 0;

  ///
  /// Enables or disables this extension. Enabling an extension that is already
  /// enabled (or disabling one already disabled) is a no-op. Enable/disable is
  /// reported to the request context's CefExtensionHandler as OnExtensionLoaded
  /// / OnExtensionUnloaded respectively. May be called on any thread; work is
  /// marshalled to the browser process UI thread.
  ///
  /*--cef()--*/
  virtual void SetEnabled(bool enable) = 0;

  ///
  /// Uninstalls this extension, removing it from the owning request context. On
  /// a persistent profile the extension is also deleted from disk. May be
  /// called on any thread; work is marshalled to the browser process UI thread.
  ///
  /*--cef()--*/
  virtual void Uninstall() = 0;
};

#endif  // CEF_API_ADDED(CEF_NEXT)

#endif  // CEF_INCLUDE_CEF_EXTENSION_H_
