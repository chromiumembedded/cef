// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/renderer/browser_manager.h"

#include "base/compiler_specific.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if defined(OS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "libcef/common/app_manager.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/net/scheme_info.h"
#include "libcef/common/values_impl.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/render_frame_observer.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/v8_impl.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_observer.h"

namespace {

CefBrowserManager* g_manager = nullptr;

}  // namespace

// Placeholder object for guest views.
class CefGuestView : public blink::WebViewObserver {
 public:
  CefGuestView(CefBrowserManager* manager,
               content::RenderView* render_view,
               bool is_windowless)
      : blink::WebViewObserver(render_view->GetWebView()),
        manager_(manager),
        is_windowless_(is_windowless) {}

  bool is_windowless() const { return is_windowless_; }

 private:
  // RenderViewObserver methods.
  void OnDestruct() override { manager_->OnGuestViewDestroyed(this); }

  CefBrowserManager* const manager_;
  const bool is_windowless_;
};

CefBrowserManager::CefBrowserManager() {
  DCHECK(!g_manager);
  g_manager = this;
}

CefBrowserManager::~CefBrowserManager() {
  g_manager = nullptr;
}

// static
CefBrowserManager* CefBrowserManager::Get() {
  CEF_REQUIRE_RT_RETURN(nullptr);
  return g_manager;
}

void CefBrowserManager::RenderThreadConnected() {
  content::RenderThread* thread = content::RenderThread::Get();

  // Retrieve the new render thread information synchronously.
  CefProcessHostMsg_GetNewRenderThreadInfo_Params params;
  thread->Send(new CefProcessHostMsg_GetNewRenderThreadInfo(&params));

  // Cross-origin entries need to be added after WebKit is initialized.
  cross_origin_whitelist_entries_ = params.cross_origin_whitelist_entries;

  WebKitInitialized();
}

void CefBrowserManager::RenderFrameCreated(
    content::RenderFrame* render_frame,
    CefRenderFrameObserver* render_frame_observer,
    bool& browser_created,
    base::Optional<bool>& is_windowless) {
  auto browser = MaybeCreateBrowser(render_frame->GetRenderView(), render_frame,
                                    &browser_created, &is_windowless);
  if (browser) {
    // Attach the frame to the observer for message routing purposes.
    render_frame_observer->AttachFrame(
        browser->GetWebFrameImpl(render_frame->GetWebFrame()).get());
  }
}

void CefBrowserManager::RenderViewCreated(content::RenderView* render_view,
                                          bool& browser_created,
                                          base::Optional<bool>& is_windowless) {
  MaybeCreateBrowser(render_view, render_view->GetMainRenderFrame(),
                     &browser_created, &is_windowless);
}

void CefBrowserManager::DevToolsAgentAttached() {
  ++devtools_agent_count_;
}

void CefBrowserManager::DevToolsAgentDetached() {
  --devtools_agent_count_;
  if (devtools_agent_count_ == 0 && uncaught_exception_stack_size_ > 0) {
    // When the last DevToolsAgent is detached the stack size is set to 0.
    // Restore the user-specified stack size here.
    CefV8SetUncaughtExceptionStackSize(uncaught_exception_stack_size_);
  }
}

CefRefPtr<CefBrowserImpl> CefBrowserManager::GetBrowserForView(
    content::RenderView* view) {
  BrowserMap::const_iterator it = browsers_.find(view);
  if (it != browsers_.end())
    return it->second;
  return nullptr;
}

CefRefPtr<CefBrowserImpl> CefBrowserManager::GetBrowserForMainFrame(
    blink::WebFrame* frame) {
  BrowserMap::const_iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    auto web_view = it->second->GetWebView();
    if (web_view && web_view->MainFrame() == frame) {
      return it->second;
    }
  }

  return nullptr;
}

void CefBrowserManager::WebKitInitialized() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Create global objects associated with the default Isolate.
  CefV8IsolateCreated();

  const CefAppManager::SchemeInfoList* schemes =
      CefAppManager::Get()->GetCustomSchemes();
  if (!schemes->empty()) {
    // Register the custom schemes. Some attributes are excluded here because
    // they use url/url_util.h APIs instead.
    CefAppManager::SchemeInfoList::const_iterator it = schemes->begin();
    for (; it != schemes->end(); ++it) {
      const CefSchemeInfo& info = *it;
      const blink::WebString& scheme =
          blink::WebString::FromUTF8(info.scheme_name);
      if (info.is_display_isolated)
        blink::WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(scheme);
      if (info.is_fetch_enabled)
        blink_glue::RegisterURLSchemeAsSupportingFetchAPI(scheme);
    }
  }

  if (!cross_origin_whitelist_entries_.empty()) {
    // Add the cross-origin white list entries.
    for (size_t i = 0; i < cross_origin_whitelist_entries_.size(); ++i) {
      const Cef_CrossOriginWhiteListEntry_Params& entry =
          cross_origin_whitelist_entries_[i];
      GURL gurl = GURL(entry.source_origin);
      blink::WebSecurityPolicy::AddOriginAccessAllowListEntry(
          gurl, blink::WebString::FromUTF8(entry.target_protocol),
          blink::WebString::FromUTF8(entry.target_domain),
          /*destination_port=*/0,
          entry.allow_target_subdomains
              ? network::mojom::CorsDomainMatchMode::kAllowSubdomains
              : network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
          network::mojom::CorsPortMatchMode::kAllowAnyPort,
          network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    }
    cross_origin_whitelist_entries_.clear();
  }

  // The number of stack trace frames to capture for uncaught exceptions.
  if (command_line->HasSwitch(switches::kUncaughtExceptionStackSize)) {
    int uncaught_exception_stack_size = 0;
    base::StringToInt(command_line->GetSwitchValueASCII(
                          switches::kUncaughtExceptionStackSize),
                      &uncaught_exception_stack_size);

    if (uncaught_exception_stack_size > 0) {
      uncaught_exception_stack_size_ = uncaught_exception_stack_size;
      CefV8SetUncaughtExceptionStackSize(uncaught_exception_stack_size_);
    }
  }

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefAppManager::Get()->GetApplication();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get())
      handler->OnWebKitInitialized();
  }
}

CefRefPtr<CefBrowserImpl> CefBrowserManager::MaybeCreateBrowser(
    content::RenderView* render_view,
    content::RenderFrame* render_frame,
    bool* browser_created,
    base::Optional<bool>* is_windowless) {
  if (browser_created)
    *browser_created = false;

  if (!render_view || !render_frame)
    return nullptr;

  // Don't create another browser or guest view object if one already exists for
  // the view.
  auto browser = GetBrowserForView(render_view);
  if (browser) {
    if (is_windowless) {
      *is_windowless = browser->is_windowless();
    }
    return browser;
  }

  auto guest_view = GetGuestViewForView(render_view);
  if (guest_view) {
    if (is_windowless) {
      *is_windowless = guest_view->is_windowless();
    }
    return nullptr;
  }

  const int render_frame_routing_id = render_frame->GetRoutingID();

  // Retrieve the browser information synchronously. This will also register
  // the routing ids with the browser info object in the browser process.
  CefProcessHostMsg_GetNewBrowserInfo_Params params;
  content::RenderThread::Get()->Send(new CefProcessHostMsg_GetNewBrowserInfo(
      render_frame_routing_id, &params));

  if (is_windowless) {
    *is_windowless = params.is_windowless;
  }

  if (params.browser_id == 0) {
    // The popup may have been canceled during creation.
    return nullptr;
  }

  if (params.is_guest_view || params.browser_id < 0) {
    // Don't create a CefBrowser for guest views, or if the new browser info
    // response has timed out.
    guest_views_.insert(std::make_pair(
        render_view, std::make_unique<CefGuestView>(this, render_view,
                                                    params.is_windowless)));
    return nullptr;
  }

  browser = new CefBrowserImpl(render_view, params.browser_id, params.is_popup,
                               params.is_windowless);
  browsers_.insert(std::make_pair(render_view, browser));

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefAppManager::Get()->GetApplication();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefDictionaryValueImpl> dictValuePtr(
          new CefDictionaryValueImpl(&params.extra_info, false, true));
      handler->OnBrowserCreated(browser.get(), dictValuePtr.get());
      dictValuePtr->Detach(nullptr);
    }
  }

  if (browser_created)
    *browser_created = true;

  return browser;
}

void CefBrowserManager::OnBrowserDestroyed(CefBrowserImpl* browser) {
  BrowserMap::iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    if (it->second.get() == browser) {
      browsers_.erase(it);
      return;
    }
  }

  // No browser was found in the map.
  NOTREACHED();
}

CefGuestView* CefBrowserManager::GetGuestViewForView(
    content::RenderView* view) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  GuestViewMap::const_iterator it = guest_views_.find(view);
  if (it != guest_views_.end())
    return it->second.get();
  return nullptr;
}

void CefBrowserManager::OnGuestViewDestroyed(CefGuestView* guest_view) {
  GuestViewMap::iterator it = guest_views_.begin();
  for (; it != guest_views_.end(); ++it) {
    if (it->second.get() == guest_view) {
      guest_views_.erase(it);
      return;
    }
  }

  // No guest view was found in the map.
  NOTREACHED();
}

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if defined(OS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
