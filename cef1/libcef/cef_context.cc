// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/cef_context.h"
#include "libcef/browser_devtools_scheme_handler.h"
#include "libcef/browser_impl.h"
#include "libcef/browser_webkit_glue.h"
#include "libcef/v8_impl.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "crypto/nss_util.h"
#endif

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "grit/webkit_resources.h"
#endif

#if defined(OS_WIN)
#include "base/win/resource_util.h"
#endif

// Global CefContext pointer
CefRefPtr<CefContext> _Context;

namespace {

// Used in multi-threaded message loop mode to observe shutdown of the UI
// thread.
class DestructionObserver : public MessageLoop::DestructionObserver {
 public:
  explicit DestructionObserver(base::WaitableEvent *event) : event_(event) {}
  virtual void WillDestroyCurrentMessageLoop() {
    MessageLoop::current()->RemoveDestructionObserver(this);
    event_->Signal();
    delete this;
  }
 private:
  base::WaitableEvent *event_;
};

#if defined(OS_WIN)

// Helper method for retrieving a resource from a module.
base::StringPiece GetRawDataResource(HMODULE module, int resource_id) {
  void* data_ptr;
  size_t data_size;
  return base::win::GetDataResourceFromModule(module, resource_id, &data_ptr,
                                              &data_size)
      ? base::StringPiece(static_cast<char*>(data_ptr), data_size)
      : base::StringPiece();
}

#endif  // defined(OS_MACOSX)

}  // namespace


class CefResourceBundleDelegate : public ui::ResourceBundle::Delegate {
 public:
  CefResourceBundleDelegate(CefContext* context)
      : context_(context),
        allow_pack_file_load_(false) {
  }

  void set_allow_pack_file_load(bool val) { allow_pack_file_load_ = val; }

 private:
  virtual FilePath GetPathForResourcePack(
      const FilePath& pack_path,
      ui::ScaleFactor scale_factor) OVERRIDE {
    // Only allow the cef pack file to load.
    if (!context_->settings().pack_loading_disabled && allow_pack_file_load_)
      return pack_path;
    return FilePath();
  }

  virtual FilePath GetPathForLocalePack(const FilePath& pack_path,
                                        const std::string& locale) OVERRIDE {
    if (!context_->settings().pack_loading_disabled)
      return pack_path;
    return FilePath();
  }

  virtual gfx::Image GetImageNamed(int resource_id) OVERRIDE {
    return gfx::Image();
  }

  virtual gfx::Image GetNativeImageNamed(
      int resource_id,
      ui::ResourceBundle::ImageRTL rtl) OVERRIDE {
    return gfx::Image();
  }

  virtual base::RefCountedStaticMemory* LoadDataResourceBytes(
      int resource_id,
      ui::ScaleFactor scale_factor) OVERRIDE {
    return NULL;
  }

  virtual bool GetRawDataResource(int resource_id,
                                  ui::ScaleFactor scale_factor,
                                  base::StringPiece* value) OVERRIDE {
    return false;
  }

  virtual bool GetLocalizedString(int message_id, string16* value) OVERRIDE {
    return false;
  }

  virtual scoped_ptr<gfx::Font> GetFont(
      ui::ResourceBundle::FontStyle style) OVERRIDE {
    return scoped_ptr<gfx::Font>();
  }

  // CefContext pointer is guaranteed to outlive this object.
  CefContext* context_;
  bool allow_pack_file_load_;

  DISALLOW_COPY_AND_ASSIGN(CefResourceBundleDelegate);
};


bool CefInitialize(const CefSettings& settings, CefRefPtr<CefApp> application) {
  // Return true if the global context already exists.
  if (_Context.get())
    return true;

  if (settings.size != sizeof(cef_settings_t)) {
    NOTREACHED() << "invalid CefSettings structure size";
    return false;
  }

  // Create the new global context object.
  _Context = new CefContext();

  // Initialize the global context.
  return _Context->Initialize(settings, application);
}

void CefShutdown() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!_Context->process()->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  // Shut down the global context. This will block until shutdown is complete.
  _Context->Shutdown();

  // Delete the global context object.
  _Context = NULL;
}

void CefDoMessageLoopWork() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!_Context->process()->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  _Context->process()->DoMessageLoopIteration();
}

void CefRunMessageLoop() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!_Context->process()->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  _Context->process()->RunMessageLoop();
}

void CefQuitMessageLoop() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // Must always be called on the same thread as Initialize.
  if (!_Context->process()->CalledOnValidThread()) {
    NOTREACHED() << "called on invalid thread";
    return;
  }

  _Context->process()->QuitMessageLoop();
}


// CefContext

CefContext::CefContext()
  : initialized_(false),
    shutting_down_(false),
    request_context_(NULL),
    current_webviewhost_(NULL),
    dev_tools_client_count_(0) {
}

CefContext::~CefContext() {
  if (!shutting_down_)
    Shutdown();
}

bool CefContext::Initialize(const CefSettings& settings,
                            CefRefPtr<CefApp> application) {
  settings_ = settings;
  application_ = application;

  cache_path_ = FilePath(CefString(&settings.cache_path));
  if (!cache_path_.empty() &&
      !file_util::PathExists(cache_path_) &&
      !file_util::CreateDirectory(cache_path_)) {
    NOTREACHED() << "The cache_path directory could not be created";
    cache_path_.clear();
  }
  if (cache_path_.empty()) {
    // Create and use a temporary directory.
    if (cache_temp_dir_.CreateUniqueTempDir()) {
      cache_path_ = cache_temp_dir_.path();
    } else {
      NOTREACHED() << "Failed to create temporary cache_path directory";
    }
  }

#if defined(OS_MACOSX) || defined(OS_WIN)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

  process_.reset(new CefProcess(settings_.multi_threaded_message_loop));
  process_->CreateChildThreads();

  initialized_ = true;

  // Perform DevTools scheme registration when CEF initialization is complete.
  CefThread::PostTask(CefThread::UI, FROM_HERE,
                      base::Bind(&RegisterDevToolsSchemeHandler, true));

  return true;
}

void CefContext::Shutdown() {
  // Must always be called on the same thread as Initialize.
  DCHECK(process_->CalledOnValidThread());

  shutting_down_ = true;

  if (settings_.multi_threaded_message_loop) {
    // Events that will be used to signal when shutdown is complete. Start in
    // non-signaled mode so that the event will block.
    base::WaitableEvent browser_shutdown_event(false, false);
    base::WaitableEvent uithread_shutdown_event(false, false);

    // Finish shutdown on the UI thread.
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        base::Bind(&CefContext::UIT_FinishShutdown, this,
                   &browser_shutdown_event, &uithread_shutdown_event));

    // Block until browser shutdown is complete.
    browser_shutdown_event.Wait();

    // Delete the process to destroy the child threads.
    process_.reset(NULL);

    // Block until UI thread shutdown is complete.
    uithread_shutdown_event.Wait();
  } else {
    // Finish shutdown on the current thread, which should be the UI thread.
    UIT_FinishShutdown(NULL, NULL);

    // Delete the process to destroy the child threads.
    process_.reset(NULL);
  }
}

int CefContext::GetNextBrowserID() {
  return next_browser_id_.GetNext() + 1;
}

void CefContext::AddBrowser(CefRefPtr<CefBrowserImpl> browser) {
  AutoLock lock_scope(this);

  browserlist_.push_back(browser);
}

void CefContext::RemoveBrowser(CefRefPtr<CefBrowserImpl> browser) {
  bool empty = false;

  {
    AutoLock lock_scope(this);

    BrowserList::iterator it = browserlist_.begin();
    for (; it != browserlist_.end(); ++it) {
      if (it->get() == browser.get()) {
        browserlist_.erase(it);
        break;
      }
    }

    if (browserlist_.empty())
      empty = true;
  }

  if (empty) {
    if (CefThread::CurrentlyOn(CefThread::UI)) {
      webkit_glue::ClearCache();
    } else {
      CefThread::PostTask(CefThread::UI, FROM_HERE,
          base::Bind(webkit_glue::ClearCache));
    }
  }
}

CefRefPtr<CefBrowserImpl> CefContext::GetBrowserByID(int id) {
  AutoLock lock_scope(this);

  BrowserList::const_iterator it = browserlist_.begin();
  for (; it != browserlist_.end(); ++it) {
    if (it->get()->browser_id() == id)
      return it->get();
  }

  return NULL;
}

void CefContext::InitializeResourceBundle() {
#if !defined(OS_WIN)
  FilePath chrome_pak_file;
#endif
  FilePath devtools_pak_file, locales_dir;

  if (!settings_.pack_loading_disabled) {
    FilePath resources_dir_path;
    if (settings_.resources_dir_path.length > 0)
      resources_dir_path = FilePath(CefString(&settings_.resources_dir_path));
    if (resources_dir_path.empty())
      resources_dir_path = GetResourcesFilePath();

    if (!resources_dir_path.empty()) {
#if !defined(OS_WIN)
      chrome_pak_file = resources_dir_path.Append(
          FILE_PATH_LITERAL("chrome.pak"));
#endif
      devtools_pak_file = resources_dir_path.Append(
          FILE_PATH_LITERAL("devtools_resources.pak"));
    }

    if (settings_.locales_dir_path.length > 0)
      locales_dir = FilePath(CefString(&settings_.locales_dir_path));

    if (!locales_dir.empty())
      PathService::Override(ui::DIR_LOCALES, locales_dir);
  }

  std::string locale_str = locale();
  if (locale_str.empty())
    locale_str = "en-US";

  resource_bundle_delegate_.reset(new CefResourceBundleDelegate(this));
  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          locale_str, resource_bundle_delegate_.get());
  if (!settings_.pack_loading_disabled) {
    CHECK(!loaded_locale.empty()) << "Locale could not be found for "
        << locale_str;

    resource_bundle_delegate_->set_allow_pack_file_load(true);

    // The chrome.pak file is required on non-Windows platforms.
#if !defined(OS_WIN)
    if (file_util::PathExists(chrome_pak_file)) {
      ResourceBundle::GetSharedInstance().AddDataPack(
          chrome_pak_file, ui::SCALE_FACTOR_NONE);
    } else {
      NOTREACHED() << "Could not load chrome.pak";
    }
#endif

    // The devtools_resources.pak file is optional.
    if (file_util::PathExists(devtools_pak_file)) {
      ResourceBundle::GetSharedInstance().AddDataPack(
          devtools_pak_file, ui::SCALE_FACTOR_NONE);
    }

    resource_bundle_delegate_->set_allow_pack_file_load(false);
  }
}

void CefContext::CleanupResourceBundle() {
  ResourceBundle::CleanupSharedInstance();
  resource_bundle_delegate_.reset(NULL);
}

string16 CefContext::GetLocalizedString(int message_id) const {
  string16 value;

  if (application_.get()) {
    CefRefPtr<CefResourceBundleHandler> handler =
        application_->GetResourceBundleHandler();
    if (handler.get()) {
      CefString cef_str;
      if (handler->GetLocalizedString(message_id, cef_str))
        value = cef_str;
    }
  }

  if (value.empty() && !settings_.pack_loading_disabled)
    value = ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);

  if (value.empty())
    LOG(ERROR) << "No localized string available for id " << message_id;

  return value;
}

base::StringPiece CefContext::GetDataResource(int resource_id) const {
  base::StringPiece value;

  if (application_.get()) {
    CefRefPtr<CefResourceBundleHandler> handler =
        application_->GetResourceBundleHandler();
    if (handler.get()) {
      void* data = NULL;
      size_t data_size = 0;
      if (handler->GetDataResource(resource_id, data, data_size))
        value = base::StringPiece(static_cast<char*>(data), data_size);
    }
  }

#if defined(OS_WIN)
  if (value.empty()) {
    FilePath file_path;
    HMODULE hModule = NULL;

    // Try to load the resource from the DLL.
    if (PathService::Get(base::FILE_MODULE, &file_path))
      hModule = ::GetModuleHandle(file_path.value().c_str());
    if (!hModule)
      hModule = ::GetModuleHandle(NULL);
    value = ::GetRawDataResource(hModule, resource_id);
  }
#elif defined(OS_MACOSX)
  if (value.empty()) {
    switch (resource_id) {
      case IDR_BROKENIMAGE: {
        // Use webkit's broken image icon (16x16)
        static std::string broken_image_data;
        if (broken_image_data.empty()) {
          FilePath path = GetResourcesFilePath();
          // In order to match WebKit's colors for the missing image, we have to
          // use a PNG. The GIF doesn't have the color range needed to correctly
          // match the TIFF they use in Safari.
          path = path.AppendASCII("missingImage.png");
          bool success = file_util::ReadFileToString(path, &broken_image_data);
          if (!success) {
            LOG(FATAL) << "Failed reading: " << path.value();
          }
        }
        value = broken_image_data;
        break;
      }
      case IDR_TEXTAREA_RESIZER: {
        // Use webkit's text area resizer image.
        static std::string resize_corner_data;
        if (resize_corner_data.empty()) {
          FilePath path = GetResourcesFilePath();
          path = path.AppendASCII("textAreaResizeCorner.png");
          bool success = file_util::ReadFileToString(path, &resize_corner_data);
          if (!success) {
            LOG(FATAL) << "Failed reading: " << path.value();
          }
        }
        value = resize_corner_data;
        break;
      }

      default:
        break;
    }
  }
#endif  // defined(OS_MACOSX)

  if (value.empty() && !settings_.pack_loading_disabled) {
    value = ResourceBundle::GetSharedInstance().GetRawDataResource(
        resource_id, ui::SCALE_FACTOR_NONE);
  }

  if (value.empty())
    LOG(ERROR) << "No data resource available for id " << resource_id;

  return value;
}

FilePath CefContext::GetResourcesFilePath() const {
#if defined(OS_MACOSX)
  // Start out with the path to the running executable.
  FilePath execPath;
  PathService::Get(base::FILE_EXE, &execPath);

  // Get the main bundle path.
  FilePath bundlePath = base::mac::GetAppBundlePath(execPath);

  // Go into the Contents/Resources directory.
  return bundlePath.Append(FILE_PATH_LITERAL("Contents"))
                   .Append(FILE_PATH_LITERAL("Resources"));
#else
  FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);
  return pak_dir;
#endif
}

std::string CefContext::locale() const {
  std::string localeStr = CefString(&settings_.locale);
  if (!localeStr.empty())
    return localeStr;

  return "en-US";
}

void CefContext::UIT_FinishShutdown(
    base::WaitableEvent* browser_shutdown_event,
    base::WaitableEvent* uithread_shutdown_event) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  BrowserList list;

  {
    AutoLock lock_scope(this);
    if (!browserlist_.empty()) {
      list = browserlist_;
      browserlist_.clear();
    }
  }

  // Destroy any remaining browser windows.
  if (!list.empty()) {
    BrowserList::iterator it = list.begin();
    for (; it != list.end(); ++it)
      (*it)->UIT_DestroyBrowser();
  }

  if (uithread_shutdown_event) {
    // The destruction observer will signal the UI thread shutdown event when
    // the UI thread has been destroyed.
    MessageLoop::current()->AddDestructionObserver(
        new DestructionObserver(uithread_shutdown_event));

    // Signal the browser shutdown event now.
    browser_shutdown_event->Signal();
  }
}

void CefContext::UIT_DevToolsClientCreated() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  ++dev_tools_client_count_;
}

void CefContext::UIT_DevToolsClientDestroyed() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));
  --dev_tools_client_count_;
  if (dev_tools_client_count_ == 0) {
    if (settings_.uncaught_exception_stack_size > 0) {
      v8::V8::SetCaptureStackTraceForUncaughtExceptions(true,
          settings_.uncaught_exception_stack_size,
          v8::StackTrace::kDetailed);
    }
  }
}
