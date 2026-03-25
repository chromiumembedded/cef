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

#ifndef CEF_INCLUDE_CEF_STORAGE_STATE_H_
#define CEF_INCLUDE_CEF_STORAGE_STATE_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_callback.h"
#include "include/cef_values.h"

class CefBrowser;

///
/// Callback interface for CefStorageStateManager state-changing operations.
/// Methods will be executed on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefStorageStateActionCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called after the operation completes. |success| indicates if the
  /// operation succeeded. |error| contains a failure description when
  /// |success| is false. |path| contains the affected state path when
  /// available.
  ///
  /*--cef(optional_param=error,optional_param=path)--*/
  virtual void OnComplete(bool success,
                          const CefString& error,
                          const CefString& path) = 0;
};

///
/// Callback interface for CefStorageStateManager read operations. Methods will
/// be executed on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefStorageStateReadCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called after the operation completes. |success| indicates if the
  /// operation succeeded. |error| contains a failure description when
  /// |success| is false. |result| contains operation-specific metadata when
  /// available.
  ///
  /*--cef(optional_param=error,optional_param=result)--*/
  virtual void OnComplete(bool success,
                          const CefString& error,
                          CefRefPtr<CefDictionaryValue> result) = 0;
};

///
/// Callback interface for CefStorageStateManager::List. Methods will be
/// executed on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefStorageStateListCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called after the operation completes. |success| indicates if the
  /// operation succeeded. |error| contains a failure description when
  /// |success| is false. |entries| contains a list of state metadata
  /// dictionaries and |directory| identifies the storage directory that was
  /// scanned.
  ///
  /*--cef(optional_param=error,optional_param=entries,optional_param=directory)--*/
  virtual void OnComplete(bool success,
                          const CefString& error,
                          CefRefPtr<CefListValue> entries,
                          const CefString& directory) = 0;
};

///
/// Manager for persisted browser storage state associated with a request
/// context. The methods of this class may be called on any browser process
/// thread unless otherwise indicated.
///
/*--cef(source=library)--*/
class CefStorageStateManager : public virtual CefBaseRefCounted {
 public:
  ///
  /// Save storage state for the associated request context. When |path| is
  /// empty the default path derived from GetDefaultStatePath() will be used.
  /// |browser| can be used to scope export to a specific browser instance for
  /// future implementations that need a live page context. |callback| will be
  /// executed on completion.
  ///
  /*--cef(optional_param=path,optional_param=browser,optional_param=callback)--*/
  virtual void Save(const CefString& path,
                    CefRefPtr<CefBrowser> browser,
                    CefRefPtr<CefStorageStateActionCallback> callback) = 0;

  ///
  /// Load storage state for the associated request context from |path|. When
  /// |browser| is specified it may be used to apply page-scoped storage in
  /// future implementations. |callback| will be executed on completion.
  ///
  /*--cef(optional_param=browser,optional_param=callback)--*/
  virtual void Load(const CefString& path,
                    CefRefPtr<CefBrowser> browser,
                    CefRefPtr<CefStorageStateActionCallback> callback) = 0;

  ///
  /// List saved state files in the default state directory. |callback| will be
  /// executed on completion.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void List(CefRefPtr<CefStorageStateListCallback> callback) = 0;

  ///
  /// Read metadata for the state file at |path|. |callback| will be executed
  /// on completion.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void Show(const CefString& path,
                    CefRefPtr<CefStorageStateReadCallback> callback) = 0;

  ///
  /// Rename the state file at |path| using |new_name| as the file stem.
  /// |callback| will be executed on completion.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void Rename(const CefString& path,
                      const CefString& new_name,
                      CefRefPtr<CefStorageStateActionCallback> callback) = 0;

  ///
  /// Clear the state file at |path|. When |path| is empty all default state
  /// files will be removed. |callback| will be executed on completion.
  ///
  /*--cef(optional_param=path,optional_param=callback)--*/
  virtual void Clear(const CefString& path,
                     CefRefPtr<CefStorageStateActionCallback> callback) = 0;

  ///
  /// Remove saved state files older than |days| from the default state
  /// directory. |callback| will be executed on completion with an operation
  /// summary.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void CleanOlderThan(
      int days,
      CefRefPtr<CefStorageStateReadCallback> callback) = 0;

  ///
  /// Set the automatic state persistence session name. An empty value disables
  /// automatic path naming.
  ///
  /*--cef(optional_param=session_name)--*/
  virtual void SetSessionName(const CefString& session_name) = 0;

  ///
  /// Returns the automatic state persistence session name, or empty if unset.
  /// Must be called on the browser process UI thread.
  ///
  /*--cef()--*/
  virtual CefString GetSessionName() = 0;

  ///
  /// Returns the default directory used for persisted state files. Must be
  /// called on the browser process UI thread.
  ///
  /*--cef()--*/
  virtual CefString GetDefaultStateDirectory() = 0;

  ///
  /// Returns the default state file path derived from the current session name.
  /// Must be called on the browser process UI thread.
  ///
  /*--cef()--*/
  virtual CefString GetDefaultStatePath() = 0;
};

#endif  // CEF_INCLUDE_CEF_STORAGE_STATE_H_
