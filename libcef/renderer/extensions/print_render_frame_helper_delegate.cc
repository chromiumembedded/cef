// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/extensions/print_render_frame_helper_delegate.h"

#include <vector>

#include "libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/constants.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

CefPrintRenderFrameHelperDelegate::CefPrintRenderFrameHelperDelegate(
    bool is_windowless)
    : is_windowless_(is_windowless) {}

CefPrintRenderFrameHelperDelegate::~CefPrintRenderFrameHelperDelegate() {}

// Return the PDF object element if |frame| is the out of process PDF extension.
blink::WebElement CefPrintRenderFrameHelperDelegate::GetPdfElement(
    blink::WebLocalFrame* frame) {
  GURL url = frame->GetDocument().Url();
  bool inside_print_preview = url.GetOrigin() == chrome::kChromeUIPrintURL;
  bool inside_pdf_extension =
      url.SchemeIs(extensions::kExtensionScheme) &&
      url.host_piece() == extension_misc::kPdfExtensionId;
  if (inside_print_preview || inside_pdf_extension) {
    // <object> with id="plugin" is created in
    // chrome/browser/resources/pdf/pdf.js.
    auto plugin_element = frame->GetDocument().GetElementById("plugin");
    if (!plugin_element.IsNull()) {
      return plugin_element;
    }
    NOTREACHED();
  }
  return blink::WebElement();
}

bool CefPrintRenderFrameHelperDelegate::IsPrintPreviewEnabled() {
  return !is_windowless_ && PrintPreviewEnabled();
}

bool CefPrintRenderFrameHelperDelegate::OverridePrint(
    blink::WebLocalFrame* frame) {
  if (!frame->GetDocument().IsPluginDocument())
    return false;

  auto* post_message_support =
      extensions::PostMessageSupport::FromWebLocalFrame(frame);
  if (post_message_support) {
    // This message is handled in chrome/browser/resources/pdf/pdf.js and
    // instructs the PDF plugin to print. This is to make window.print() on a
    // PDF plugin document correctly print the PDF. See
    // https://crbug.com/448720.
    base::DictionaryValue message;
    message.SetString("type", "print");
    post_message_support->PostMessageFromValue(message);
    return true;
  }
  return false;
}

}  // namespace extensions
