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
#include "libcef/browser/extensions/chrome_api_registration.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/net/internal_scheme_handler.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/app_manager.h"
#include "libcef/features/runtime.h"

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
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_untrusted_web_ui_configs.h"
#include "chrome/browser/ui/webui/chrome_web_ui_configs.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/user_agent.h"
#include "ipc/ipc_channel.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "ui/base/resource/resource_bundle.h"
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
//    AlloyContentBrowserClient::GetAdditionalWebUISchemes.
// 2. Allow the scheme in CefWebUIControllerFactory::AllowWebUIForURL.
// 3. Add an entry for chrome::kChromeUIDevToolsHost in kAllowedWebUIHosts and
//    kUnlistedHosts.
// 4. Remove scheme::RegisterInternalHandlers and related plumbing.

// Chrome hosts implemented by WebUI.
// Some WebUI handlers have Chrome dependencies that may fail in CEF without
// additional changes. Do not add new hosts to this list without also manually
// testing all related functionality in CEF.
const char* kAllowedWebUIHosts[] = {
    chrome::kChromeUIAccessibilityHost,
    content::kChromeUIBlobInternalsHost,
    chrome::kChromeUIChromeURLsHost,
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
    chrome::kChromeUIPrintHost,
    content::kChromeUIProcessInternalsHost,
    content::kChromeUIResourcesHost,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    chrome::kChromeUISandboxHost,
#endif
    content::kChromeUIServiceWorkerInternalsHost,
    chrome::kChromeUISystemInfoHost,
    chrome::kChromeUIThemeHost,
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
    chrome::kChromeUIThemeHost,
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
    {chrome::kChromeUIChromeURLsHost, CHROME_WEBUI_HOSTS},
    {kChromeUIExtensionsSupportHost, CHROME_EXTENSIONS_SUPPORT},
    {kChromeUILicenseHost, CHROME_LICENSE},
    {chrome::kChromeUIVersionHost, CHROME_VERSION},
    {kChromeUIWebUIHostsHost, CHROME_WEBUI_HOSTS},
};

ChromeHostId GetChromeHostId(const std::string& host) {
  for (auto kAllowedCefHost : kAllowedCefHosts) {
    if (base::EqualsCaseInsensitiveASCII(kAllowedCefHost.host, host.c_str())) {
      return kAllowedCefHost.host_id;
    }
  }

  return CHROME_UNKNOWN;
}

// Returns WebUI hosts. Does not include chrome debug hosts (for crashing, etc).
void GetAllowedHosts(std::vector<std::string>* hosts) {
  // Explicitly whitelisted WebUI hosts.
  for (auto& kAllowedWebUIHost : kAllowedWebUIHosts) {
    hosts->push_back(kAllowedWebUIHost);
  }
}

// Returns true if a host should not be listed on "chrome://webui-hosts".
bool IsUnlistedHost(const std::string& host) {
  for (auto& kUnlistedHost : kUnlistedHosts) {
    if (host == kUnlistedHost) {
      return true;
    }
  }
  return false;
}

// Returns true if a host is WebUI and should be allowed to load.
bool IsAllowedWebUIHost(const std::string& host) {
  // Chrome runtime allows all WebUI hosts.
  if (cef::IsChromeRuntimeEnabled()) {
    return true;
  }

  // Explicitly whitelisted WebUI hosts.
  for (auto& kAllowedWebUIHost : kAllowedWebUIHosts) {
    if (base::EqualsCaseInsensitiveASCII(kAllowedWebUIHost, host.c_str())) {
      return true;
    }
  }

  return false;
}

// Additional debug URLs that are not included in chrome::kChromeDebugURLs.
const char* kAllowedDebugURLs[] = {
    blink::kChromeUIBrowserCrashURL,
};

void GetDebugURLs(std::vector<std::string>* urls) {
  for (size_t i = 0; i < chrome::kNumberOfChromeDebugURLs; ++i) {
    urls->push_back(chrome::kChromeDebugURLs[i]);
  }

  for (auto& kAllowedDebugURL : kAllowedDebugURLs) {
    urls->push_back(kAllowedDebugURL);
  }
}

std::string GetOSType() {
#if BUILDFLAG(IS_WIN)
  return "Windows";
#elif BUILDFLAG(IS_MAC)
  return "Mac OS X";
#elif BUILDFLAG(IS_LINUX)
  return "Linux";
#else
  return "Unknown";
#endif
}

std::string GetCommandLine() {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());
#elif BUILDFLAG(IS_POSIX)
  std::string command_line = "";
  using ArgvList = std::vector<std::string>;
  const ArgvList& argv = base::CommandLine::ForCurrentProcess()->argv();
  for (ArgvList::const_iterator iter = argv.begin(); iter != argv.end();
       iter++) {
    command_line += " " + *iter;
  }
  // TODO(viettrungluu): |command_line| could really have any encoding, whereas
  // below we assumes it's UTF-8.
  return command_line;
#endif
}

std::string GetModulePath() {
  base::FilePath path;
  if (base::PathService::Get(base::FILE_MODULE, &path)) {
    return CefString(path.value());
  }
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
  using KeyMap = std::map<std::string, std::string>;
  KeyMap values_;
  std::string ident_start_;
  std::string ident_end_;
};

bool OnExtensionsSupportUI(std::string* mime_type, std::string* output) {
  *mime_type = "text/html";

  if (cef::IsChromeRuntimeEnabled()) {
    // Redirect to the Chrome documentation.
    *output =
        "<html><head>\n"
        "<meta http-equiv=\"refresh\" "
        "content=\"0;URL='https://developer.chrome.com/docs/extensions/'\"/>\n"
        "</head></html>\n";
    return true;
  }

  static const char kDevURL[] = "https://developer.chrome.com/extensions/";

  std::string html =
      "<html>\n<head><title>Extensions Support</title></head>\n"
      "<body bgcolor=\"white\"><h3>Supported Chrome Extensions "
      "APIs</h3>\nFollow <a "
      "href=\"https://github.com/chromiumembedded/cef/issues/1947\" "
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

  *output = html;
  return true;
}

bool OnLicenseUI(std::string* mime_type, std::string* output) {
  std::string piece =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_CEF_LICENSE_TXT);
  if (piece.empty()) {
    DCHECK(false) << "Failed to load license txt resource.";
    return false;
  }

  *mime_type = "text/html";
  *output = "<html><head><title>License</title></head><body><pre>" + piece +
            "</pre></body></html>";

  return true;
}

bool OnVersionUI(Profile* profile,
                 std::string* mime_type,
                 std::string* output) {
  std::string tmpl =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_CEF_VERSION_HTML);
  if (tmpl.empty()) {
    DCHECK(false) << "Failed to load version html resource.";
    return false;
  }

  base::FilePath user_data_dir =
      base::PathService::CheckedGet(chrome::DIR_USER_DATA);

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
  parser.Add(
      "USERAGENT",
      CefAppManager::Get()->GetContentClient()->browser()->GetUserAgent());
  parser.Add("COMMANDLINE", GetCommandLine());
  parser.Add("MODULEPATH", GetModulePath());
  parser.Add("ROOTCACHEPATH", CefString(user_data_dir.value()));
  parser.Add("CACHEPATH", CefString(profile->GetPath().value()));

  parser.Parse(&tmpl);

  *mime_type = "text/html";
  *output = tmpl;

  return true;
}

bool OnWebUIHostsUI(std::string* mime_type, std::string* output) {
  std::string html =
      "<html>\n<head><title>Chrome URLs</title></head>\n"
      "<body bgcolor=\"white\"><h3>List of Chrome URLs</h3>\n<ul>\n";

  std::vector<std::string> list;
  GetAllowedHosts(&list);
  std::sort(list.begin(), list.end());

  for (const auto& i : list) {
    if (IsUnlistedHost(i)) {
      continue;
    }

    html += "<li><a href=\"chrome://" + i + "\">chrome://" + i + "</a></li>\n";
  }

  list.clear();
  GetDebugURLs(&list);
  std::sort(list.begin(), list.end());

  html +=
      "</ul>\n<h3>For Debug</h3>\n"
      "<p>The following pages are for debugging purposes only. Because they "
      "crash or hang the renderer, they're not linked directly; you can type "
      "them into the address bar if you need them.</p>\n<ul>\n";
  for (const auto& i : list) {
    html += "<li>" + std::string(i) + "</li>\n";
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

  CefURLDataSource(const CefURLDataSource&) = delete;
  CefURLDataSource& operator=(const CefURLDataSource&) = delete;

  ~CefURLDataSource() override = default;

  // content::URLDataSource implementation.
  std::string GetSource() override { return host_; }

  void StartDataRequest(
      const GURL& path,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override {
    std::move(callback).Run(output_);
  }

  std::string GetMimeType(const GURL& url) override { return mime_type_; }

  bool AllowCaching() override { return false; }

 private:
  const std::string host_;
  const ChromeHostId host_id_;
  Profile* const profile_;

  std::string mime_type_;
  scoped_refptr<base::RefCountedString> output_;
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

  CefWebUIController(const CefWebUIController&) = delete;
  CefWebUIController& operator=(const CefWebUIController&) = delete;

  ~CefWebUIController() override = default;
};

// Intercepts all WebUI calls and either blocks them or forwards them to the
// Content or Chrome WebUI factory as appropriate.
class CefWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  CefWebUIControllerFactory(const CefWebUIControllerFactory&) = delete;
  CefWebUIControllerFactory& operator=(const CefWebUIControllerFactory&) =
      delete;

  // Returns true if WebUI is allowed to handle the specified |url|.
  static bool AllowWebUIForURL(const GURL& url) {
    if (cef::IsChromeRuntimeEnabled() &&
        url.SchemeIs(content::kChromeDevToolsScheme)) {
      return DevToolsUIBindings::IsValidFrontendURL(url);
    }

    if (!url.SchemeIs(content::kChromeUIScheme) &&
        !url.SchemeIs(content::kChromeUIUntrustedScheme)) {
      return false;
    }

    if (IsAllowedWebUIHost(url.host())) {
      return true;
    }

    return false;
  }

  // Returns true if WebUI is allowed to make network requests.
  static bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) {
    if (!AllowWebUIForURL(origin.GetURL())) {
      return false;
    }

    if (ChromeWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(
            origin)) {
      return true;
    }

    return false;
  }

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    std::unique_ptr<content::WebUIController> controller;
    if (!AllowWebUIForURL(url)) {
      return controller;
    }

    // Set up the chrome://theme/ source. These URLs are referenced from many
    // places (WebUI and chrome://resources which live in //ui). WebUI code
    // can live in both //content and //chrome. Since ThemeSource lives in
    // //chrome the WebUI from //content is not performing this setup despite
    // the fact that it's needed for proper handling of theme resource requests.
    // See https://crbug.com/1011280.
    Profile* profile = Profile::FromWebUI(web_ui);
    content::URLDataSource::Add(profile,
                                std::make_unique<ThemeSource>(profile));

    const auto host_id = GetChromeHostId(url.host());
    if (host_id != CHROME_UNKNOWN) {
      return std::make_unique<CefWebUIController>(web_ui, url.host(), host_id);
    }

    controller = content::WebUIConfigMap::GetInstance()
                     .controller_factory()
                     ->CreateWebUIControllerForURL(web_ui, url);
    if (controller) {
      return controller;
    }

    return ChromeWebUIControllerFactory::GetInstance()
        ->CreateWebUIControllerForURL(web_ui, url);
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    content::WebUI::TypeID type = content::WebUI::kNoWebUI;
    if (!AllowWebUIForURL(url)) {
      return type;
    }

    const auto host_id = GetChromeHostId(url.host());
    if (host_id != CHROME_UNKNOWN) {
      return kCefWebUITypeID;
    }

    type = content::WebUIConfigMap::GetInstance()
               .controller_factory()
               ->GetWebUIType(browser_context, url);
    if (type != content::WebUI::kNoWebUI) {
      return type;
    }

    type = ChromeWebUIControllerFactory::GetInstance()->GetWebUIType(
        browser_context, url);
    if (type != content::WebUI::kNoWebUI) {
      return type;
    }

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    if (!AllowWebUIForURL(url)) {
      return false;
    }

    const auto host_id = GetChromeHostId(url.host());
    if (host_id != CHROME_UNKNOWN) {
      return true;
    }

    if (content::WebUIConfigMap::GetInstance()
            .controller_factory()
            ->UseWebUIForURL(browser_context, url) ||
        ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
            browser_context, url)) {
      return true;
    }

    return false;
  }

  static void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) {
    // For Chrome runtime this is registered in
    // ChromeContentBrowserClient::BrowserURLHandlerCreated().
    if (cef::IsAlloyRuntimeEnabled()) {
      // Handler to rewrite chrome://about and chrome://sync URLs.
      handler->AddHandlerPair(&HandleChromeAboutAndChromeSyncRewrite,
                              content::BrowserURLHandler::null_handler());
    }

    // chrome: & friends. For Chrome runtime the default registration is
    // disabled is ChromeContentBrowserClient::BrowserURLHandlerCreated().
    handler->AddHandlerPair(&HandleWebUI, &HandleWebUIReverse);
  }

  static CefWebUIControllerFactory* GetInstance();

 protected:
  CefWebUIControllerFactory() = default;
  ~CefWebUIControllerFactory() override = default;

 private:
  friend struct base::LazyInstanceTraitsBase<CefWebUIControllerFactory>;

  // From chrome/browser/chrome_content_browser_client.cc

  // Handles rewriting Web UI URLs.
  static bool HandleWebUI(GURL* url, content::BrowserContext* browser_context) {
    if (!GetInstance()->UseWebUIForURL(browser_context, *url)) {
      return false;
    }

    return true;
  }

  // Reverse URL handler for Web UI.
  static bool HandleWebUIReverse(GURL* url,
                                 content::BrowserContext* browser_context) {
    // No need to actually reverse-rewrite the URL.
    return false;
  }
};

base::LazyInstance<CefWebUIControllerFactory>::Leaky
    g_web_ui_controller_factory = LAZY_INSTANCE_INITIALIZER;

// static
CefWebUIControllerFactory* CefWebUIControllerFactory::GetInstance() {
  return &g_web_ui_controller_factory.Get();
}

}  // namespace

void RegisterWebUIControllerFactory() {
  // Channel all WebUI handling through CefWebUIControllerFactory.
  content::WebUIControllerFactory::UnregisterFactoryForTesting(
      content::WebUIConfigMap::GetInstance().controller_factory());

  content::WebUIControllerFactory::RegisterFactory(
      CefWebUIControllerFactory::GetInstance());

  RegisterChromeWebUIConfigs();
  RegisterChromeUntrustedWebUIConfigs();
}

void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) {
  CefWebUIControllerFactory::BrowserURLHandlerCreated(handler);
}

bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) {
  return CefWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(origin);
}

}  // namespace scheme
