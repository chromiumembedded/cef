#ifndef CEF_INCLUDE_CEF_CLIPBOARD_HANDLER_H_
#define CEF_INCLUDE_CEF_CLIPBOARD_HANDLER_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_browser.h"

///
// Class used to handle file downloads. The methods of this class will called
// on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefClipboardHandler : public virtual CefBaseRefCounted {
 public:
  ///
  // Called when the clipboard is updated.
  ///
  /*--cef()--*/
  virtual void OnClipboardChanged(const char* data, size_t size) = 0;
};

#endif  // CEF_INCLUDE_CEF_DOWNLOAD_HANDLER_H_
