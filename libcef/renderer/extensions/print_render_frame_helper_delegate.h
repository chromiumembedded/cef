// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_EXTENSIONS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
#define CEF_LIBCEF_RENDERER_EXTENSIONS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_

#include "components/printing/renderer/print_render_frame_helper.h"

namespace extensions {

class CefPrintRenderFrameHelperDelegate
    : public printing::PrintRenderFrameHelper::Delegate {
 public:
  explicit CefPrintRenderFrameHelperDelegate(bool is_windowless);
  ~CefPrintRenderFrameHelperDelegate() override;

  blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) override;
  bool IsPrintPreviewEnabled() override;
  bool ShouldGenerateTaggedPDF() override;
  bool OverridePrint(blink::WebLocalFrame* frame) override;

 private:
  bool is_windowless_;
};

}  // namespace extensions

#endif  // CEF_LIBCEF_RENDERER_EXTENSIONS_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
