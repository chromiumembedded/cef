// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/chrome_scheme_handler.h"
#include <map>
#include <string>
#include "include/cef_version.h"
#include "include/cef_web_plugin.h"
#include "libcef/browser/context.h"
#include "libcef/browser/internal_scheme_handler.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/public/common/content_client.h"
#include "grit/cef_resources.h"
#include "v8/include/v8.h"
#include "webkit/user_agent/user_agent_util.h"

namespace scheme {

const char kChromeScheme[] = "chrome";
const char kChromeURL[] = "chrome://";

namespace {

const char kChromeCreditsDomain[] = "credits";
const char kChromeLicenseDomain[] = "license";
const char kChromeVersionDomain[] = "version";

enum ChromeDomain {
  CHROME_UNKNOWN = 0,
  CHROME_CREDITS,
  CHROME_LICENSE,
  CHROME_VERSION,
};

ChromeDomain GetChromeDomain(const std::string& domain_name) {
  static struct {
    const char* name;
    ChromeDomain domain;
  } domains[] = {
    { kChromeCreditsDomain, CHROME_CREDITS },
    { kChromeLicenseDomain, CHROME_LICENSE },
    { kChromeVersionDomain, CHROME_VERSION },
  };

  for (size_t i = 0; i < sizeof(domains) / sizeof(domains[0]); ++i) {
    if (base::strcasecmp(domains[i].name, domain_name.c_str()) == 0)
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
  return WideToUTF8(CommandLine::ForCurrentProcess()->GetCommandLineString());
#elif defined(OS_POSIX)
  std::string command_line = "";
  typedef std::vector<std::string> ArgvList;
  const ArgvList& argv = CommandLine::ForCurrentProcess()->argv();
  for (ArgvList::const_iterator iter = argv.begin(); iter != argv.end(); iter++)
    command_line += " " + *iter;
  // TODO(viettrungluu): |command_line| could really have any encoding, whereas
  // below we assumes it's UTF-8.
  return command_line;
#endif
}

std::string GetModulePath() {
  FilePath path;
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

  virtual bool OnRequest(CefRefPtr<CefRequest> request,
                         Action* action) OVERRIDE {
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
      case CHROME_VERSION:
        handled = OnVersion(action);
        break;
      default:
        break;
    }

    if (!handled && domain != CHROME_VERSION) {
      action->redirect_url =
          GURL(std::string(kChromeURL) + kChromeVersionDomain);
    }

    return true;
  }

  bool OnCredits(const std::string& path, Action* action) {
    if (path == "credits.js") {
      action->resource_id = IDR_CEF_CREDITS_JS;
    } else if (path == "swiftshader.jpg") {
      action->resource_id = IDR_CEF_CREDITS_SWIFTSHADER_JPG;
    } else {
      action->mime_type = "text/html";
      action->resource_id = IDR_CEF_CREDITS_HTML;
    }
    return true;
  }

  bool OnLicense(Action* action) {
    base::StringPiece piece = content::GetContentClient()->GetDataResource(
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

  bool OnVersion(Action* action) {
    base::StringPiece piece = content::GetContentClient()->GetDataResource(
          IDR_CEF_VERSION_HTML, ui::SCALE_FACTOR_NONE);
    if (piece.empty()) {
      NOTREACHED() << "Failed to load version html resource.";
      return false;
    }

    TemplateParser parser;
    parser.Add("YEAR", MAKE_STRING(COPYRIGHT_YEAR));
    parser.Add("CEF",
        base::StringPrintf("%d.%d.%d",
            CEF_VERSION_MAJOR,
            CHROME_VERSION_BUILD,
            CEF_REVISION));
    parser.Add("CHROMIUM",
        base::StringPrintf("%d.%d.%d.%d",
            CHROME_VERSION_MAJOR,
            CHROME_VERSION_MINOR,
            CHROME_VERSION_BUILD,
            CHROME_VERSION_PATCH));
    parser.Add("OS", GetOSType());
    parser.Add("WEBKIT",
        base::StringPrintf("%d.%d",
            webkit_glue::GetWebKitMajorVersion(),
            webkit_glue::GetWebKitMinorVersion()));
    parser.Add("JAVASCRIPT", v8::V8::GetVersion());
    parser.Add("FLASH", std::string());  // Value populated asynchronously.
    parser.Add("USERAGENT", content::GetUserAgent(GURL()));
    parser.Add("COMMANDLINE", GetCommandLine());
    parser.Add("MODULEPATH", GetModulePath());
    parser.Add("CACHEPATH", CefString(_Context->cache_path().value()));

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

    virtual bool Visit(CefRefPtr<CefWebPluginInfo> info,
                       int count, int total) OVERRIDE {
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

}  // namespace

void RegisterChromeHandler() {
  CefRegisterSchemeHandlerFactory(
      kChromeScheme,
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

}  // namespace scheme
