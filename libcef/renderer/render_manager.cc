// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/renderer/render_manager.h"

#include <tuple>

#include "build/build_config.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "libcef/common/app_manager.h"
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
#include "cef/libcef/common/mojom/cef.mojom.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/switches.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_observer.h"

namespace {

CefRenderManager* g_manager = nullptr;

}  // namespace

// Placeholder object for guest views.
class CefGuestView : public blink::WebViewObserver {
 public:
  CefGuestView(CefRenderManager* manager,
               blink::WebView* web_view,
               bool is_windowless)
      : blink::WebViewObserver(web_view),
        manager_(manager),
        is_windowless_(is_windowless) {}

  bool is_windowless() const { return is_windowless_; }

 private:
  // RenderViewObserver methods.
  void OnDestruct() override { manager_->OnGuestViewDestroyed(this); }

  CefRenderManager* const manager_;
  const bool is_windowless_;
};

CefRenderManager::CefRenderManager() {
  DCHECK(!g_manager);
  g_manager = this;
}

CefRenderManager::~CefRenderManager() {
  g_manager = nullptr;
}

// static
CefRenderManager* CefRenderManager::Get() {
  CEF_REQUIRE_RT_RETURN(nullptr);
  return g_manager;
}

void CefRenderManager::RenderThreadConnected() {
  // Retrieve the new render thread information synchronously.
  auto params = cef::mojom::NewRenderThreadInfo::New();
  GetBrowserManager()->GetNewRenderThreadInfo(&params);

  // Cross-origin entries need to be added after WebKit is initialized.
  if (params->cross_origin_whitelist_entries) {
    cross_origin_whitelist_entries_.swap(
        *params->cross_origin_whitelist_entries);
  }

  WebKitInitialized();
}

void CefRenderManager::RenderFrameCreated(
    content::RenderFrame* render_frame,
    CefRenderFrameObserver* render_frame_observer,
    bool& browser_created,
    absl::optional<bool>& is_windowless) {
  auto browser = MaybeCreateBrowser(render_frame->GetWebView(), render_frame,
                                    &browser_created, &is_windowless);
  if (browser) {
    // Attach the frame to the observer for message routing purposes.
    render_frame_observer->AttachFrame(
        browser->GetWebFrameImpl(render_frame->GetWebFrame()).get());
  }

  // Enable support for draggable regions.
  // TODO: This has performance consequences so consider making it configurable
  // (e.g. only enabled for frameless windows). See issue #3636.
  render_frame->GetWebView()->SetSupportsAppRegion(true);
}

void CefRenderManager::WebViewCreated(blink::WebView* web_view,
                                      bool& browser_created,
                                      absl::optional<bool>& is_windowless) {
  content::RenderFrame* render_frame = nullptr;
  if (web_view->MainFrame()->IsWebLocalFrame()) {
    render_frame = content::RenderFrame::FromWebFrame(
        web_view->MainFrame()->ToWebLocalFrame());
  }

  MaybeCreateBrowser(web_view, render_frame, &browser_created, &is_windowless);
}

void CefRenderManager::DevToolsAgentAttached() {
  ++devtools_agent_count_;
}

void CefRenderManager::DevToolsAgentDetached() {
  --devtools_agent_count_;
  if (devtools_agent_count_ == 0 && uncaught_exception_stack_size_ > 0) {
    // When the last DevToolsAgent is detached the stack size is set to 0.
    // Restore the user-specified stack size here.
    CefV8SetUncaughtExceptionStackSize(uncaught_exception_stack_size_);
  }
}

void CefRenderManager::ExposeInterfacesToBrowser(mojo::BinderMap* binders) {
  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  binders->Add<cef::mojom::RenderManager>(
      base::BindRepeating(
          [](CefRenderManager* render_manager,
             mojo::PendingReceiver<cef::mojom::RenderManager> receiver) {
            render_manager->BindReceiver(std::move(receiver));
          },
          base::Unretained(this)),
      task_runner);
}

CefRefPtr<CefBrowserImpl> CefRenderManager::GetBrowserForView(
    blink::WebView* view) {
  BrowserMap::const_iterator it = browsers_.find(view);
  if (it != browsers_.end()) {
    return it->second;
  }
  return nullptr;
}

CefRefPtr<CefBrowserImpl> CefRenderManager::GetBrowserForMainFrame(
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

mojo::Remote<cef::mojom::BrowserManager>&
CefRenderManager::GetBrowserManager() {
  if (!browser_manager_) {
    content::RenderThread::Get()->BindHostReceiver(
        browser_manager_.BindNewPipeAndPassReceiver());
  }
  return browser_manager_;
}

// static
bool CefRenderManager::IsExtensionProcess() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      extensions::switches::kExtensionProcess);
}

// static
bool CefRenderManager::IsPdfProcess() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPdfRenderer);
}

void CefRenderManager::BindReceiver(
    mojo::PendingReceiver<cef::mojom::RenderManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CefRenderManager::ModifyCrossOriginWhitelistEntry(
    bool add,
    cef::mojom::CrossOriginWhiteListEntryPtr entry) {
  GURL gurl = GURL(entry->source_origin);
  if (add) {
    blink::WebSecurityPolicy::AddOriginAccessAllowListEntry(
        gurl, blink::WebString::FromUTF8(entry->target_protocol),
        blink::WebString::FromUTF8(entry->target_domain),
        /*destination_port=*/0,
        entry->allow_target_subdomains
            ? network::mojom::CorsDomainMatchMode::kAllowSubdomains
            : network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
        network::mojom::CorsPortMatchMode::kAllowAnyPort,
        network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
  } else {
    blink::WebSecurityPolicy::ClearOriginAccessListForOrigin(gurl);
  }
}

void CefRenderManager::ClearCrossOriginWhitelist() {
  blink::WebSecurityPolicy::ClearOriginAccessList();
}

void CefRenderManager::WebKitInitialized() {
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
      if (info.is_display_isolated) {
        blink::WebSecurityPolicy::RegisterURLSchemeAsDisplayIsolated(scheme);
      }
      if (info.is_fetch_enabled) {
        blink_glue::RegisterURLSchemeAsSupportingFetchAPI(scheme);
      }
    }
  }

  if (!cross_origin_whitelist_entries_.empty()) {
    // Add the cross-origin white list entries.
    for (auto& entry : cross_origin_whitelist_entries_) {
      ModifyCrossOriginWhitelistEntry(/*add=*/true, std::move(entry));
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
    if (handler.get()) {
      handler->OnWebKitInitialized();
    }
  }
}

CefRefPtr<CefBrowserImpl> CefRenderManager::MaybeCreateBrowser(
    blink::WebView* web_view,
    content::RenderFrame* render_frame,
    bool* browser_created,
    absl::optional<bool>* is_windowless) {
  if (browser_created) {
    *browser_created = false;
  }

  if (!web_view || !render_frame) {
    return nullptr;
  }

  // Don't create another browser or guest view object if one already exists for
  // the view.
  auto browser = GetBrowserForView(web_view);
  if (browser) {
    if (is_windowless) {
      *is_windowless = browser->is_windowless();
    }
    return browser;
  }

  auto guest_view = GetGuestViewForView(web_view);
  if (guest_view) {
    if (is_windowless) {
      *is_windowless = guest_view->is_windowless();
    }
    return nullptr;
  }

  const bool is_pdf = IsPdfProcess();

  auto params = cef::mojom::NewBrowserInfo::New();
  if (!is_pdf) {
    // Retrieve browser information synchronously.
    GetBrowserManager()->GetNewBrowserInfo(
        render_frame->GetWebFrame()->GetLocalFrameToken(), &params);
    if (params->browser_id == 0) {
      // The popup may have been canceled during creation.
      return nullptr;
    }
  }

  if (is_windowless) {
    *is_windowless = params->is_windowless;
  }

  if (is_pdf || params->is_guest_view || params->browser_id < 0) {
    // Don't create a CefBrowser for a PDF renderer, guest view, or if the new
    // browser info response has timed out.
    guest_views_.insert(std::make_pair(
        web_view,
        std::make_unique<CefGuestView>(this, web_view, params->is_windowless)));
    return nullptr;
  }

  browser = new CefBrowserImpl(web_view, params->browser_id, params->is_popup,
                               params->is_windowless);
  browsers_.insert(std::make_pair(web_view, browser));

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefAppManager::Get()->GetApplication();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefDictionaryValueImpl> dictValuePtr;
      if (params->extra_info) {
        dictValuePtr =
            new CefDictionaryValueImpl(std::move(*params->extra_info),
                                       /*read_only=*/true);
      }
      handler->OnBrowserCreated(browser.get(), dictValuePtr.get());
    }
  }

  if (browser_created) {
    *browser_created = true;
  }

  return browser;
}

void CefRenderManager::OnBrowserDestroyed(CefBrowserImpl* browser) {
  BrowserMap::iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    if (it->second.get() == browser) {
      browsers_.erase(it);
      return;
    }
  }

  // No browser was found in the map.
  DCHECK(false);
}

CefGuestView* CefRenderManager::GetGuestViewForView(blink::WebView* view) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  GuestViewMap::const_iterator it = guest_views_.find(view);
  if (it != guest_views_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void CefRenderManager::OnGuestViewDestroyed(CefGuestView* guest_view) {
  GuestViewMap::iterator it = guest_views_.begin();
  for (; it != guest_views_.end(); ++it) {
    if (it->second.get() == guest_view) {
      guest_views_.erase(it);
      return;
    }
  }

  // No guest view was found in the map.
  DCHECK(false);
}

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
