// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CONTEXT_MENU_PARAMS_IMPL_H_
#define CEF_LIBCEF_BROWSER_CONTEXT_MENU_PARAMS_IMPL_H_
#pragma once

#include "include/cef_context_menu_handler.h"
#include "libcef/common/value_base.h"

#include "content/public/common/context_menu_params.h"

// CefContextMenuParams implementation. This class is not thread safe.
class CefContextMenuParamsImpl
    : public CefValueBase<CefContextMenuParams, content::ContextMenuParams> {
 public:
  explicit CefContextMenuParamsImpl(content::ContextMenuParams* value);

  // CefContextMenuParams methods.
  int GetXCoord() override;
  int GetYCoord() override;
  TypeFlags GetTypeFlags() override;
  CefString GetLinkUrl() override;
  CefString GetUnfilteredLinkUrl() override;
  CefString GetSourceUrl() override;
  bool HasImageContents() override;
  CefString GetTitleText() override;
  CefString GetPageUrl() override;
  CefString GetFrameUrl() override;
  CefString GetFrameCharset() override;
  MediaType GetMediaType() override;
  MediaStateFlags GetMediaStateFlags() override;
  CefString GetSelectionText() override;
  CefString GetMisspelledWord() override;
  bool GetDictionarySuggestions(
      std::vector<CefString>& suggestions) override;
  bool IsEditable() override;
  bool IsSpellCheckEnabled() override;
  EditStateFlags GetEditStateFlags() override;
  bool IsCustomMenu() override;
  bool IsPepperMenu() override;

  DISALLOW_COPY_AND_ASSIGN(CefContextMenuParamsImpl);
};

#endif  // CEF_LIBCEF_BROWSER_CONTEXT_MENU_PARAMS_IMPL_H_
