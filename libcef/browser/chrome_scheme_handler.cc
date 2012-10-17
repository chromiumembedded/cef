// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome_scheme_handler.h"

#include <map>
#include <string>

#include "include/cef_trace.h"
#include "include/cef_version.h"
#include "include/cef_web_plugin.h"
#include "libcef/browser/context.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/internal_scheme_handler.h"
#include "libcef/browser/thread_util.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/content_client.h"
#include "grit/cef_resources.h"
#include "ipc/ipc_channel.h"
#include "v8/include/v8.h"
#include "webkit/user_agent/user_agent_util.h"

namespace scheme {

const char kChromeScheme[] = "chrome";
const char kChromeURL[] = "chrome://";
const char kChromeProcessMessage[] = "chrome.send";

namespace {

const char kChromeCreditsDomain[] = "credits";
const char kChromeLicenseDomain[] = "license";
const char kChromeTracingDomain[] = "tracing";
const char kChromeVersionDomain[] = "version";

enum ChromeDomain {
  CHROME_UNKNOWN = 0,
  CHROME_CREDITS,
  CHROME_LICENSE,
  CHROME_TRACING,
  CHROME_VERSION,
};

ChromeDomain GetChromeDomain(const std::string& domain_name) {
  static struct {
    const char* name;
    ChromeDomain domain;
  } domains[] = {
    { kChromeCreditsDomain, CHROME_CREDITS },
    { kChromeLicenseDomain, CHROME_LICENSE },
    { kChromeTracingDomain, CHROME_TRACING },
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
      case CHROME_TRACING:
        handled = OnTracing(path, action);
        break;
      case CHROME_VERSION:
        handled = OnVersion(action);
        break;
      default:
        break;
    }

    if (!handled && domain != CHROME_VERSION) {
      DLOG(INFO) << "Reguest for unknown chrome resource: " <<
          url.spec().c_str();
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

  bool OnTracing(const std::string& path, Action* action) {
    if (path == "tracing.js") {
      action->resource_id = IDR_CEF_TRACING_JS;
    } else {
      action->mime_type = "text/html";
      action->resource_id = IDR_CEF_TRACING_HTML;
    }
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

// Test that the tracing browser window hasn't closed or navigated elsewhere.
bool IsTraceFrameValid(CefRefPtr<CefFrameHostImpl> frame) {
  if (!frame->IsValid())
    return false;

  std::string tracing_url(kChromeURL);
  tracing_url += kChromeTracingDomain;

  std::string url = frame->GetURL();
  if (url.find(tracing_url.c_str()) != 0)
    return false;

  return true;
}

void LoadTraceFile(CefRefPtr<CefFrameHostImpl> frame, const FilePath& path) {
  CEF_REQUIRE_FILET();

  if (!IsTraceFrameValid(frame))
    return;

  std::string file_contents;
  if (!file_util::ReadFileToString(path, &file_contents)) {
    frame->SendJavaScript(
      "tracingController.onLoadTraceFileCanceled();", std::string(), 0);
    return;
  }

  // We need to escape the file contents, because it will go into a javascript
  // quoted string in LoadTraceFileSuccess. We need to escape control characters
  // (to have well-formed javascript statements), as well as \ and ' (the only
  // special characters in a ''-quoted string). Do the escaping on this thread,
  // it may take a little while for big files and we don't want to block the UI
  // during that time. Also do the UTF-16 conversion here.
  // Note: we're using UTF-16 because we'll need to cut the string into slices
  // to give to Javascript, and it's easier to cut than UTF-8 (since JS strings
  // are arrays of 16-bit values, UCS-2 really, whereas we can't cut inside of a
  // multibyte UTF-8 codepoint).
  size_t size = file_contents.size();
  std::string escaped_contents;
  escaped_contents.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    char c = file_contents[i];
    if (c < ' ') {
      escaped_contents += base::StringPrintf("\\u%04x", c);
      continue;
    }
    if (c == '\\' || c == '\'')
      escaped_contents.push_back('\\');
    escaped_contents.push_back(c);
  }
  file_contents.clear();

  const string16& contents16 = UTF8ToUTF16(escaped_contents);

  // We need to pass contents to tracingController.onLoadTraceFileComplete, but
  // that may be arbitrarily big, and IPC messages are limited in size. So we
  // need to cut it into pieces and rebuild the string in Javascript.
  // IPC::Channel::kMaximumMessageSize is in bytes, and we need to account for
  // overhead.
  static const size_t kMaxSize = IPC::Channel::kMaximumMessageSize / 2 - 128;
  const string16& first_prefix = ASCIIToUTF16("window.traceData = '");
  const string16& prefix = ASCIIToUTF16("window.traceData += '");
  const string16& suffix = ASCIIToUTF16("';");

  for (size_t i = 0; i < contents16.size(); i += kMaxSize) {
    string16 javascript = i == 0 ? first_prefix : prefix;
    javascript += contents16.substr(i, kMaxSize) + suffix;
    frame->SendJavaScript(UTF16ToUTF8(javascript), std::string(), 0);
  }

  frame->SendJavaScript(
      "tracingController.onLoadTraceFileComplete(JSON.parse(window.traceData));"
      "delete window.traceData;",
      std::string(), 0);
}

void SaveTraceFile(CefRefPtr<CefFrameHostImpl> frame,
                   const FilePath& path,
                   scoped_ptr<std::string> contents) {
  CEF_REQUIRE_FILET();

  if (!IsTraceFrameValid(frame))
    return;

  if (file_util::WriteFile(path, contents->c_str(), contents->size())) {
    frame->SendJavaScript(
        "tracingController.onSaveTraceFileComplete();",
        std::string(), 0);
  } else {
    frame->SendJavaScript(
        "tracingController.onSaveTraceFileCanceled();",
        std::string(), 0);
  }
}

void OnChromeTracingProcessMessage(CefRefPtr<CefBrowser> browser,
                                   const std::string& action,
                                   const base::ListValue* arguments) {
  CefRefPtr<CefFrameHostImpl> frame =
      static_cast<CefFrameHostImpl*>(browser->GetMainFrame().get());

  if (action == "tracingControllerInitialized") {
    // Send the client info to the tracingController.
  } else if (action == "beginTracing") {
    if (!arguments || arguments->GetSize() != 2) {
      NOTREACHED() << "Invalid arguments to " << action.c_str();
      return;
    }

    class Client : public CefTraceClient {
     public:
      explicit Client(CefRefPtr<CefFrameHostImpl> frame)
          : frame_(frame),
            ended_(false) {
      }

      virtual void OnTraceDataCollected(const char* fragment,
                                        size_t fragment_size) OVERRIDE {
        const std::string& prefix = "tracingController.onTraceDataCollected([";
        const std::string& suffix = "]);";

        std::string str;
        str.reserve(prefix.size() + fragment_size + suffix.size() + 1);
        str.append(prefix);
        str.append(fragment, fragment_size);
        str.append(suffix);
        Execute(str);
      }

      virtual void OnTraceBufferPercentFullReply(float percent_full) OVERRIDE {
        Execute(base::StringPrintf(
            "tracingController.onRequestBufferPercentFullComplete(%f);",
            percent_full));
      }

      virtual void OnEndTracingComplete() OVERRIDE {
        ended_ = true;
        Execute("tracingController.onEndTracingComplete();");
      }

     private:
      void Execute(const std::string& code) {
        if (!IsTraceFrameValid(frame_) && !ended_) {
          ended_ = true;
          CefEndTracingAsync();
          return;
        }
        frame_->SendJavaScript(code, std::string(), 0);
      }

      CefRefPtr<CefFrameHostImpl> frame_;
      bool ended_;
      IMPLEMENT_REFCOUNTING(Callback);
    };

    std::string categories;
    arguments->GetString(1, &categories);

    CefRefPtr<CefTraceClient> client = new Client(frame);

    // Tracing may already be running, in which case the previous client will
    // continue handling it.
    CefBeginTracing(client, categories);
  } else if (action == "endTracingAsync") {
    if (!CefEndTracingAsync()) {
      // We weren't really tracing to begin with.
      frame->SendJavaScript(
          "tracingController.onEndTracingComplete();",
          std::string(), 0);
    }
  } else if (action == "beginRequestBufferPercentFull") {
    CefGetTraceBufferPercentFullAsync();
  } else if (action == "loadTraceFile") {
    class Callback : public CefRunFileDialogCallback {
     public:
      explicit Callback(CefRefPtr<CefFrameHostImpl> frame)
          : frame_(frame) {
      }

      virtual void OnFileDialogDismissed(
          CefRefPtr<CefBrowserHost> browser_host,
          const std::vector<CefString>& file_paths) OVERRIDE {
        if (!IsTraceFrameValid(frame_))
          return;

        if (!file_paths.empty()) {
          CEF_POST_TASK(CEF_FILET,
              base::Bind(LoadTraceFile, frame_,
                         FilePath(file_paths.front())));
        } else {
          frame_->SendJavaScript(
              "tracingController.onLoadTraceFileCanceled();",
              std::string(), 0);
        }
      }

     private:
      CefRefPtr<CefFrameHostImpl> frame_;
      IMPLEMENT_REFCOUNTING(Callback);
    };

    browser->GetHost()->RunFileDialog(FILE_DIALOG_OPEN, CefString(),
        CefString(), std::vector<CefString>(), new Callback(frame));
  } else if (action == "saveTraceFile") {
    if (!arguments || arguments->GetSize() != 1) {
      NOTREACHED() << "Invalid arguments to " << action.c_str();
      return;
    }

    class Callback : public CefRunFileDialogCallback {
      public:
      Callback(CefRefPtr<CefFrameHostImpl> frame,
               scoped_ptr<std::string> contents)
          : frame_(frame),
            contents_(contents.Pass()) {
      }

      virtual void OnFileDialogDismissed(
          CefRefPtr<CefBrowserHost> browser_host,
          const std::vector<CefString>& file_paths) OVERRIDE {
        if (!IsTraceFrameValid(frame_))
          return;

        if (!file_paths.empty()) {
          CEF_POST_TASK(CEF_FILET,
              base::Bind(SaveTraceFile, frame_,
                          FilePath(file_paths.front()),
                          base::Passed(contents_.Pass())));
        } else {
          frame_->SendJavaScript(
              "tracingController.onSaveTraceFileCanceled();",
              std::string(), 0);
        }
      }

      private:
      CefRefPtr<CefFrameHostImpl> frame_;
      scoped_ptr<std::string> contents_;
      IMPLEMENT_REFCOUNTING(Callback);
    };

    std::string contents_str;
    arguments->GetString(0, &contents_str);

    scoped_ptr<std::string> contents(new std::string());
    contents_str.swap(*contents);

    browser->GetHost()->RunFileDialog(FILE_DIALOG_SAVE, CefString(),
        CefString(), std::vector<CefString>(),
        new Callback(frame, contents.Pass()));
  } else {
    NOTREACHED() << "Unknown trace action: " << action.c_str();
  }
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

void OnChromeProcessMessage(CefRefPtr<CefBrowser> browser,
                            const base::ListValue& arguments) {
  std::string action;
  const base::ListValue* args = NULL;

  size_t size = arguments.GetSize();
  if (size > 0) {
    arguments.GetString(0, &action);
    if (size > 1)
      arguments.GetList(1, &args);
  }

  GURL url = GURL(browser->GetMainFrame()->GetURL().ToString());
  ChromeDomain domain = GetChromeDomain(url.host());
  switch (domain) {
    case CHROME_TRACING:
      OnChromeTracingProcessMessage(browser, action, args);
    default:
      break;
  }
}

}  // namespace scheme
