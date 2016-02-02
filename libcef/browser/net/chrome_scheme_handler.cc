// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/chrome_scheme_handler.h"

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
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/net/view_http_cache_job_factory.h"
#include "content/browser/net/view_blob_internals_job_factory.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "grit/cef_resources.h"
#include "grit/components_resources.h"
#include "ipc/ipc_channel.h"
#include "net/url_request/url_request.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/resources/grit/webui_resources_map.h"
#include "v8/include/v8.h"

namespace scheme {

const char kChromeURL[] = "chrome://";

namespace {

const char kChromeCreditsDomain[] = "credits";
const char kChromeLicenseDomain[] = "license";
const char kChromeResourcesDomain[] = "resources";
const char kChromeVersionDomain[] = "version";

enum ChromeDomain {
  CHROME_UNKNOWN = 0,
  CHROME_CREDITS,
  CHROME_LICENSE,
  CHROME_RESOURCES,
  CHROME_VERSION,
};

ChromeDomain GetChromeDomain(const std::string& domain_name) {
  static struct {
    const char* name;
    ChromeDomain domain;
  } domains[] = {
    { kChromeCreditsDomain, CHROME_CREDITS },
    { kChromeLicenseDomain, CHROME_LICENSE },
    { kChromeResourcesDomain, CHROME_RESOURCES },
    { kChromeVersionDomain, CHROME_VERSION },
  };

  for (size_t i = 0; i < sizeof(domains) / sizeof(domains[0]); ++i) {
    if (base::EqualsCaseInsensitiveASCII(domains[i].name, domain_name.c_str()))
      return domains[i].domain;
  }

  return CHROME_UNKNOWN;
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

// From content/browser/webui/shared_resources_data_source.cc.
using ResourcesMap = base::hash_map<std::string, int>;

// TODO(rkc): Once we have a separate source for apps, remove '*/apps/' aliases.
const char* kPathAliases[][2] = {
    {"../../../third_party/polymer/v1_0/components-chromium/", "polymer/v1_0/"},
    {"../../../third_party/web-animations-js/sources/",
     "polymer/v1_0/web-animations-js/"},
    {"../../views/resources/default_100_percent/common/", "images/apps/"},
    {"../../views/resources/default_200_percent/common/", "images/2x/apps/"},
    {"../../webui/resources/cr_elements/", "cr_elements/"}};

void AddResource(const std::string& path,
                 int resource_id,
                 ResourcesMap* resources_map) {
  if (!resources_map->insert(std::make_pair(path, resource_id)).second)
    NOTREACHED() << "Redefinition of '" << path << "'";
}

const ResourcesMap* CreateResourcesMap() {
  ResourcesMap* result = new ResourcesMap();
  for (size_t i = 0; i < kWebuiResourcesSize; ++i) {
    const std::string resource_name = kWebuiResources[i].name;
    const int resource_id = kWebuiResources[i].value;
    AddResource(resource_name, resource_id, result);
    for (const char* (&alias)[2]: kPathAliases) {
      if (base::StartsWith(resource_name, alias[0],
                           base::CompareCase::SENSITIVE)) {
        AddResource(alias[1] + resource_name.substr(strlen(alias[0])),
                    resource_id, result);
      }
    }
  }

  return result;
}

const ResourcesMap& GetResourcesMap() {
  // This pointer will be intentionally leaked on shutdown.
  static const ResourcesMap* resources_map = CreateResourcesMap();
  return *resources_map;
}

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

    ChromeDomain domain = GetChromeDomain(url.host());
    switch (domain) {
      case CHROME_CREDITS:
        handled = OnCredits(path, action);
        break;
      case CHROME_LICENSE:
        handled = OnLicense(action);
        break;
      case CHROME_RESOURCES:
        handled = OnResources(path, action);
        break;
      case CHROME_VERSION:
        handled = OnVersion(browser, action);
        break;
      default:
        break;
    }

    if (!handled && domain != CHROME_VERSION) {
      LOG(INFO) << "Reguest for unknown chrome resource: " <<
          url.spec().c_str();

      if (domain != CHROME_RESOURCES) {
        action->redirect_url =
            GURL(std::string(kChromeURL) + kChromeVersionDomain);
        return true;
      }
    }

    return handled;
  }

  bool OnCredits(const std::string& path, Action* action) {
    if (path == "credits.js") {
      action->resource_id = IDR_ABOUT_UI_CREDITS_JS;
    } else {
      action->mime_type = "text/html";
      action->resource_id = IDR_ABOUT_UI_CREDITS_HTML;
    }
    return true;
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

  bool OnResources(const std::string& path, Action* action) {
    // Implementation based on SharedResourcesDataSource::StartDataRequest.
    const ResourcesMap& resources_map = GetResourcesMap();
    auto it = resources_map.find(path);
    int resource_id = (it != resources_map.end()) ? it->second : -1;

    if (resource_id == -1) {
      NOTREACHED() << "Failed to find resource id for " << path;
      return false;
    }

    if (resource_id == IDR_WEBUI_CSS_TEXT_DEFAULTS ||
        resource_id == IDR_WEBUI_CSS_TEXT_DEFAULTS_MD) {
      const std::string& css = resource_id == IDR_WEBUI_CSS_TEXT_DEFAULTS ?
          webui::GetWebUiCssTextDefaults() : webui::GetWebUiCssTextDefaultsMd();
      DCHECK(!css.empty());
      action->mime_type = "text/css";
      action->stream =  CefStreamReader::CreateForData(
          const_cast<char*>(css.c_str()), css.length());
      action->stream_size = css.length();
    } else {
      action->resource_id = resource_id;
    }

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
    parser.Add("WEBKIT",
        base::StringPrintf("%d.%d",
            content::GetWebKitMajorVersion(),
            content::GetWebKitMinorVersion()));
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
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_protocol_handler)
      : request_manager_(request_manager),
        chrome_protocol_handler_(std::move(chrome_protocol_handler)) {
    DCHECK(request_manager_);
  }

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    // Keep synchronized with the checks in
    // ChromeProtocolHandler::MaybeCreateJob.
    if (content::ViewHttpCacheJobFactory::IsSupportedURL(request->url()) ||
        (request->url().SchemeIs(content::kChromeUIScheme) &&
         request->url().host() == content::kChromeUIAppCacheInternalsHost) ||
        content::ViewBlobInternalsJobFactory::IsSupportedURL(request->url()) ||
        (request->url().SchemeIs(content::kChromeUIScheme) &&
         request->url().host() == content::kChromeUIHistogramHost)) {
      return chrome_protocol_handler_->MaybeCreateJob(request,
                                                      network_delegate);
    }

    // Use the protocol handler registered with CEF.
    return request_manager_->GetRequestJob(request, network_delegate);
  }

 private:
  CefURLRequestManager* request_manager_;
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
      chrome_protocol_handler_;
};

}  // namespace

void RegisterChromeHandler(CefURLRequestManager* request_manager) {
  request_manager->AddFactory(
      content::kChromeUIScheme,
      std::string(),
      CreateInternalHandlerFactory(
          make_scoped_ptr<InternalHandlerDelegate>(new Delegate())));
}

bool WillHandleBrowserAboutURL(GURL* url,
                               content::BrowserContext* browser_context) {
  std::string text = url->possibly_invalid_spec();
  if (text.find("about:") == 0 && text != "about:blank" && text.length() > 6) {
    // Redirect about: URLs to chrome://
    *url = GURL(kChromeURL + text.substr(6));
  }

  // Allow the redirection to proceed.
  return false;
}

void DidFinishChromeLoad(CefRefPtr<CefFrame> frame,
                         const GURL& validated_url) {
  ChromeDomain domain = GetChromeDomain(validated_url.host());
  switch (domain) {
    case CHROME_VERSION:
      DidFinishChromeVersionLoad(frame);
    default:
      break;
  }
}

scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
WrapChromeProtocolHandler(
    CefURLRequestManager* request_manager,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_protocol_handler) {
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler> ret(
      new ChromeProtocolHandlerWrapper(request_manager,
                                       std::move(chrome_protocol_handler)));
  return ret;
}

}  // namespace scheme
