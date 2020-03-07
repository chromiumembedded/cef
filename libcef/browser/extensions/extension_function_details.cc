// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2014 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/extensions/extension_function_details.h"

#include "libcef/browser/browser_context.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/navigate_params.h"
#include "libcef/browser/thread_util.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/error_utils.h"

using content::RenderViewHost;
using content::WebContents;

namespace extensions {

namespace keys = extensions::tabs_constants;

namespace {

class CefGetExtensionLoadFileCallbackImpl
    : public CefGetExtensionResourceCallback {
 public:
  CefGetExtensionLoadFileCallbackImpl(
      const std::string& file,
      CefExtensionFunctionDetails::LoadFileCallback callback)
      : file_(file), callback_(std::move(callback)) {}

  ~CefGetExtensionLoadFileCallbackImpl() {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(file_, std::move(callback_), nullptr);
      } else {
        CEF_POST_TASK(CEF_UIT, base::BindOnce(
                                   &CefGetExtensionLoadFileCallbackImpl::RunNow,
                                   file_, std::move(callback_), nullptr));
      }
    }
  }

  void Continue(CefRefPtr<CefStreamReader> stream) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        // Always continue asynchronously.
        CEF_POST_TASK(CEF_UIT, base::BindOnce(
                                   &CefGetExtensionLoadFileCallbackImpl::RunNow,
                                   file_, std::move(callback_), stream));
      }
    } else {
      CEF_POST_TASK(CEF_UIT, base::BindOnce(
                                 &CefGetExtensionLoadFileCallbackImpl::Continue,
                                 this, stream));
    }
  }

  void Cancel() override { Continue(nullptr); }

  void Disconnect() { callback_.Reset(); }

 private:
  static void RunNow(const std::string& file,
                     CefExtensionFunctionDetails::LoadFileCallback callback,
                     CefRefPtr<CefStreamReader> stream) {
    CEF_REQUIRE_UIT();

    if (!stream) {
      std::move(callback).Run(nullptr);
      return;
    }

    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(LoadFileFromStream, file, stream), std::move(callback));
  }

  static std::unique_ptr<std::string> LoadFileFromStream(
      const std::string& file,
      CefRefPtr<CefStreamReader> stream) {
    CEF_REQUIRE_BLOCKING();

    // Move to the end of the stream.
    stream->Seek(0, SEEK_END);
    const int64 size = stream->Tell();
    if (size == 0) {
      LOG(WARNING) << "Extension resource " << file << " is empty.";
      return nullptr;
    }

    std::unique_ptr<std::string> result(new std::string());
    result->resize(size);

    // Move to the beginning of the stream.
    stream->Seek(0, SEEK_SET);

    // Read all stream contents into the string.
    int64 read, offset = 0;
    do {
      read =
          static_cast<int>(stream->Read(&(*result)[offset], 1, size - offset));
      offset += read;
    } while (read > 0 && offset < size);

    if (offset != size) {
      LOG(WARNING) << "Extension resource " << file << " read failed; expected "
                   << size << ", got " << offset << " bytes.";
      return nullptr;
    }

    return result;
  }

  const std::string file_;
  CefExtensionFunctionDetails::LoadFileCallback callback_;

  IMPLEMENT_REFCOUNTING(CefGetExtensionLoadFileCallbackImpl);
  DISALLOW_COPY_AND_ASSIGN(CefGetExtensionLoadFileCallbackImpl);
};

}  // namespace

CefExtensionFunctionDetails::CefExtensionFunctionDetails(
    ExtensionFunction* function)
    : function_(function) {}

CefExtensionFunctionDetails::~CefExtensionFunctionDetails() {}

Profile* CefExtensionFunctionDetails::GetProfile() const {
  return Profile::FromBrowserContext(function_->browser_context());
}

CefRefPtr<CefBrowserHostImpl> CefExtensionFunctionDetails::GetSenderBrowser()
    const {
  content::WebContents* web_contents = function_->GetSenderWebContents();
  if (web_contents)
    return CefBrowserHostImpl::GetBrowserForContents(web_contents);
  return nullptr;
}

CefRefPtr<CefBrowserHostImpl> CefExtensionFunctionDetails::GetCurrentBrowser()
    const {
  // Start with the browser hosting the extension.
  CefRefPtr<CefBrowserHostImpl> browser = GetSenderBrowser();
  if (browser && browser->client()) {
    CefRefPtr<CefExtensionHandler> handler = GetCefExtension()->GetHandler();
    if (handler) {
      // Give the handler an opportunity to specify a different browser.
      CefRefPtr<CefBrowser> active_browser =
          handler->GetActiveBrowser(GetCefExtension(), browser.get(),
                                    function_->include_incognito_information());
      if (active_browser && active_browser != browser) {
        CefRefPtr<CefBrowserHostImpl> active_browser_impl =
            static_cast<CefBrowserHostImpl*>(active_browser.get());

        // Make sure we're operating in the same BrowserContextImpl.
        if (CefBrowserContext::GetForContext(browser->GetBrowserContext()) ==
            CefBrowserContext::GetForContext(
                active_browser_impl->GetBrowserContext())) {
          browser = active_browser_impl;
        } else {
          LOG(WARNING) << "Browser with tabId "
                       << active_browser->GetIdentifier()
                       << " cannot be accessed because is uses a different "
                          "CefRequestContext";
        }
      }
    }
  }

  // May be null during startup/shutdown.
  return browser;
}

bool CefExtensionFunctionDetails::CanAccessBrowser(
    CefRefPtr<CefBrowserHostImpl> target) const {
  DCHECK(target);

  // Start with the browser hosting the extension.
  CefRefPtr<CefBrowserHostImpl> browser = GetSenderBrowser();
  if (browser == target) {
    // A sender can always access itself.
    return true;
  }

  if (browser && browser->client()) {
    CefRefPtr<CefExtensionHandler> handler = GetCefExtension()->GetHandler();
    if (handler) {
      return handler->CanAccessBrowser(
          GetCefExtension(), browser.get(),
          function_->include_incognito_information(), target);
    }
  }

  // Default to allowing access.
  return true;
}

CefRefPtr<CefBrowserHostImpl>
CefExtensionFunctionDetails::GetBrowserForTabIdFirstTime(
    int tab_id,
    std::string* error_message) const {
  DCHECK(!get_browser_called_first_time_);
  get_browser_called_first_time_ = true;

  CefRefPtr<CefBrowserHostImpl> browser;

  if (tab_id >= 0) {
    // May be an invalid tabId or in the wrong BrowserContext.
    browser = GetBrowserForTabId(tab_id, function_->browser_context());
    if (!browser || !browser->web_contents() || !CanAccessBrowser(browser)) {
      if (error_message) {
        *error_message = ErrorUtils::FormatErrorMessage(
            keys::kTabNotFoundError, base::NumberToString(tab_id));
      }
      return nullptr;
    }
  } else {
    // May return NULL during shutdown.
    browser = GetCurrentBrowser();
    if (!browser || !browser->web_contents()) {
      if (error_message) {
        *error_message = keys::kNoCurrentWindowError;
      }
      return nullptr;
    }
  }

  return browser;
}

CefRefPtr<CefBrowserHostImpl>
CefExtensionFunctionDetails::GetBrowserForTabIdAgain(
    int tab_id,
    std::string* error_message) const {
  DCHECK_GE(tab_id, 0);
  DCHECK(get_browser_called_first_time_);

  // May return NULL during shutdown.
  CefRefPtr<CefBrowserHostImpl> browser =
      GetBrowserForTabId(tab_id, function_->browser_context());
  if (!browser || !browser->web_contents()) {
    if (error_message) {
      *error_message = ErrorUtils::FormatErrorMessage(
          keys::kTabNotFoundError, base::NumberToString(tab_id));
    }
  }
  return browser;
}

bool CefExtensionFunctionDetails::LoadFile(const std::string& file,
                                           LoadFileCallback callback) const {
  // Start with the browser hosting the extension.
  CefRefPtr<CefBrowserHostImpl> browser = GetSenderBrowser();
  if (browser && browser->client()) {
    CefRefPtr<CefExtensionHandler> handler = GetCefExtension()->GetHandler();
    if (handler) {
      CefRefPtr<CefGetExtensionLoadFileCallbackImpl> cef_callback(
          new CefGetExtensionLoadFileCallbackImpl(file, std::move(callback)));
      if (handler->GetExtensionResource(GetCefExtension(), browser.get(), file,
                                        cef_callback)) {
        return true;
      }
      cef_callback->Disconnect();
    }
  }

  return false;
}

CefExtensionFunctionDetails::OpenTabParams::OpenTabParams() {}

CefExtensionFunctionDetails::OpenTabParams::~OpenTabParams() {}

base::DictionaryValue* CefExtensionFunctionDetails::OpenTab(
    const OpenTabParams& params,
    bool user_gesture,
    std::string* error_message) const {
  CefRefPtr<CefBrowserHostImpl> sender_browser = GetSenderBrowser();
  if (!sender_browser)
    return nullptr;

  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (params.window_id.get())
    window_id = *params.window_id;

  // CEF doesn't have the concept of windows containing tab strips so we'll
  // select an "active browser" for BrowserContext sharing instead.
  CefRefPtr<CefBrowserHostImpl> active_browser =
      GetBrowserForTabIdFirstTime(window_id, error_message);
  if (!active_browser)
    return nullptr;

  // If an opener browser was specified then we expect it to exist.
  int opener_browser_id = -1;
  if (params.opener_tab_id.get() && *params.opener_tab_id >= 0) {
    if (GetBrowserForTabIdAgain(*params.opener_tab_id, error_message)) {
      opener_browser_id = *params.opener_tab_id;
    } else {
      return nullptr;
    }
  }

  GURL url;
  if (params.url.get()) {
    std::string url_string = *params.url;
    url = ExtensionTabUtil::ResolvePossiblyRelativeURL(url_string,
                                                       function()->extension());
    if (!url.is_valid()) {
      if (error_message) {
        *error_message =
            ErrorUtils::FormatErrorMessage(keys::kInvalidUrlError, url_string);
      }
      return nullptr;
    }
  }

  // Don't let extensions crash the browser or renderers.
  if (ExtensionTabUtil::IsKillURL(url)) {
    if (error_message)
      *error_message = keys::kNoCrashBrowserError;
    return nullptr;
  }

  // Default to foreground for the new tab. The presence of 'active' property
  // will override this default.
  bool active = true;
  if (params.active.get())
    active = *params.active;

  // CEF doesn't use the index value but we let the client see/modify it.
  int index = 0;
  if (params.index.get())
    index = *params.index;

  CefBrowserContext* browser_context_impl =
      CefBrowserContext::GetForContext(active_browser->GetBrowserContext());

  // A CEF representation should always exist.
  CefRefPtr<CefExtension> cef_extension =
      browser_context_impl->extension_system()->GetExtension(
          function()->extension()->id());
  DCHECK(cef_extension);
  if (!cef_extension)
    return nullptr;

  // Always use the same request context that the extension was registered with.
  // GetLoaderContext() will return NULL for internal extensions.
  CefRefPtr<CefRequestContext> request_context =
      cef_extension->GetLoaderContext();
  if (!request_context)
    return nullptr;

  CefBrowserHostImpl::CreateParams create_params;
  create_params.url = url;
  create_params.request_context = request_context;
  create_params.window_info.reset(new CefWindowInfo);

#if defined(OS_WIN)
  create_params.window_info->SetAsPopup(nullptr, CefString());
#endif

  // Start with the active browser's settings.
  create_params.client = active_browser->GetClient();
  create_params.settings = active_browser->settings();

  CefRefPtr<CefExtensionHandler> handler = cef_extension->GetHandler();
  if (handler.get() &&
      handler->OnBeforeBrowser(cef_extension, sender_browser.get(),
                               active_browser.get(), index, url.spec(), active,
                               *create_params.window_info, create_params.client,
                               create_params.settings)) {
    // Cancel the browser creation.
    return nullptr;
  }

  if (active_browser->IsViewsHosted()) {
    // The new browser will also be Views hosted.
    create_params.window_info.reset();
  }

  // Browser creation may fail under certain rare circumstances.
  CefRefPtr<CefBrowserHostImpl> new_browser =
      CefBrowserHostImpl::Create(create_params);
  if (!new_browser)
    return nullptr;

  // Return data about the newly created tab.
  auto extension = function()->extension();
  auto web_contents = new_browser->web_contents();
  auto result = CreateTabObject(new_browser, opener_browser_id, active, index);
  auto scrub_tab_behavior = ExtensionTabUtil::GetScrubTabBehavior(
      extension, extensions::Feature::Context::UNSPECIFIED_CONTEXT,
      web_contents);
  ExtensionTabUtil::ScrubTabForExtension(extension, web_contents, result.get(),
                                         scrub_tab_behavior);
  return result->ToValue().release();
}

std::unique_ptr<api::tabs::Tab> CefExtensionFunctionDetails::CreateTabObject(
    CefRefPtr<CefBrowserHostImpl> new_browser,
    int opener_browser_id,
    bool active,
    int index) const {
  content::WebContents* contents = new_browser->web_contents();

  bool is_loading = contents->IsLoading();
  auto tab_object = std::make_unique<api::tabs::Tab>();
  tab_object->id = std::make_unique<int>(new_browser->GetIdentifier());
  tab_object->index = index;
  tab_object->window_id = *tab_object->id;
  tab_object->status = std::make_unique<std::string>(
      is_loading ? keys::kStatusValueLoading : keys::kStatusValueComplete);
  tab_object->active = active;
  tab_object->selected = true;
  tab_object->highlighted = true;
  tab_object->pinned = false;
  // TODO(extensions): Use RecentlyAudibleHelper to populate |audible|.
  tab_object->discarded = false;
  tab_object->auto_discardable = false;
  tab_object->muted_info = CreateMutedInfo(contents);
  tab_object->incognito = false;
  gfx::Size contents_size = contents->GetContainerBounds().size();
  tab_object->width = std::make_unique<int>(contents_size.width());
  tab_object->height = std::make_unique<int>(contents_size.height());
  tab_object->url = std::make_unique<std::string>(contents->GetURL().spec());
  tab_object->title =
      std::make_unique<std::string>(base::UTF16ToUTF8(contents->GetTitle()));

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (entry && entry->GetFavicon().valid) {
    tab_object->fav_icon_url =
        std::make_unique<std::string>(entry->GetFavicon().url.spec());
  }

  if (opener_browser_id >= 0)
    tab_object->opener_tab_id = std::make_unique<int>(opener_browser_id);

  return tab_object;
}

// static
std::unique_ptr<api::tabs::MutedInfo>
CefExtensionFunctionDetails::CreateMutedInfo(content::WebContents* contents) {
  DCHECK(contents);
  std::unique_ptr<api::tabs::MutedInfo> info(new api::tabs::MutedInfo);
  info->muted = contents->IsAudioMuted();
  // TODO(cef): Maybe populate |info->reason|.
  return info;
}

CefRefPtr<CefExtension> CefExtensionFunctionDetails::GetCefExtension() const {
  if (!cef_extension_) {
    cef_extension_ =
        static_cast<CefBrowserContext*>(function_->browser_context())
            ->extension_system()
            ->GetExtension(function_->extension_id());
    DCHECK(cef_extension_);
  }
  return cef_extension_;
}

}  // namespace extensions
