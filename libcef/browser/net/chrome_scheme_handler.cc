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
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/extensions/chrome_api_registration.h"
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
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/user_agent.h"
#include "ipc/ipc_channel.h"
#include "net/url_request/url_request.h"
#include "v8/include/v8.h"

using extensions::api::cef::kSupportedAPIs;

namespace scheme {

const char kChromeURL[] = "chrome://";

namespace {

const char kChromeUIExtensionsSupportHost[] = "extensions-support";
const char kChromeUILicenseHost[] = "license";
const char kChromeUIWebUIHostsHost[] = "webui-hosts";

// TODO(network): Consider handling content::kChromeDevToolsScheme via WebUI
// (DevToolsUI class) with the following changes:
// 1. Add an entry for content::kChromeDevToolsScheme in
//    CefContentBrowserClient::GetAdditionalWebUISchemes.
// 2. Allow the scheme in CefWebUIControllerFactory::AllowWebUIForURL.
// 3. Add an entry for chrome::kChromeUIDevToolsHost in kAllowedWebUIHosts and
//    kUnlistedHosts.
// 4. Remove scheme::RegisterInternalHandlers and related plumbing.

// Chrome hosts implemented by WebUI.
// Some WebUI handlers have Chrome dependencies that may fail in CEF without
// additional changes. Do not add new hosts to this list without also manually
// testing all related functionality in CEF.
const char* kAllowedWebUIHosts[] = {
    content::kChromeUIAppCacheInternalsHost,
    chrome::kChromeUIAccessibilityHost,
    content::kChromeUIBlobInternalsHost,
    chrome::kChromeUICreditsHost,
    kChromeUIExtensionsSupportHost,
    content::kChromeUIGpuHost,
    content::kChromeUIHistogramHost,
    content::kChromeUIIndexedDBInternalsHost,
    kChromeUILicenseHost,
    content::kChromeUIMediaInternalsHost,
    chrome::kChromeUINetExportHost,
    chrome::kChromeUINetInternalsHost,
    content::kChromeUINetworkErrorHost,
    content::kChromeUINetworkErrorsListingHost,
    content::kChromeUIResourcesHost,
    content::kChromeUIServiceWorkerInternalsHost,
    chrome::kChromeUISystemInfoHost,
    content::kChromeUITracingHost,
    chrome::kChromeUIVersionHost,
    content::kChromeUIWebRTCInternalsHost,
    kChromeUIWebUIHostsHost,
};

// Hosts that don't have useful output when linked directly. They'll be excluded
// from the "chrome://webui-hosts" listing.
const char* kUnlistedHosts[] = {
    content::kChromeUINetworkErrorHost,
    content::kChromeUIResourcesHost,
};

enum ChromeHostId {
  CHROME_UNKNOWN = 0,
  CHROME_EXTENSIONS_SUPPORT,
  CHROME_LICENSE,
  CHROME_VERSION,
  CHROME_WEBUI_HOSTS,
};

// Chrome hosts implemented by CEF.
const struct {
  const char* host;
  ChromeHostId host_id;
} kAllowedCefHosts[] = {
    {kChromeUIExtensionsSupportHost, CHROME_EXTENSIONS_SUPPORT},
    {kChromeUILicenseHost, CHROME_LICENSE},
    {chrome::kChromeUIVersionHost, CHROME_VERSION},
    {kChromeUIWebUIHostsHost, CHROME_WEBUI_HOSTS},
};

ChromeHostId GetChromeHostId(const std::string& host) {
  for (size_t i = 0; i < sizeof(kAllowedCefHosts) / sizeof(kAllowedCefHosts[0]);
       ++i) {
    if (base::EqualsCaseInsensitiveASCII(kAllowedCefHosts[i].host,
                                         host.c_str())) {
      return kAllowedCefHosts[i].host_id;
    }
  }

  return CHROME_UNKNOWN;
}

// Returns WebUI hosts. Does not include chrome debug hosts (for crashing, etc).
void GetAllowedHosts(std::vector<std::string>* hosts) {
  // Explicitly whitelisted WebUI hosts.
  for (size_t i = 0;
       i < sizeof(kAllowedWebUIHosts) / sizeof(kAllowedWebUIHosts[0]); ++i) {
    hosts->push_back(kAllowedWebUIHosts[i]);
  }
}

// Returns true if a host should not be listed on "chrome://webui-hosts".
bool IsUnlistedHost(const std::string& host) {
  for (size_t i = 0; i < sizeof(kUnlistedHosts) / sizeof(kUnlistedHosts[0]);
       ++i) {
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
    if (base::EqualsCaseInsensitiveASCII(kAllowedWebUIHosts[i], host.c_str())) {
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
  for (size_t i = 0; i < chrome::kNumberOfChromeDebugURLs; ++i) {
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
  for (size_t i = 0; i < chrome::kNumberOfChromeDebugURLs; ++i) {
    urls->push_back(chrome::kChromeDebugURLs[i]);
  }

  for (size_t i = 0;
       i < sizeof(kAllowedDebugURLs) / sizeof(kAllowedDebugURLs[0]); ++i) {
    urls->push_back(kAllowedDebugURLs[i]);
  }
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
  if (base::PathService::Get(base::FILE_MODULE, &path))
    return CefString(path.value());
  return std::string();
}

class TemplateParser {
 public:
  TemplateParser() : ident_start_("$$"), ident_end_("$$") {}

  TemplateParser(const std::string& ident_start, const std::string& ident_end)
      : ident_start_(ident_start), ident_end_(ident_end) {}

  void Add(const std::string& key, const std::string& value) {
    values_.insert(std::make_pair(key, value));
  }

  void Parse(std::string* tmpl) {
    int start_pos, end_pos = 0;
    int ident_start_len = ident_start_.length();
    int ident_end_len = ident_end_.length();

    while (true) {
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

bool OnExtensionsSupportUI(std::string* mime_type, std::string* output) {
  static const char kDevURL[] = "https://developer.chrome.com/extensions/";

  std::string html =
      "<html>\n<head><title>Extensions Support</title></head>\n"
      "<body bgcolor=\"white\"><h3>Supported Chrome Extensions "
      "APIs</h3>\nFollow <a "
      "href=\"https://bitbucket.org/chromiumembedded/cef/issues/1947\" "
      "target=\"new\">issue #1947</a> for development progress.\n<ul>\n";

  bool has_top_level_name = false;
  for (size_t i = 0; kSupportedAPIs[i] != nullptr; ++i) {
    const std::string& api_name = kSupportedAPIs[i];
    if (api_name.find("Private") != std::string::npos) {
      // Don't list private APIs.
      continue;
    }

    const size_t dot_pos = api_name.find('.');
    if (dot_pos == std::string::npos) {
      if (has_top_level_name) {
        // End the previous top-level API entry.
        html += "</ul></li>\n";
      } else {
        has_top_level_name = true;
      }

      // Start a new top-level API entry.
      html += "<li><a href=\"" + std::string(kDevURL) + api_name +
              "\" target=\"new\">" + api_name + "</a><ul>\n";
    } else {
      // Function name.
      const std::string& group_name = api_name.substr(0, dot_pos);
      const std::string& function_name = api_name.substr(dot_pos + 1);
      html += "\t<li><a href=\"" + std::string(kDevURL) + group_name +
              "#method-" + function_name + "\" target=\"new\">" + api_name +
              "</a></li>\n";
    }
  }

  if (has_top_level_name) {
    // End the last top-level API entry.
    html += "</ul></li>\n";
  }

  html += "</ul>\n</body>\n</html>";

  *mime_type = "text/html";
  *output = html;

  return true;
}

bool OnLicenseUI(std::string* mime_type, std::string* output) {
  base::StringPiece piece = CefContentClient::Get()->GetDataResource(
      IDR_CEF_LICENSE_TXT, ui::SCALE_FACTOR_NONE);
  if (piece.empty()) {
    NOTREACHED() << "Failed to load license txt resource.";
    return false;
  }

  *mime_type = "text/html";
  *output = "<html><head><title>License</title></head><body><pre>" +
            piece.as_string() + "</pre></body></html>";

  return true;
}

bool OnVersionUI(Profile* profile,
                 std::string* mime_type,
                 std::string* output) {
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
             base::StringPrintf("%d.%d.%d.%d", CHROME_VERSION_MAJOR,
                                CHROME_VERSION_MINOR, CHROME_VERSION_BUILD,
                                CHROME_VERSION_PATCH));
  parser.Add("OS", GetOSType());
  parser.Add("WEBKIT", content::GetWebKitVersion());
  parser.Add("JAVASCRIPT", v8::V8::GetVersion());
  parser.Add("FLASH", std::string());  // Value populated asynchronously.
  parser.Add("USERAGENT", CefContentClient::Get()->browser()->GetUserAgent());
  parser.Add("COMMANDLINE", GetCommandLine());
  parser.Add("MODULEPATH", GetModulePath());
  parser.Add("CACHEPATH", CefString(profile->GetPath().value()));

  std::string tmpl = piece.as_string();
  parser.Parse(&tmpl);

  *mime_type = "text/html";
  *output = tmpl;

  return true;
}

bool OnWebUIHostsUI(std::string* mime_type, std::string* output) {
  std::string html =
      "<html>\n<head><title>WebUI Hosts</title></head>\n"
      "<body bgcolor=\"white\"><h3>WebUI Hosts</h3>\n<ul>\n";

  std::vector<std::string> list;
  GetAllowedHosts(&list);
  std::sort(list.begin(), list.end());

  for (size_t i = 0U; i < list.size(); ++i) {
    if (IsUnlistedHost(list[i]))
      continue;

    html += "<li><a href=\"chrome://" + list[i] + "\">chrome://" + list[i] +
            "</a></li>\n";
  }

  list.clear();
  GetDebugURLs(&list);
  std::sort(list.begin(), list.end());

  html +=
      "</ul>\n<h3>For Debug</h3>\n"
      "<p>The following pages are for debugging purposes only. Because they "
      "crash or hang the renderer, they're not linked directly; you can type "
      "them into the address bar if you need them.</p>\n<ul>\n";
  for (size_t i = 0U; i < list.size(); ++i) {
    html += "<li>" + std::string(list[i]) + "</li>\n";
  }
  html += "</ul>\n";

  html += "</body>\n</html>";

  *mime_type = "text/html";
  *output = html;

  return true;
}

const content::WebUI::TypeID kCefWebUITypeID = &kCefWebUITypeID;

class CefURLDataSource : public content::URLDataSource {
 public:
  CefURLDataSource(const std::string& host,
                   ChromeHostId host_id,
                   Profile* profile)
      : host_(host), host_id_(host_id), profile_(profile) {
    CEF_REQUIRE_UIT();
    output_ = new base::RefCountedString();
    bool handled = false;
    switch (host_id_) {
      case CHROME_EXTENSIONS_SUPPORT:
        handled = OnExtensionsSupportUI(&mime_type_, &output_->data());
        break;
      case CHROME_LICENSE:
        handled = OnLicenseUI(&mime_type_, &output_->data());
        break;
      case CHROME_VERSION:
        handled = OnVersionUI(profile_, &mime_type_, &output_->data());
        break;
      case CHROME_WEBUI_HOSTS:
        handled = OnWebUIHostsUI(&mime_type_, &output_->data());
        break;
      default:
        break;
    }
    DCHECK(handled) << "Unhandled WebUI host: " << host;
  }

  ~CefURLDataSource() override = default;

  // content::URLDataSource implementation.
  std::string GetSource() const override { return host_; }

  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override {
    callback.Run(output_);
  }

  std::string GetMimeType(const std::string& path) const override {
    return mime_type_;
  }

  bool AllowCaching() const override { return false; }

 private:
  const std::string host_;
  const ChromeHostId host_id_;
  Profile* const profile_;

  std::string mime_type_;
  scoped_refptr<base::RefCountedString> output_;

  DISALLOW_COPY_AND_ASSIGN(CefURLDataSource);
};

class CefWebUIController : public content::WebUIController {
 public:
  CefWebUIController(content::WebUI* web_ui,
                     const std::string& host,
                     ChromeHostId host_id)
      : content::WebUIController(web_ui) {
    Profile* profile = Profile::FromWebUI(web_ui);
    content::URLDataSource::Add(
        profile, std::make_unique<CefURLDataSource>(host, host_id, profile));
  }

  ~CefWebUIController() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefWebUIController);
};

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

  // Returns true if WebUI is allowed to make network requests.
  static bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) {
    if (!AllowWebUIForURL(origin.GetURL()))
      return false;

    if (ChromeWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(
            origin)) {
      return true;
    }

    return false;
  }

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) const override {
    std::unique_ptr<content::WebUIController> controller;
    if (!AllowWebUIForURL(url))
      return controller;

    const auto host_id = GetChromeHostId(url.host());
    if (host_id != CHROME_UNKNOWN) {
      return std::make_unique<CefWebUIController>(web_ui, url.host(), host_id);
    }

    controller = content::ContentWebUIControllerFactory::GetInstance()
                     ->CreateWebUIControllerForURL(web_ui, url);
    if (controller.get())
      return controller;

    return ChromeWebUIControllerFactory::GetInstance()
        ->CreateWebUIControllerForURL(web_ui, url);
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) const override {
    content::WebUI::TypeID type = content::WebUI::kNoWebUI;
    if (!AllowWebUIForURL(url))
      return type;

    const auto host_id = GetChromeHostId(url.host());
    if (host_id != CHROME_UNKNOWN) {
      return kCefWebUITypeID;
    }

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

    const auto host_id = GetChromeHostId(url.host());
    if (host_id != CHROME_UNKNOWN) {
      return true;
    }

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

    const auto host_id = GetChromeHostId(url.host());
    if (host_id != CHROME_UNKNOWN) {
      // TODO(network): Use WebUI bindings to implement DidFinishChromeLoad.
      return false;
    }

    if (content::ContentWebUIControllerFactory::GetInstance()
            ->UseWebUIBindingsForURL(browser_context, url) ||
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

void DidFinishChromeVersionLoad(CefRefPtr<CefFrame> frame) {
  // Retieve Flash version information and update asynchronously.
  class Visitor : public CefWebPluginInfoVisitor {
   public:
    Visitor(CefRefPtr<CefFrame> frame) : frame_(frame) {}

    bool Visit(CefRefPtr<CefWebPluginInfo> info,
               int count,
               int total) override {
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
class ChromeProtocolHandlerWrapper
    : public net::URLRequestJobFactory::ProtocolHandler {
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

void DidFinishChromeLoad(CefRefPtr<CefFrame> frame, const GURL& validated_url) {
  ChromeHostId host_id = GetChromeHostId(validated_url.host());
  switch (host_id) {
    case CHROME_VERSION:
      DidFinishChromeVersionLoad(frame);
      break;
    default:
      break;
  }
}

bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) {
  return CefWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(origin);
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
