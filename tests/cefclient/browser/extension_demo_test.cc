// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/extension_demo_test.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"
#include "include/cef_extension.h"
#include "include/cef_extension_handler.h"
#include "include/cef_parser.h"
#include "include/cef_request_context.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/root_window.h"
#include "tests/cefclient/browser/root_window_manager.h"
#include "tests/cefclient/browser/test_runner.h"
#include "tests/shared/common/client_switches.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace client::extension_demo_test {

namespace {

const char kSwitch[] = "extensions-demo";
const char kDemoUrl[] = "https://tests/extension_demo.html";
const char kTab0Url[] = "https://vuejs.org/";
const char kTab1Url[] = "https://react.dev/";

class DemoController;

// All demo controllers keyed by main browser id. UI-thread only.
using ControllerMap = std::map<int, CefRefPtr<DemoController>>;
ControllerMap& Controllers();
CefRefPtr<DemoController> GetController(int browser_id);

// Per-main-browser state. Created on the first cefQuery from the demo page
// and kept alive in g_controllers. UI-thread only.
class DemoController : public CefExtensionHandler {
 public:
  explicit DemoController(CefRefPtr<CefBrowser> main_browser)
      : main_browser_(main_browser),
        main_browser_id_(main_browser->GetIdentifier()) {
    CEF_REQUIRE_UI_THREAD();
    main_browser_->GetHost()->GetRequestContext()->SetExtensionsHandler(this);
  }

  DemoController(const DemoController&) = delete;
  DemoController& operator=(const DemoController&) = delete;

  CefRefPtr<CefBrowser> main_browser() const { return main_browser_; }
  int main_browser_id() const { return main_browser_id_; }

  // CefExtensionHandler.
  void OnExtensionLoaded(CefRefPtr<CefExtension>) override { PushRefresh(); }
  void OnExtensionUnloaded(CefRefPtr<CefExtension>) override { PushRefresh(); }
  void OnExtensionLoadFailed(cef_errorcode_t error_code) override {
    if (!main_browser_) {
      return;
    }
    auto frame = main_browser_->GetMainFrame();
    if (!frame) {
      return;
    }
    std::string js = "if (window.onLoadFailed) window.onLoadFailed(" +
                     std::to_string(static_cast<int>(error_code)) + ");";
    frame->ExecuteJavaScript(js, frame->GetURL(), 0);
  }
  void OnExtensionActionUpdated(CefRefPtr<CefExtension>,
                                CefRefPtr<CefBrowser>) override {
    PushRefresh();
  }

  void EnsureTabBrowsers();
  void OnTabBrowserCreated(int tab_index, CefRefPtr<CefBrowser> browser);
  void OnTabBrowserClosed(int tab_index);
  void LayoutTabs(int viewport_h_px,
                  const std::vector<CefRect>& tab_rects_px);
  void OnTabFocused(int tab_index);

  void PushRefresh() {
    if (!main_browser_) {
      return;
    }
    auto frame = main_browser_->GetMainFrame();
    if (frame) {
      frame->ExecuteJavaScript(
          "if (window.refreshExtensions) window.refreshExtensions();",
          frame->GetURL(), 0);
    }
  }

  CefRefPtr<CefBrowser> active_tab_browser() const {
    if (active_tab_ == 1) {
      return tab_browsers_[0];
    }
    if (active_tab_ == 2) {
      return tab_browsers_[1];
    }
    return nullptr;
  }

  // Drop the refs that participate in the
  // request_context <-> CefExtensionHandler <-> browser cycle. Called from
  // ~DemoHandler when the last demo browser closes.
  void Detach() {
    CEF_REQUIRE_UI_THREAD();
    if (main_browser_) {
      main_browser_->GetHost()->GetRequestContext()->SetExtensionsHandler(
          nullptr);
    }
    main_browser_ = nullptr;
    tab_browsers_[0] = nullptr;
    tab_browsers_[1] = nullptr;
  }

 private:
  ~DemoController() override = default;

  CefRefPtr<CefBrowser> main_browser_;
  const int main_browser_id_;

  CefRefPtr<CefBrowser> tab_browsers_[2];
  bool tab_create_started_[2] = {false, false};
  int active_tab_ = 0;  // 0 = none, 1 = tab0, 2 = tab1.

  IMPLEMENT_REFCOUNTING(DemoController);
};

#if defined(OS_WIN)
// Minimal CefClient for tab child browsers. Captures the CefBrowser pointer
// back to the owning DemoController and reports focus changes.
class TabClient : public CefClient,
                  public CefLifeSpanHandler,
                  public CefFocusHandler {
 public:
  TabClient(int main_browser_id, int tab_index)
      : main_browser_id_(main_browser_id), tab_index_(tab_index) {}

  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }

  // CefLifeSpanHandler.
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    if (CefCurrentlyOn(TID_UI)) {
      DispatchCreated(main_browser_id_, tab_index_, browser);
    } else {
      CefPostTask(TID_UI, base::BindOnce(&TabClient::DispatchCreated,
                                         main_browser_id_, tab_index_,
                                         browser));
    }
  }
  void OnBeforeClose(CefRefPtr<CefBrowser>) override {
    if (CefCurrentlyOn(TID_UI)) {
      DispatchClosed(main_browser_id_, tab_index_);
    } else {
      CefPostTask(TID_UI, base::BindOnce(&TabClient::DispatchClosed,
                                         main_browser_id_, tab_index_));
    }
  }

  // CefFocusHandler.
  void OnGotFocus(CefRefPtr<CefBrowser>) override {
    if (CefCurrentlyOn(TID_UI)) {
      DispatchFocused(main_browser_id_, tab_index_);
    } else {
      CefPostTask(TID_UI, base::BindOnce(&TabClient::DispatchFocused,
                                         main_browser_id_, tab_index_));
    }
  }

  static void DispatchCreated(int id, int idx, CefRefPtr<CefBrowser> browser) {
    auto c = GetController(id);
    if (c) {
      c->OnTabBrowserCreated(idx, browser);
    }
  }
  static void DispatchClosed(int id, int idx) {
    auto c = GetController(id);
    if (c) {
      c->OnTabBrowserClosed(idx);
    }
  }
  static void DispatchFocused(int id, int idx) {
    auto c = GetController(id);
    if (c) {
      c->OnTabFocused(idx);
    }
  }

 private:
  const int main_browser_id_;
  const int tab_index_;

  IMPLEMENT_REFCOUNTING(TabClient);
};
#endif  // OS_WIN

ControllerMap& Controllers() {
  // Intentionally leaked; the controller map lives until process exit.
  static ControllerMap* m = new ControllerMap();
  return *m;
}

CefRefPtr<DemoController> GetController(int browser_id) {
  CEF_REQUIRE_UI_THREAD();
  auto& m = Controllers();
  auto it = m.find(browser_id);
  return it == m.end() ? nullptr : it->second;
}

CefRefPtr<DemoController> GetOrCreateController(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  auto& m = Controllers();
  const int id = browser->GetIdentifier();
  auto it = m.find(id);
  if (it != m.end()) {
    return it->second;
  }
  CefRefPtr<DemoController> c = new DemoController(browser);
  m[id] = c;
  return c;
}

void DemoController::EnsureTabBrowsers() {
  CEF_REQUIRE_UI_THREAD();
#if defined(OS_WIN)
  if (!main_browser_) {
    return;
  }
  HWND host_hwnd = main_browser_->GetHost()->GetWindowHandle();
  if (!host_hwnd) {
    return;
  }
  HWND parent_hwnd = ::GetAncestor(host_hwnd, GA_ROOT);
  if (!parent_hwnd) {
    return;
  }

  for (int i = 0; i < 2; ++i) {
    if (tab_browsers_[i] || tab_create_started_[i]) {
      continue;
    }
    tab_create_started_[i] = true;

    CefWindowInfo wi;
    wi.SetAsChild(parent_hwnd, CefRect(0, 0, 1, 1));
    wi.runtime_style = main_browser_->GetHost()->GetRuntimeStyle();

    CefBrowserSettings settings;
    MainContext::Get()->PopulateBrowserSettings(&settings);

    CefRefPtr<CefClient> client = new TabClient(main_browser_id_, i + 1);

    const char* url = i == 0 ? kTab0Url : kTab1Url;
    CefBrowserHost::CreateBrowser(
        wi, client, url, settings, /*extra_info=*/nullptr,
        main_browser_->GetHost()->GetRequestContext());
  }
#endif
}

void DemoController::OnTabBrowserCreated(int tab_index,
                                         CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  if (tab_index < 1 || tab_index > 2) {
    return;
  }
  tab_browsers_[tab_index - 1] = browser;
  // Default the first tab to active.
  if (active_tab_ == 0) {
    OnTabFocused(tab_index);
  }
  // Ask the page to re-report its tab rectangles so we can position this one.
  if (main_browser_) {
    auto frame = main_browser_->GetMainFrame();
    if (frame) {
      frame->ExecuteJavaScript("if (window.layoutTabs) window.layoutTabs();",
                               frame->GetURL(), 0);
    }
  }
}

void DemoController::OnTabBrowserClosed(int tab_index) {
  CEF_REQUIRE_UI_THREAD();
  if (tab_index < 1 || tab_index > 2) {
    return;
  }
  tab_browsers_[tab_index - 1] = nullptr;
}

void DemoController::LayoutTabs(int viewport_h_px,
                                const std::vector<CefRect>& tab_rects_px) {
  CEF_REQUIRE_UI_THREAD();
#if defined(OS_WIN)
  if (!main_browser_) {
    return;
  }
  HWND host_hwnd = main_browser_->GetHost()->GetWindowHandle();
  if (!host_hwnd) {
    return;
  }
  HWND parent_hwnd = ::GetAncestor(host_hwnd, GA_ROOT);
  if (!parent_hwnd) {
    return;
  }
  RECT cr;
  if (!::GetClientRect(parent_hwnd, &cr)) {
    return;
  }
  const int parent_client_h = cr.bottom - cr.top;
  const int top_inset = parent_client_h - viewport_h_px;

  for (size_t i = 0; i < tab_rects_px.size() && i < 2; ++i) {
    if (!tab_browsers_[i]) {
      continue;
    }
    const CefRect& r = tab_rects_px[i];
    HWND child = tab_browsers_[i]->GetHost()->GetWindowHandle();
    if (!child) {
      continue;
    }
    ::SetWindowPos(child, HWND_TOP, r.x, top_inset + r.y, r.width, r.height,
                   SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }
#endif
}

void DemoController::OnTabFocused(int tab_index) {
  CEF_REQUIRE_UI_THREAD();
  active_tab_ = tab_index;
  if (!main_browser_) {
    return;
  }
  auto frame = main_browser_->GetMainFrame();
  if (frame) {
    std::string js = "if (window.setActiveTab) window.setActiveTab(" +
                     std::to_string(tab_index) + ");";
    frame->ExecuteJavaScript(js, frame->GetURL(), 0);
  }
  PushRefresh();
}

// ---- Helpers ----

std::string IconToDataUri(CefRefPtr<CefImage> img) {
  if (!img || img->IsEmpty()) {
    return std::string();
  }
  int pw = 0, ph = 0;
  CefRefPtr<CefBinaryValue> png = img->GetAsPNG(2.0f, true, pw, ph);
  if (!png || png->GetSize() == 0) {
    png = img->GetAsPNG(1.0f, true, pw, ph);
  }
  if (!png || png->GetSize() == 0) {
    return std::string();
  }
  std::vector<unsigned char> buf(png->GetSize());
  png->GetData(buf.data(), buf.size(), 0);
  CefString b64 = CefBase64Encode(buf.data(), buf.size());
  return "data:image/png;base64," + b64.ToString();
}

std::string BuildExtensionsJson(DemoController* c) {
  CEF_REQUIRE_UI_THREAD();
  auto ctx = c->main_browser()->GetHost()->GetRequestContext();
  std::vector<CefString> ids;
  ctx->GetExtensions(ids);

  CefRefPtr<CefBrowser> active = c->active_tab_browser();

  CefRefPtr<CefListValue> list = CefListValue::Create();
  size_t n = 0;
  for (const CefString& id : ids) {
    CefRefPtr<CefExtension> ext = ctx->GetExtension(id);
    if (!ext) {
      continue;
    }
    if (!ext->ShouldShowInExtensionsUI()) {
      continue;
    }
    CefRefPtr<CefImage> icon = ext->GetActionIcon(active);
    std::string icon_uri = IconToDataUri(icon);

    CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
    d->SetString("id", ext->GetIdentifier());
    d->SetString("name", ext->GetName());
    d->SetString("version", ext->GetVersion());
    d->SetBool("enabled", ext->IsEnabled());
    d->SetString("icon", icon_uri);
    list->SetDictionary(n++, d);
  }
  CefRefPtr<CefValue> root = CefValue::Create();
  root->SetList(list);
  return CefWriteJSON(root, JSON_WRITER_DEFAULT).ToString();
}

// The cefQuery handler. One global instance services all demo browsers; it
// dispatches by browser id to the right DemoController.
class DemoHandler : public CefMessageRouterBrowserSide::Handler {
 public:
  DemoHandler() = default;

  // BaseClientHandler::OnBeforeClose deletes message handlers when the last
  // browser of the owning ClientHandler closes. Detach the controllers (so the
  // request_context drops its CefExtensionHandler ref back to them) and clear
  // the map so the controllers' CefBrowser refs are released before CefShutdown
  // -- otherwise the request_context <-> handler <-> browser cycle keeps the
  // BrowserContext alive past AtExit and the ImplManager DCHECKs.
  ~DemoHandler() override {
    for (auto& entry : Controllers()) {
      entry.second->Detach();
    }
    Controllers().clear();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    const std::string& url = frame->GetURL();
    if (url.find(kDemoUrl) != 0) {
      return false;
    }

    CefRefPtr<CefValue> v =
        CefParseJSON(request, JSON_PARSER_ALLOW_TRAILING_COMMAS);
    if (!v || v->GetType() != VTYPE_DICTIONARY) {
      callback->Failure(0, "Bad request JSON");
      return true;
    }
    CefRefPtr<CefDictionaryValue> d = v->GetDictionary();
    const std::string cmd = d->GetString("command");

    if (cmd == "init") {
      auto c = GetOrCreateController(browser);
      c->EnsureTabBrowsers();
      callback->Success("ok");
      return true;
    }

    auto c = GetController(browser->GetIdentifier());
    if (!c) {
      c = GetOrCreateController(browser);
    }

    auto ctx = browser->GetHost()->GetRequestContext();

    if (cmd == "listExtensions") {
      callback->Success(BuildExtensionsJson(c.get()));
      return true;
    }
    if (cmd == "loadUnpacked") {
      ctx->LoadUnpackedExtension(d->GetString("path"), /*handler=*/nullptr);
      callback->Success("ok");
      return true;
    }
    if (cmd == "installUnpacked") {
      ctx->InstallUnpackedExtension(d->GetString("path"),
                                    /*handler=*/nullptr);
      callback->Success("ok");
      return true;
    }
    if (cmd == "installCrx") {
      ctx->InstallExtension(d->GetString("path"), /*handler=*/nullptr);
      callback->Success("ok");
      return true;
    }
    if (cmd == "setEnabled") {
      ctx->SetExtensionEnabled(d->GetString("id"), d->GetBool("enable"));
      callback->Success("ok");
      return true;
    }
    if (cmd == "remove") {
      ctx->UninstallExtension(d->GetString("id"));
      callback->Success("ok");
      return true;
    }
    if (cmd == "showPopup") {
      const std::string id = d->GetString("id");
      CefRect rect(d->GetInt("x"), d->GetInt("y"),
                   d->GetInt("w"), d->GetInt("h"));
      CefRefPtr<CefBrowser> src = c->active_tab_browser();
      if (!src) {
        src = browser;
      }
      ctx->ShowExtensionPopup(id, src, rect);
      callback->Success("ok");
      return true;
    }
    if (cmd == "closePopup") {
      ctx->CloseExtensionPopup(d->GetString("id"));
      callback->Success("ok");
      return true;
    }
    if (cmd == "layoutTabs") {
      const int vh = d->GetInt("vh");
      std::vector<CefRect> rects;
      CefRefPtr<CefListValue> list = d->GetList("tabs");
      if (list) {
        for (size_t i = 0; i < list->GetSize(); ++i) {
          CefRefPtr<CefDictionaryValue> t = list->GetDictionary(i);
          if (!t) {
            continue;
          }
          rects.emplace_back(t->GetInt("x"), t->GetInt("y"),
                             t->GetInt("w"), t->GetInt("h"));
        }
      }
      c->LayoutTabs(vh, rects);
      callback->Success("ok");
      return true;
    }

    callback->Failure(0, "Unknown command: " + cmd);
    return true;
  }
};

}  // namespace

bool IsEnabled(CefRefPtr<CefCommandLine> command_line) {
  return command_line && command_line->HasSwitch(kSwitch);
}

void Launch(CefRefPtr<CefCommandLine> command_line) {
  // Use the same defaults as a normal cefclient first window: RootWindowConfig
  // resolves --use-views / --use-alloy-style / --hide-controls / etc. from
  // |command_line|. We only substitute the loaded URL and OSR flag, mirroring
  // the existing cefclient_win.cc/cefclient_gtk.cc first-window setup.
  auto config = std::make_unique<RootWindowConfig>(command_line->Copy());
  config->always_on_top = command_line->HasSwitch(switches::kAlwaysOnTop);
  config->with_osr =
      command_line->HasSwitch(switches::kOffScreenRenderingEnabled);
  config->url = kDemoUrl;
  MainContext::Get()->GetRootWindowManager()->CreateRootWindow(
      std::move(config));
}

void CreateMessageHandlers(test_runner::MessageHandlerSet& handlers) {
  handlers.insert(new DemoHandler());
}

}  // namespace client::extension_demo_test
