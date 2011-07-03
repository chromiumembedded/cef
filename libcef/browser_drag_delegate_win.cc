// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cef.h"
#include "browser_drag_delegate_win.h"
#include "browser_impl.h"
#include "browser_webview_delegate.h"
#include "cef_thread.h"
#include "drag_download_file.h"
#include "web_drag_source_win.h"
#include "web_drag_utils_win.h"
#include "web_drop_target_win.h"

#include <windows.h>

#include <string>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "net/base/file_stream.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "views/drag_utils.h"
#include "webkit/glue/webdropdata.h"

using WebKit::WebDragOperationsMask;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;
using WebKit::WebView;

namespace {

HHOOK msg_hook = NULL;
DWORD drag_out_thread_id = 0;
bool mouse_up_received = false;

LRESULT CALLBACK MsgFilterProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code == base::MessagePumpForUI::kMessageFilterCode &&
      !mouse_up_received) {
    MSG* msg = reinterpret_cast<MSG*>(lparam);
    // We do not care about WM_SYSKEYDOWN and WM_SYSKEYUP because when ALT key
    // is pressed down on drag-and-drop, it means to create a link.
    if (msg->message == WM_MOUSEMOVE || msg->message == WM_LBUTTONUP ||
        msg->message == WM_KEYDOWN || msg->message == WM_KEYUP) {
      // Forward the message from the UI thread to the drag-and-drop thread.
      PostThreadMessage(drag_out_thread_id,
                        msg->message,
                        msg->wParam,
                        msg->lParam);

      // If the left button is up, we do not need to forward the message any
      // more.
      if (msg->message == WM_LBUTTONUP || !(GetKeyState(VK_LBUTTON) & 0x8000))
        mouse_up_received = true;

      return TRUE;
    }
  }
  return CallNextHookEx(msg_hook, code, wparam, lparam);
}

// Parse the download metadata set in DataTransfer.setData. The metadata
// consists of a set of the following values separated by ":"
// * MIME type
// * File name
// * URL
// If the file name contains special characters, they need to be escaped
// appropriately.
// For example, we can have
//   text/plain:example.txt:http://example.com/example.txt
// From chrome/browser/download/drag_download_util.cc
bool ParseDownloadMetadata(const string16& metadata,
                           string16* mime_type,
                           FilePath* file_name,
                           GURL* url) {
  const char16 separator = L':';

  size_t mime_type_end_pos = metadata.find(separator);
  if (mime_type_end_pos == string16::npos)
    return false;

  size_t file_name_end_pos = metadata.find(separator, mime_type_end_pos + 1);
  if (file_name_end_pos == string16::npos)
    return false;

  GURL parsed_url = GURL(metadata.substr(file_name_end_pos + 1));
  if (!parsed_url.is_valid())
    return false;

  if (mime_type)
    *mime_type = metadata.substr(0, mime_type_end_pos);
  if (file_name) {
    string16 file_name_str = metadata.substr(
        mime_type_end_pos + 1, file_name_end_pos - mime_type_end_pos  - 1);
#if defined(OS_WIN)
    *file_name = FilePath(file_name_str);
#else
    *file_name = FilePath(UTF16ToUTF8(file_name_str));
#endif
  }
  if (url)
    *url = parsed_url;

  return true;
}

#if defined(OS_WIN)
// Returns whether the specified extension is automatically integrated into the
// windows shell.
// From chrome/browser/download/download_util.cc
bool IsShellIntegratedExtension(const string16& extension) {
  string16 extension_lower = StringToLowerASCII(extension);

  static const wchar_t* const integrated_extensions[] = {
    // See <http://msdn.microsoft.com/en-us/library/ms811694.aspx>.
    L"local",
    // Right-clicking on shortcuts can be magical.
    L"lnk",
  };

  for (int i = 0; i < arraysize(integrated_extensions); ++i) {
    if (extension_lower == integrated_extensions[i])
      return true;
  }

  // See <http://www.juniper.net/security/auto/vulnerabilities/vuln2612.html>.
  // That vulnerability report is not exactly on point, but files become magical
  // if their end in a CLSID.  Here we block extensions that look like CLSIDs.
  if (extension_lower.size() > 0 && extension_lower.at(0) == L'{' &&
      extension_lower.at(extension_lower.length() - 1) == L'}')
    return true;

  return false;
}

// Returns whether the specified file name is a reserved name on windows.
// This includes names like "com2.zip" (which correspond to devices) and
// desktop.ini and thumbs.db which have special meaning to the windows shell.
// From chrome/browser/download/download_util.cc
bool IsReservedName(const string16& filename) {
  // This list is taken from the MSDN article "Naming a file"
  // http://msdn2.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // I also added clock$ because GetSaveFileName seems to consider it as a
  // reserved name too.
  static const wchar_t* const known_devices[] = {
    L"con", L"prn", L"aux", L"nul", L"com1", L"com2", L"com3", L"com4", L"com5",
    L"com6", L"com7", L"com8", L"com9", L"lpt1", L"lpt2", L"lpt3", L"lpt4",
    L"lpt5", L"lpt6", L"lpt7", L"lpt8", L"lpt9", L"clock$"
  };
  string16 filename_lower = StringToLowerASCII(filename);

  for (int i = 0; i < arraysize(known_devices); ++i) {
    // Exact match.
    if (filename_lower == known_devices[i])
      return true;
    // Starts with "DEVICE.".
    if (filename_lower.find(string16(known_devices[i]) + L".") == 0)
      return true;
  }

  static const wchar_t* const magic_names[] = {
    // These file names are used by the "Customize folder" feature of the shell.
    L"desktop.ini",
    L"thumbs.db",
  };

  for (int i = 0; i < arraysize(magic_names); ++i) {
    if (filename_lower == magic_names[i])
      return true;
  }

  return false;
}
#endif  // OS_WIN

// Create an extension based on the file name and mime type.
// From chrome/browser/download/download_util.cc
void GenerateExtension(const FilePath& file_name,
                       const std::string& mime_type,
                       FilePath::StringType* generated_extension) {
  // We're worried about two things here:
  //
  // 1) Usability.  If the site fails to provide a file extension, we want to
  //    guess a reasonable file extension based on the content type.
  //
  // 2) Shell integration.  Some file extensions automatically integrate with
  //    the shell.  We block these extensions to prevent a malicious web site
  //    from integrating with the user's shell.

  // See if our file name already contains an extension.
  FilePath::StringType extension = file_name.Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.

#if defined(OS_WIN)
  static const FilePath::CharType default_extension[] =
      FILE_PATH_LITERAL("download");

  // Rename shell-integrated extensions.
  if (IsShellIntegratedExtension(extension))
    extension.assign(default_extension);
#endif

  if (extension.empty()) {
    // The GetPreferredExtensionForMimeType call will end up going to disk.  Do
    // this on another thread to avoid slowing the IO thread.
    // http://crbug.com/61827
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    net::GetPreferredExtensionForMimeType(mime_type, &extension);
  }

  generated_extension->swap(extension);
}

// Used to make sure we have a safe file extension and filename for a
// download.  |file_name| can either be just the file name or it can be a
// full path to a file.
// From chrome/browser/download/download_util.cc
void GenerateSafeFileName(const std::string& mime_type, FilePath* file_name) {
  // Make sure we get the right file extension
  FilePath::StringType extension;
  GenerateExtension(*file_name, mime_type, &extension);
  *file_name = file_name->ReplaceExtension(extension);

#if defined(OS_WIN)
  // Prepend "_" to the file name if it's a reserved name
  FilePath::StringType leaf_name = file_name->BaseName().value();
  DCHECK(!leaf_name.empty());
  if (IsReservedName(leaf_name)) {
    leaf_name = FilePath::StringType(FILE_PATH_LITERAL("_")) + leaf_name;
    *file_name = file_name->DirName();
    if (file_name->value() == FilePath::kCurrentDirectory) {
      *file_name = FilePath(leaf_name);
    } else {
      *file_name = file_name->Append(leaf_name);
    }
  }
#endif
}

// Create a file name based on the response from the server.
// From chrome/browser/download/download_util.cc
void GenerateFileName(const GURL& url,
                      const std::string& content_disposition,
                      const std::string& referrer_charset,
                      const std::string& mime_type,
                      FilePath* generated_name) {
  string16 new_name = net::GetSuggestedFilename(GURL(url),
                                                content_disposition,
                                                referrer_charset,
                                                "",
                                                string16(L"download"));

  // TODO(evan): this code is totally wrong -- we should just generate
  // Unicode filenames and do all this encoding switching at the end.
  // However, I'm just shuffling wrong code around, at least not adding
  // to it.
#if defined(OS_WIN)
  *generated_name = FilePath(new_name);
#else
  *generated_name = FilePath(
      base::SysWideToNativeMB(UTF16ToWide(new_name)));
#endif

  DCHECK(!generated_name->empty());

  GenerateSafeFileName(mime_type, generated_name);
}

}  // namespace

class DragDropThread : public base::Thread {
 public:
  explicit DragDropThread(BrowserDragDelegate* drag_handler)
       : base::Thread("Chrome_DragDropThread"),
         drag_handler_(drag_handler) {
  }

  virtual ~DragDropThread() {
    Thread::Stop();
  }

 protected:
  // base::Thread implementations:
  virtual void Init() {
    int ole_result = OleInitialize(NULL);
    DCHECK(ole_result == S_OK);
  }

  virtual void CleanUp() {
    OleUninitialize();
  }

 private:
  // Hold a reference count to BrowserDragDelegate to make sure that it is always
  // alive in the thread lifetime.
  scoped_refptr<BrowserDragDelegate> drag_handler_;

  DISALLOW_COPY_AND_ASSIGN(DragDropThread);
};

BrowserDragDelegate::BrowserDragDelegate(BrowserWebViewDelegate* view)
    : drag_drop_thread_id_(0),
      view_(view),
      drag_ended_(false),
      old_drop_target_suspended_state_(false) {
}

BrowserDragDelegate::~BrowserDragDelegate() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  DCHECK(!drag_drop_thread_.get());
}

void BrowserDragDelegate::StartDragging(const WebDropData& drop_data,
                                        WebDragOperationsMask ops,
                                        const SkBitmap& image,
                                        const gfx::Point& image_offset) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  CefBrowserImpl* browser = view_->GetBrowser();
  WebView* web_view = browser->UIT_GetWebView();
  drag_source_ = new WebDragSource(browser->UIT_GetWebViewWndHandle(),
                                   web_view);

  const GURL& page_url = web_view->mainFrame()->document().url();
  const std::string& page_encoding =
      web_view->mainFrame()->document().encoding().utf8();

  // If it is not drag-out, do the drag-and-drop in the current UI thread.
  if (drop_data.download_metadata.empty()) {
    DoDragging(drop_data, ops, page_url, page_encoding, image, image_offset);
    EndDragging(false);
    return;
  }

  // We do not want to drag and drop the download to itself.
  old_drop_target_suspended_state_ = view_->drop_target()->suspended();
  view_->drop_target()->set_suspended(true);

  // Start a background thread to do the drag-and-drop.
  DCHECK(!drag_drop_thread_.get());
  drag_drop_thread_.reset(new DragDropThread(this));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_UI;
  if (drag_drop_thread_->StartWithOptions(options)) {
    drag_drop_thread_->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &BrowserDragDelegate::StartBackgroundDragging,
                          drop_data,
                          ops,
                          page_url,
                          page_encoding,
                          image,
                          image_offset));
  }

  // Install a hook procedure to monitor the messages so that we can forward
  // the appropriate ones to the background thread.
  drag_out_thread_id = drag_drop_thread_->thread_id();
  mouse_up_received = false;
  DCHECK(!msg_hook);
  msg_hook = SetWindowsHookEx(WH_MSGFILTER,
                              MsgFilterProc,
                              NULL,
                              GetCurrentThreadId());

  // Attach the input state of the background thread to the UI thread so that
  // SetCursor can work from the background thread.
  AttachThreadInput(drag_out_thread_id, GetCurrentThreadId(), TRUE);
}

void BrowserDragDelegate::StartBackgroundDragging(
    const WebDropData& drop_data,
    WebDragOperationsMask ops,
    const GURL& page_url,
    const std::string& page_encoding,
    const SkBitmap& image,
    const gfx::Point& image_offset) {
  drag_drop_thread_id_ = base::PlatformThread::CurrentId();

  DoDragging(drop_data, ops, page_url, page_encoding, image, image_offset);
  CefThread::PostTask(
      CefThread::UI, FROM_HERE,
      NewRunnableMethod(this, &BrowserDragDelegate::EndDragging, true));
}

void BrowserDragDelegate::PrepareDragForDownload(
    const WebDropData& drop_data,
    ui::OSExchangeData* data,
    const GURL& page_url,
    const std::string& page_encoding) {
  // Parse the download metadata.
  string16 mime_type;
  FilePath file_name;
  GURL download_url;
  if (!ParseDownloadMetadata(drop_data.download_metadata,
                             &mime_type,
                             &file_name,
                             &download_url))
    return;

  // Generate the download filename.
  std::string content_disposition =
      "attachment; filename=" + UTF16ToUTF8(file_name.value());
  FilePath generated_file_name;
  GenerateFileName(download_url,
                   content_disposition,
                   std::string(),
                   UTF16ToUTF8(mime_type),
                   &generated_file_name);

  // Provide the data as file (CF_HDROP). A temporary download file with the
  // Zone.Identifier ADS (Alternate Data Stream) attached will be created.
  linked_ptr<net::FileStream> empty_file_stream;
  scoped_refptr<DragDownloadFile> download_file =
      new DragDownloadFile(generated_file_name,
                           empty_file_stream,
                           download_url,
                           page_url,
                           page_encoding,
                           view_);
  ui::OSExchangeData::DownloadFileInfo file_download(FilePath(),
                                                     download_file.get());
  data->SetDownloadFileInfo(file_download);

  // Enable asynchronous operation.
  ui::OSExchangeDataProviderWin::GetIAsyncOperation(*data)->SetAsyncMode(TRUE);
}

void BrowserDragDelegate::PrepareDragForFileContents(
    const WebDropData& drop_data, ui::OSExchangeData* data) {
  // Images without ALT text will only have a file extension so we need to
  // synthesize one from the provided extension and URL.
  FilePath file_name(drop_data.file_description_filename);
  file_name = file_name.BaseName().RemoveExtension();
  if (file_name.value().empty()) {
    // Retrieve the name from the URL.
    file_name = FilePath(
        net::GetSuggestedFilename(drop_data.url, "", "", "", string16()));
    if (file_name.value().size() + drop_data.file_extension.size() + 1 >
        MAX_PATH) {
      file_name = FilePath(file_name.value().substr(
          0, MAX_PATH - drop_data.file_extension.size() - 2));
    }
  }
  file_name = file_name.ReplaceExtension(drop_data.file_extension);
  data->SetFileContents(file_name, drop_data.file_contents);
}

void BrowserDragDelegate::PrepareDragForUrl(const WebDropData& drop_data,
                                               ui::OSExchangeData* data) {
  if (drop_data.url.SchemeIs("javascript")) {
    // We don't want to allow javascript URLs to be dragged to the desktop.
  } else {
    data->SetURL(drop_data.url, drop_data.url_title);
  }
}

void BrowserDragDelegate::DoDragging(const WebDropData& drop_data,
                                    WebDragOperationsMask ops,
                                    const GURL& page_url,
                                    const std::string& page_encoding,
                                    const SkBitmap& image,
                                    const gfx::Point& image_offset) {
  ui::OSExchangeData data;

  if (!drop_data.download_metadata.empty()) {
    PrepareDragForDownload(drop_data, &data, page_url, page_encoding);

    // Set the observer.
    ui::OSExchangeDataProviderWin::GetDataObjectImpl(data)->set_observer(this);
  } else {
    // We set the file contents before the URL because the URL also sets file
    // contents (to a .URL shortcut).  We want to prefer file content data over
    // a shortcut so we add it first.
    if (!drop_data.file_contents.empty())
      PrepareDragForFileContents(drop_data, &data);
    if (!drop_data.text_html.empty())
      data.SetHtml(drop_data.text_html, drop_data.html_base_url);
    // We set the text contents before the URL because the URL also sets text
    // content.
    if (!drop_data.plain_text.empty())
      data.SetString(drop_data.plain_text);
    if (drop_data.url.is_valid())
      PrepareDragForUrl(drop_data, &data);
  }

  // Set drag image.
  if (!image.isNull()) {
    drag_utils::SetDragImageOnDataObject(
        image, gfx::Size(image.width(), image.height()), image_offset, &data);
  }

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  DWORD effect;
  DoDragDrop(ui::OSExchangeDataProviderWin::GetIDataObject(data), drag_source_,
             web_drag_utils_win::WebDragOpMaskToWinDragOpMask(ops), &effect);
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  // This works because WebDragSource::OnDragSourceDrop uses PostTask to
  // dispatch the actual event.
  drag_source_->set_effect(effect);
}

void BrowserDragDelegate::EndDragging(bool restore_suspended_state) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  if (drag_ended_)
    return;
  drag_ended_ = true;

  if (restore_suspended_state)
    view_->drop_target()->set_suspended(old_drop_target_suspended_state_);

  if (msg_hook) {
    AttachThreadInput(drag_out_thread_id, GetCurrentThreadId(), FALSE);
    UnhookWindowsHookEx(msg_hook);
    msg_hook = NULL;
  }

  view_->EndDragging();
}

void BrowserDragDelegate::CancelDrag() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  drag_source_->CancelDrag();
}

void BrowserDragDelegate::CloseThread() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  drag_drop_thread_.reset();
}

void BrowserDragDelegate::OnWaitForData() {
  DCHECK(drag_drop_thread_id_ == base::PlatformThread::CurrentId());

  // When the left button is released and we start to wait for the data, end
  // the dragging before DoDragDrop returns. This makes the page leave the drag
  // mode so that it can start to process the normal input events.
  CefThread::PostTask(
      CefThread::UI, FROM_HERE,
      NewRunnableMethod(this, &BrowserDragDelegate::EndDragging, true));
}

void BrowserDragDelegate::OnDataObjectDisposed() {
  DCHECK(drag_drop_thread_id_ == base::PlatformThread::CurrentId());

  // The drag-and-drop thread is only closed after OLE is done with
  // DataObjectImpl.
  CefThread::PostTask(
      CefThread::UI, FROM_HERE,
      NewRunnableMethod(this, &BrowserDragDelegate::CloseThread));
}
