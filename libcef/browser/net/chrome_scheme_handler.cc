// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/chrome_scheme_handler.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "include/cef_version.h"
#include "include/cef_web_plugin.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/net/internal_scheme_handler.h"
#include "libcef/browser/net/url_request_manager.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/content_client.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "cef/grit/cef_resources.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/url_constants.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "ipc/ipc_channel.h"
#include "net/url_request/url_request.h"
#include "v8/include/v8.h"

namespace scheme {

const char kChromeURL[] = "chrome://";

namespace {

const char kChromeUILicenseHost[] = "license";
const char kChromeUIWebUIHostsHost[] = "webui-hosts";

// Chrome hosts implemented by WebUI.
// Some WebUI handlers have Chrome dependencies that may fail in CEF without
// additional changes. Do not add new hosts to this list without also manually
// testing all related functionality in CEF.
const char* kAllowedWebUIHosts[] = {
  content::kChromeUIAppCacheInternalsHost,
  content::kChromeUIAccessibilityHost,
  content::kChromeUIBlobInternalsHost,
  chrome::kChromeUICreditsHost,
  content::kChromeUIGpuHost,
  content::kChromeUIHistogramHost,
  content::kChromeUIIndexedDBInternalsHost,
  content::kChromeUIMediaInternalsHost,
  chrome::kChromeUINetExportHost,
  chrome::kChromeUINetInternalsHost,
  content::kChromeUINetworkErrorHost,
  content::kChromeUINetworkErrorsListingHost,
  content::kChromeUINetworkViewCacheHost,
  content::kChromeUIResourcesHost,
  content::kChromeUIServiceWorkerInternalsHost,
  chrome::kChromeUISystemInfoHost,
  content::kChromeUITracingHost,
  content::kChromeUIWebRTCInternalsHost,
};

// Hosts that don't have useful output when linked directly. They'll be excluded
// from the "chrome://webui-hosts" listing.
const char* kUnlistedHosts[] = {
  content::kChromeUINetworkErrorHost,
  content::kChromeUIResourcesHost,
};

enum ChromeHostId {
  CHROME_UNKNOWN = 0,
  CHROME_LICENSE,
  CHROME_VERSION,
  CHROME_WEBUI_HOSTS,
};

// Chrome hosts implemented by CEF.
const struct {
  const char* host;
  ChromeHostId host_id;
} kAllowedCefHosts[] = {
  { kChromeUILicenseHost, CHROME_LICENSE },
  { chrome::kChromeUIVersionHost, CHROME_VERSION },
  { kChromeUIWebUIHostsHost, CHROME_WEBUI_HOSTS },
};

ChromeHostId GetChromeHostId(const std::string& host) {
  for (size_t i = 0;
       i < sizeof(kAllowedCefHosts) / sizeof(kAllowedCefHosts[0]); ++i) {
    if (base::EqualsCaseInsensitiveASCII(kAllowedCefHosts[i].host,
                                         host.c_str())) {
      return kAllowedCefHosts[i].host_id;
    }
  }

  return CHROME_UNKNOWN;
}

// Returns CEF and WebUI hosts. Does not include chrome debug hosts (for
// crashing, etc).
void GetAllowedHosts(std::vector<std::string>* hosts) {
  // Hosts implemented by CEF.
  for (size_t i = 0;
       i < sizeof(kAllowedCefHosts) / sizeof(kAllowedCefHosts[0]); ++i) {
    hosts->push_back(kAllowedCefHosts[i].host);
  }

  // Explicitly whitelisted WebUI hosts.
  for (size_t i = 0;
       i < sizeof(kAllowedWebUIHosts) / sizeof(kAllowedWebUIHosts[0]); ++i) {
    hosts->push_back(kAllowedWebUIHosts[i]);
  }
}

// Returns true if a host should not be listed on "chrome://webui-hosts".
bool IsUnlistedHost(const std::string& host) {
  for (size_t i = 0;
       i < sizeof(kUnlistedHosts) / sizeof(kUnlistedHosts[0]); ++i) {
    if (host == kUnlistedHosts[i])
      return true;
  }
  return false;
}

// Returns true if a host is WebUI and should be allowed to load.
bool IsAllowedWebUIHost(const std::string& host) {
  // Explicitly whitelisted WebUI hosts.
  for (size_t i = 0;
        i < sizeof(kAllowedWebUIHosts) / sizeof(kAllowedWebUIHosts[0]); ++i) {
    if (base::EqualsCaseInsensitiveASCII(kAllowedWebUIHosts[i],
                                         host.c_str())) {
      return true;
    }
  }

  return false;
}

// Additional debug URLs that are not included in chrome::kChromeDebugURLs.
const char* kAllowedDebugURLs[] = {
  content::kChromeUIBrowserCrashURL,
};

// Returns true for debug URLs that receive special handling (for crashes, etc).
bool IsDebugURL(const GURL& url) {
  // URLs handled by the renderer process in
  // content/renderer/render_frame_impl.cc MaybeHandleDebugURL().
  if (content::IsRendererDebugURL(url))
    return true;

  // Also include URLs handled by the browser process in
  // content/browser/frame_host/debug_urls.cc HandleDebugURL().
  for (int i = 0; i < chrome::kNumberOfChromeDebugURLs; ++i) {
    GURL host(chrome::kChromeDebugURLs[i]);
    if (url.GetOrigin() == host.GetOrigin())
      return true;
  }

  for (size_t i = 0;
       i < sizeof(kAllowedDebugURLs) / sizeof(kAllowedDebugURLs[0]); ++i) {
    GURL host(kAllowedDebugURLs[i]);
    if (url.GetOrigin() == host.GetOrigin())
      return true;
  }

  return false;
}

void GetDebugURLs(std::vector<std::string>* urls) {
  for (int i = 0; i < chrome::kNumberOfChromeDebugURLs; ++i) {
    urls->push_back(chrome::kChromeDebugURLs[i]);
  }

  for (size_t i = 0;
       i < sizeof(kAllowedDebugURLs) / sizeof(kAllowedDebugURLs[0]); ++i) {
    urls->push_back(kAllowedDebugURLs[i]);
  }
}

// Intercepts all WebUI calls and either blocks them or forwards them to the
// Content or Chrome WebUI factory as appropriate.
class CefWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  // Returns true if WebUI is allowed to handle the specified |url|.
  static bool AllowWebUIForURL(const GURL& url) {
    if (!url.SchemeIs(content::kChromeUIScheme))
      return false;

    if (IsAllowedWebUIHost(url.host()))
      return true;

    return false;
  }

  content::WebUIController* CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) const override {
    content::WebUIController* controller = nullptr;
    if (!AllowWebUIForURL(url))
      return controller;

    controller = content::ContentWebUIControllerFactory::GetInstance()->
        CreateWebUIControllerForURL(web_ui, url);
    if (controller != nullptr)
      return controller;

    controller = ChromeWebUIControllerFactory::GetInstance()->
        CreateWebUIControllerForURL(web_ui, url);
    if (controller != nullptr)
      return controller;

    return nullptr;
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) const override {
    content::WebUI::TypeID type = content::WebUI::kNoWebUI;
    if (!AllowWebUIForURL(url))
      return type;

    type = content::ContentWebUIControllerFactory::GetInstance()->GetWebUIType(
        browser_context, url);
    if (type != content::WebUI::kNoWebUI)
      return type;

    type = ChromeWebUIControllerFactory::GetInstance()->GetWebUIType(
        browser_context, url);
    if (type != content::WebUI::kNoWebUI)
      return type;

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) const override {
    if (!AllowWebUIForURL(url))
      return false;

    if (content::ContentWebUIControllerFactory::GetInstance()->UseWebUIForURL(
            browser_context, url) ||
        ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
            browser_context, url)) {
      return true;
    }

    return false;
  }

  bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
                              const GURL& url) const override {
    if (!AllowWebUIForURL(url))
      return false;

    if (content::ContentWebUIControllerFactory::GetInstance()->
            UseWebUIBindingsForURL(browser_context, url) ||
        ChromeWebUIControllerFactory::GetInstance()->UseWebUIBindingsForURL(
            browser_context, url)) {
      return true;
    }

    return false;
  }

  static void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) {
    // about: handler. Must come before chrome: handler, since it will
    // rewrite about: urls to chrome: URLs and then expect chrome: to
    // actually handle them.  Also relies on a preliminary fixup phase.
    handler->SetFixupHandler(&FixupBrowserAboutURL);
    handler->AddHandlerPair(&WillHandleBrowserAboutURL,
                            content::BrowserURLHandler::null_handler());

    // chrome: & friends.
    handler->AddHandlerPair(&HandleWebUI, &HandleWebUIReverse);
  }

  static CefWebUIControllerFactory* GetInstance();

 protected:
  CefWebUIControllerFactory() {}
  ~CefWebUIControllerFactory() override {}

 private:
  friend struct base::LazyInstanceTraitsBase<CefWebUIControllerFactory>;

  // From chrome/browser/chrome_content_browser_client.cc

  // Returns a copy of the given url with its host set to given host and path
  // set to given path. Other parts of the url will be the same.
  static GURL ReplaceURLHostAndPath(const GURL& url,
                                    const std::string& host,
                                    const std::string& path) {
    url::Replacements<char> replacements;
    replacements.SetHost(host.c_str(), url::Component(0, host.length()));
    replacements.SetPath(path.c_str(), url::Component(0, path.length()));
    return url.ReplaceComponents(replacements);
  }

  // Maps "foo://bar/baz/" to "foo://chrome/bar/baz/".
  static GURL AddUberHost(const GURL& url) {
    const std::string uber_host = chrome::kChromeUIUberHost;
    std::string new_path;
    url.host_piece().AppendToString(&new_path);
    url.path_piece().AppendToString(&new_path);

    return ReplaceURLHostAndPath(url, uber_host, new_path);
  }

  // If url->host() is "chrome" and url->path() has characters other than the
  // first slash, changes the url from "foo://chrome/bar/" to "foo://bar/" and
  // returns true. Otherwise returns false.
  static bool RemoveUberHost(GURL* url) {
    if (url->host() != chrome::kChromeUIUberHost)
      return false;

    if (url->path().empty() || url->path() == "/")
      return false;

    const std::string old_path = url->path();

    const std::string::size_type separator = old_path.find('/', 1);
    std::string new_host;
    std::string new_path;
    if (separator == std::string::npos) {
      new_host = old_path.substr(1);
    } else {
      new_host = old_path.substr(1, separator - 1);
      new_path = old_path.substr(separator);
    }

    // Do not allow URLs with paths empty before the first slash since we can't
    // have an empty host. (e.g "foo://chrome//")
    if (new_host.empty())
      return false;

    *url = ReplaceURLHostAndPath(*url, new_host, new_path);

    DCHECK(url->is_valid());

    return true;
  }

  // Handles rewriting Web UI URLs.
  static bool HandleWebUI(GURL* url, content::BrowserContext* browser_context) {
    // Do not handle special URLs such as "about:foo"
    if (!url->host().empty()) {
      const GURL chrome_url = AddUberHost(*url);

      // Handle valid "chrome://chrome/foo" URLs so the reverse handler will
      // be called.
      if (GetInstance()->UseWebUIForURL(browser_context, chrome_url))
        return true;
    }

    if (!GetInstance()->UseWebUIForURL(browser_context, *url))
      return false;

    return true;
  }

  // Reverse URL handler for Web UI. Maps "chrome://chrome/foo/" to
  // "chrome://foo/".
  static bool HandleWebUIReverse(GURL* url,
                                 content::BrowserContext* browser_context) {
    if (!url->is_valid() || !url->SchemeIs(content::kChromeUIScheme))
      return false;

    return RemoveUberHost(url);
  }

  DISALLOW_COPY_AND_ASSIGN(CefWebUIControllerFactory);
};

base::LazyInstance<CefWebUIControllerFactory>::Leaky
    g_web_ui_controller_factory = LAZY_INSTANCE_INITIALIZER;

// static
CefWebUIControllerFactory* CefWebUIControllerFactory::GetInstance() {
  return &g_web_ui_controller_factory.Get();
}


std::string GetOSType() {
#if defined(OS_WIN)
  return "Windows";
#elif defined(OS_MACOSX)
  return "Mac OS X";
#elif defined(OS_CHROMEOS)
  return "Chromium OS";
#elif defined(OS_ANDROID)
  return "Android";
#elif defined(OS_LINUX)
  return "Linux";
#elif defined(OS_FREEBSD)
  return "FreeBSD";
#elif defined(OS_OPENBSD)
  return "OpenBSD";
#elif defined(OS_SOLARIS)
  return "Solaris";
#else
  return "Unknown";
#endif
}

std::string GetCommandLine() {
#if defined(OS_WIN)
  return base::WideToUTF8(
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());
#elif defined(OS_POSIX)
  std::string command_line = "";
  typedef std::vector<std::string> ArgvList;
  const ArgvList& argv = base::CommandLine::ForCurrentProcess()->argv();
  for (ArgvList::const_iterator iter = argv.begin(); iter != argv.end(); iter++)
    command_line += " " + *iter;
  // TODO(viettrungluu): |command_line| could really have any encoding, whereas
  // below we assumes it's UTF-8.
  return command_line;
#endif
}

std::string GetModulePath() {
  base::FilePath path;
  if (PathService::Get(base::FILE_MODULE, &path))
    return CefString(path.value());
  return std::string();
}

class TemplateParser {
 public:
  TemplateParser()
      : ident_start_("$$"),
        ident_end_("$$") {
  }

  TemplateParser(const std::string& ident_start,
                 const std::string& ident_end)
      : ident_start_(ident_start),
        ident_end_(ident_end) {
  }

  void Add(const std::string& key, const std::string& value) {
    values_.insert(std::make_pair(key, value));
  }

  void Parse(std::string* tmpl) {
    int start_pos, end_pos = 0;
    int ident_start_len = ident_start_.length();
    int ident_end_len = ident_end_.length();

    while(true) {
      start_pos = tmpl->find(ident_start_, end_pos);
      if (start_pos >= 0) {
        end_pos = tmpl->find(ident_end_, start_pos + ident_start_len);
        if (end_pos >= 0) {
          // Found an identifier. Check if a substitution exists.
          std::string key = tmpl->substr(start_pos + ident_start_len,
                                         end_pos - start_pos - ident_start_len);
          KeyMap::const_iterator it = values_.find(key);
          if (it != values_.end()) {
            // Peform the substitution.
            tmpl->replace(start_pos, end_pos + ident_end_len - start_pos,
                it->second);
            end_pos = start_pos + it->second.length();
          } else {
            // Leave the unknown identifier in place.
            end_pos += ident_end_len;
          }

          if (end_pos >= static_cast<int>(tmpl->length()) - ident_start_len -
                         ident_end_len) {
            // Not enough room remaining for more identifiers.
            break;
          }
        } else {
          // No end identifier found.
          break;
        }
      } else {
        // No start identifier found.
        break;
      }
    }
  }

 private:
  typedef std::map<std::string, std::string> KeyMap;
  KeyMap values_;
  std::string ident_start_;
  std::string ident_end_;
};

class Delegate : public InternalHandlerDelegate {
 public:
  Delegate() {}

  bool OnRequest(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefRequest> request,
                 Action* action) override {
    GURL url = GURL(request->GetURL().ToString());
    std::string path = url.path();
    if (path.length() > 0)
      path = path.substr(1);

    bool handled = false;

    ChromeHostId host_id = GetChromeHostId(url.host());
    switch (host_id) {
      case CHROME_LICENSE:
        handled = OnLicense(action);
        break;
      case CHROME_VERSION:
        handled = OnVersion(browser, action);
        break;
      case CHROME_WEBUI_HOSTS:
        handled = OnWebUIHosts(action);
        break;
      default:
        break;
    }

    if (!handled && host_id != CHROME_VERSION) {
      LOG(INFO) << "Reguest for unknown chrome resource: " <<
          url.spec().c_str();

      action->redirect_url =
          GURL(std::string(kChromeURL) + chrome::kChromeUIVersionHost);
      return true;
    }

    return handled;
  }

  bool OnLicense(Action* action) {
    base::StringPiece piece = CefContentClient::Get()->GetDataResource(
          IDR_CEF_LICENSE_TXT, ui::SCALE_FACTOR_NONE);
    if (piece.empty()) {
      NOTREACHED() << "Failed to load license txt resource.";
      return false;
    }

    std::string html = "<html><head><title>License</title></head><body><pre>" +
                       piece.as_string() + "</pre></body></html>";

    action->mime_type = "text/html";
    action->stream =  CefStreamReader::CreateForData(
        const_cast<char*>(html.c_str()), html.length());
    action->stream_size = html.length();

    return true;
  }

  bool OnVersion(CefRefPtr<CefBrowser> browser,
                 Action* action) {
    base::StringPiece piece = CefContentClient::Get()->GetDataResource(
          IDR_CEF_VERSION_HTML, ui::SCALE_FACTOR_NONE);
    if (piece.empty()) {
      NOTREACHED() << "Failed to load version html resource.";
      return false;
    }

    TemplateParser parser;
    parser.Add("YEAR", MAKE_STRING(COPYRIGHT_YEAR));
    parser.Add("CEF", CEF_VERSION);
    parser.Add("CHROMIUM",
        base::StringPrintf("%d.%d.%d.%d",
            CHROME_VERSION_MAJOR,
            CHROME_VERSION_MINOR,
            CHROME_VERSION_BUILD,
            CHROME_VERSION_PATCH));
    parser.Add("OS", GetOSType());
    parser.Add("WEBKIT", content::GetWebKitVersion());
    parser.Add("JAVASCRIPT", v8::V8::GetVersion());
    parser.Add("FLASH", std::string());  // Value populated asynchronously.
    parser.Add("USERAGENT", CefContentClient::Get()->GetUserAgent());
    parser.Add("COMMANDLINE", GetCommandLine());
    parser.Add("MODULEPATH", GetModulePath());
    parser.Add("CACHEPATH", browser.get() ?
                  browser->GetHost()->GetRequestContext()->GetCachePath() : "");

    std::string tmpl = piece.as_string();
    parser.Parse(&tmpl);

    action->mime_type = "text/html";
    action->stream =  CefStreamReader::CreateForData(
        const_cast<char*>(tmpl.c_str()), tmpl.length());
    action->stream_size = tmpl.length();

    return true;
  }

  bool OnWebUIHosts(Action* action) {
    std::string html = "<html>\n<head><title>WebUI Hosts</title></head>\n"
                       "<body bgcolor=\"white\"><h3>WebUI Hosts</h3>\n<ul>\n";

    std::vector<std::string> list;
    GetAllowedHosts(&list);
    std::sort(list.begin(), list.end());

    for (size_t i = 0U; i < list.size(); ++i) {
      if (IsUnlistedHost(list[i]))
        continue;

      html += "<li><a href=\"chrome://" + list[i] + "\">chrome://" +
              list[i] + "</a></li>\n";
    }

    list.clear();
    GetDebugURLs(&list);
    std::sort(list.begin(), list.end());

    html += "</ul>\n<h3>For Debug</h3>\n"
        "<p>The following pages are for debugging purposes only. Because they "
        "crash or hang the renderer, they're not linked directly; you can type "
        "them into the address bar if you need them.</p>\n<ul>\n";
    for (size_t i = 0U; i < list.size(); ++i) {
      html += "<li>" + std::string(list[i]) + "</li>\n";
    }
    html += "</ul>\n";

    html += "</body>\n</html>";

    action->mime_type = "text/html";
    action->stream =  CefStreamReader::CreateForData(
        const_cast<char*>(html.c_str()), html.length());
    action->stream_size = html.length();

    return true;
  }
};

void DidFinishChromeVersionLoad(CefRefPtr<CefFrame> frame) {
  // Retieve Flash version information and update asynchronously.
  class Visitor : public CefWebPluginInfoVisitor {
   public:
    Visitor(CefRefPtr<CefFrame> frame)
        : frame_(frame) {
    }

    bool Visit(CefRefPtr<CefWebPluginInfo> info,
               int count, int total) override {
      std::string name = info->GetName();
      if (name == "Shockwave Flash") {
        if (frame_->IsValid()) {
          std::string version = info->GetVersion();
          frame_->ExecuteJavaScript(
              "document.getElementById('flash').innerText = '" + version + "';",
              std::string(), 0);
        }
        return false;
      }

      return true;
    }

   private:
    CefRefPtr<CefFrame> frame_;

    IMPLEMENT_REFCOUNTING(Visitor);
  };

  CefVisitWebPluginInfo(new Visitor(frame));
}

// Wrapper for a ChromeProtocolHandler instance from
// content/browser/webui/url_data_manager_backend.cc. 
class ChromeProtocolHandlerWrapper :
    public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ChromeProtocolHandlerWrapper(
      CefURLRequestManager* request_manager,
      std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_protocol_handler)
      : request_manager_(request_manager),
        chrome_protocol_handler_(std::move(chrome_protocol_handler)) {
    DCHECK(request_manager_);
  }

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    // Don't handle debug URLs.
    if (IsDebugURL(request->url()))
      return nullptr;

    // Only allow WebUI to handle chrome:// URLs whitelisted by CEF.
    if (CefWebUIControllerFactory::AllowWebUIForURL(request->url())) {
      return chrome_protocol_handler_->MaybeCreateJob(request,
                                                      network_delegate);
    }

    // Use the protocol handler registered with CEF.
    return request_manager_->GetRequestJob(request, network_delegate);
  }

 private:
  CefURLRequestManager* request_manager_;
  std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
      chrome_protocol_handler_;
};

}  // namespace

void RegisterChromeHandler(CefURLRequestManager* request_manager) {
  request_manager->AddFactory(
      content::kChromeUIScheme,
      std::string(),
      CreateInternalHandlerFactory(base::WrapUnique(new Delegate())));
}

void RegisterWebUIControllerFactory() {
  // Channel all WebUI handling through CefWebUIControllerFactory.
  content::WebUIControllerFactory::UnregisterFactoryForTesting(
      content::ContentWebUIControllerFactory::GetInstance());

  content::WebUIControllerFactory::RegisterFactory(
      CefWebUIControllerFactory::GetInstance());
}

void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) {
  CefWebUIControllerFactory::BrowserURLHandlerCreated(handler);
}

void DidFinishChromeLoad(CefRefPtr<CefFrame> frame,
                         const GURL& validated_url) {
  ChromeHostId host_id = GetChromeHostId(validated_url.host());
  switch (host_id) {
    case CHROME_VERSION:
      DidFinishChromeVersionLoad(frame);
    default:
      break;
  }
}

std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
WrapChromeProtocolHandler(
    CefURLRequestManager* request_manager,
    std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_protocol_handler) {
  std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler> ret(
      new ChromeProtocolHandlerWrapper(request_manager,
                                       std::move(chrome_protocol_handler)));
  return ret;
}

}  // namespace scheme
