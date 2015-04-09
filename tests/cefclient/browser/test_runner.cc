// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/browser/test_runner.h"

#include <sstream>

#include "include/base/cef_bind.h"
#include "include/cef_parser.h"
#include "include/cef_task.h"
#include "include/cef_trace.h"
#include "include/cef_web_plugin.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "cefclient/browser/binding_test.h"
#include "cefclient/browser/dialog_test.h"
#include "cefclient/browser/main_context.h"
#include "cefclient/browser/resource.h"
#include "cefclient/browser/resource_util.h"
#include "cefclient/browser/root_window_manager.h"
#include "cefclient/browser/scheme_test.h"
#include "cefclient/browser/urlrequest_test.h"
#include "cefclient/browser/window_test.h"

namespace client {
namespace test_runner {

namespace {

const char kTestOrigin[] = "http://tests/";

// Replace all instances of |from| with |to| in |str|.
std::string StringReplace(const std::string& str, const std::string& from,
                          const std::string& to) {
  std::string result = str;
  std::string::size_type pos = 0;
  std::string::size_type from_len = from.length();
  std::string::size_type to_len = to.length();
  do {
    pos = result.find(from, pos);
    if (pos != std::string::npos) {
      result.replace(pos, from_len, to);
      pos += to_len;
    }
  } while (pos != std::string::npos);
  return result;
}

void RunGetSourceTest(CefRefPtr<CefBrowser> browser) {
  class Visitor : public CefStringVisitor {
   public:
    explicit Visitor(CefRefPtr<CefBrowser> browser) : browser_(browser) {}
    virtual void Visit(const CefString& string) OVERRIDE {
      std::string source = StringReplace(string, "<", "&lt;");
      source = StringReplace(source, ">", "&gt;");
      std::stringstream ss;
      ss << "<html><body bgcolor=\"white\">Source:<pre>" << source <<
            "</pre></body></html>";
      browser_->GetMainFrame()->LoadString(ss.str(), "http://tests/getsource");
    }
   private:
    CefRefPtr<CefBrowser> browser_;
    IMPLEMENT_REFCOUNTING(Visitor);
  };

  browser->GetMainFrame()->GetSource(new Visitor(browser));
}

void RunGetTextTest(CefRefPtr<CefBrowser> browser) {
  class Visitor : public CefStringVisitor {
   public:
    explicit Visitor(CefRefPtr<CefBrowser> browser) : browser_(browser) {}
    virtual void Visit(const CefString& string) OVERRIDE {
      std::string text = StringReplace(string, "<", "&lt;");
      text = StringReplace(text, ">", "&gt;");
      std::stringstream ss;
      ss << "<html><body bgcolor=\"white\">Text:<pre>" << text <<
            "</pre></body></html>";
      browser_->GetMainFrame()->LoadString(ss.str(), "http://tests/gettext");
    }
   private:
    CefRefPtr<CefBrowser> browser_;
    IMPLEMENT_REFCOUNTING(Visitor);
  };

  browser->GetMainFrame()->GetText(new Visitor(browser));
}

void RunRequestTest(CefRefPtr<CefBrowser> browser) {
  // Create a new request
  CefRefPtr<CefRequest> request(CefRequest::Create());

  // Set the request URL
  request->SetURL("http://tests/request");

  // Add post data to the request.  The correct method and content-
  // type headers will be set by CEF.
  CefRefPtr<CefPostDataElement> postDataElement(CefPostDataElement::Create());
  std::string data = "arg1=val1&arg2=val2";
  postDataElement->SetToBytes(data.length(), data.c_str());
  CefRefPtr<CefPostData> postData(CefPostData::Create());
  postData->AddElement(postDataElement);
  request->SetPostData(postData);

  // Add a custom header
  CefRequest::HeaderMap headerMap;
  headerMap.insert(
      std::make_pair("X-My-Header", "My Header Value"));
  request->SetHeaderMap(headerMap);

  // Load the request
  browser->GetMainFrame()->LoadRequest(request);
}

void RunNewWindowTest(CefRefPtr<CefBrowser> browser) {
  MainContext::Get()->GetRootWindowManager()->CreateRootWindow(
      true,             // Show controls.
      browser->GetHost()->IsWindowRenderingDisabled(),
      CefRect(),        // Use default system size.
      std::string());   // Use default URL.
}

void RunPopupWindowTest(CefRefPtr<CefBrowser> browser) {
  browser->GetMainFrame()->ExecuteJavaScript(
      "window.open('http://www.google.com');", "about:blank", 0);
}

void RunPluginInfoTest(CefRefPtr<CefBrowser> browser) {
  class Visitor : public CefWebPluginInfoVisitor {
   public:
    explicit Visitor(CefRefPtr<CefBrowser> browser)
        : browser_(browser) {
      html_ = "<html><head><title>Plugin Info Test</title></head>"
              "<body bgcolor=\"white\">"
              "\n<b>Installed plugins:</b>";
    }
    ~Visitor() {
      html_ += "\n</body></html>";

      // Load the html in the browser.
      browser_->GetMainFrame()->LoadString(html_, "http://tests/plugin_info");
    }

    virtual bool Visit(CefRefPtr<CefWebPluginInfo> info, int count, int total)
        OVERRIDE {
      html_ +=  "\n<br/><br/>Name: " + info->GetName().ToString() +
                "\n<br/>Description: " + info->GetDescription().ToString() +
                "\n<br/>Version: " + info->GetVersion().ToString() +
                "\n<br/>Path: " + info->GetPath().ToString();
      return true;
    }

   private:
    std::string html_;
    CefRefPtr<CefBrowser> browser_;
    IMPLEMENT_REFCOUNTING(Visitor);
  };

  CefVisitWebPluginInfo(new Visitor(browser));
}

void ModifyZoom(CefRefPtr<CefBrowser> browser, double delta) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::Bind(&ModifyZoom, browser, delta));
    return;
  }

  browser->GetHost()->SetZoomLevel(
      browser->GetHost()->GetZoomLevel() + delta);
}

void BeginTracing() {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::Bind(&BeginTracing));
    return;
  }

  CefBeginTracing(CefString(), NULL);
}

void EndTracing(CefRefPtr<CefBrowser> browser) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the UI thread.
    CefPostTask(TID_UI, base::Bind(&EndTracing, browser));
    return;
  }

  class Client : public CefEndTracingCallback,
                 public CefRunFileDialogCallback {
   public:
    explicit Client(CefRefPtr<CefBrowser> browser)
        : browser_(browser) {
      RunDialog();
    }

    void RunDialog() {
      static const char kDefaultFileName[] = "trace.txt";
      std::string path = MainContext::Get()->GetDownloadPath(kDefaultFileName);
      if (path.empty())
        path = kDefaultFileName;

      // Results in a call to OnFileDialogDismissed.
      browser_->GetHost()->RunFileDialog(
          FILE_DIALOG_SAVE,
          CefString(),  // title
          path,
          std::vector<CefString>(),  // accept_filters
          0,  // selected_accept_filter
          this);
    }

    virtual void OnFileDialogDismissed(
        int selected_accept_filter,
        const std::vector<CefString>& file_paths) OVERRIDE {
      if (!file_paths.empty()) {
        // File selected. Results in a call to OnEndTracingComplete.
        CefEndTracing(file_paths.front(), this);
      } else {
        // No file selected. Discard the trace data.
        CefEndTracing(CefString(), NULL);
      }
    }

    virtual void OnEndTracingComplete(
        const CefString& tracing_file) OVERRIDE {
      Alert(browser_,
            "File \"" + tracing_file.ToString() + "\" saved successfully.");
    }

   private:
    CefRefPtr<CefBrowser> browser_;

    IMPLEMENT_REFCOUNTING(Client);
  };

  new Client(browser);
}

void RunOtherTests(CefRefPtr<CefBrowser> browser) {
  browser->GetMainFrame()->LoadURL("http://tests/other_tests");
}

// Retrieve the file name and mime type based on the specified url.
bool ParseTestUrl(const std::string& url,
                  std::string* file_name,
                  std::string* mime_type) {
  // Retrieve the path component.
  CefURLParts parts;
  CefParseURL(url, parts);
  std::string file = CefString(&parts.path);
  if (file.size() < 2)
    return false;

  // Remove the leading slash.
  file = file.substr(1);

  // Verify that the file name is valid.
  for(size_t i = 0; i < file.size(); ++i) {
    const char c = file[i];
    if (!isalpha(c) && !isdigit(c) && c != '_' && c != '.')
      return false;
  }

  // Determine the mime type based on the file extension, if any.
  size_t pos = file.rfind(".");
  if (pos != std::string::npos) {
    std::string ext = file.substr(pos + 1);
    if (ext == "html")
      *mime_type = "text/html";
    else if (ext == "png")
      *mime_type = "image/png";
    else
      return false;
  } else {
    // Default to an html extension if none is specified.
    *mime_type = "text/html";
    file += ".html";
  }

  *file_name = file;
  return true;
}

}  // namespace

void RunTest(CefRefPtr<CefBrowser> browser, int id) {
  if (!browser)
    return;

  switch (id) {
    case ID_TESTS_GETSOURCE:
       RunGetSourceTest(browser);
      break;
    case ID_TESTS_GETTEXT:
      RunGetTextTest(browser);
      break;
    case ID_TESTS_WINDOW_NEW:
      RunNewWindowTest(browser);
      break;
    case ID_TESTS_WINDOW_POPUP:
      RunPopupWindowTest(browser);
      break;
    case ID_TESTS_REQUEST:
      RunRequestTest(browser);
      break;
    case ID_TESTS_PLUGIN_INFO:
      RunPluginInfoTest(browser);
      break;
    case ID_TESTS_ZOOM_IN:
      ModifyZoom(browser, 0.5);
      break;
    case ID_TESTS_ZOOM_OUT:
      ModifyZoom(browser, -0.5);
      break;
    case ID_TESTS_ZOOM_RESET:
      browser->GetHost()->SetZoomLevel(0.0);
      break;
    case ID_TESTS_TRACING_BEGIN:
      BeginTracing();
      break;
    case ID_TESTS_TRACING_END:
      EndTracing(browser);
      break;
    case ID_TESTS_PRINT:
      browser->GetHost()->Print();
      break;
    case ID_TESTS_OTHER_TESTS:
      RunOtherTests(browser);
      break;
  }
}

std::string DumpRequestContents(CefRefPtr<CefRequest> request) {
  std::stringstream ss;

  ss << "URL: " << std::string(request->GetURL());
  ss << "\nMethod: " << std::string(request->GetMethod());

  CefRequest::HeaderMap headerMap;
  request->GetHeaderMap(headerMap);
  if (headerMap.size() > 0) {
    ss << "\nHeaders:";
    CefRequest::HeaderMap::const_iterator it = headerMap.begin();
    for (; it != headerMap.end(); ++it) {
      ss << "\n\t" << std::string((*it).first) << ": " <<
          std::string((*it).second);
    }
  }

  CefRefPtr<CefPostData> postData = request->GetPostData();
  if (postData.get()) {
    CefPostData::ElementVector elements;
    postData->GetElements(elements);
    if (elements.size() > 0) {
      ss << "\nPost Data:";
      CefRefPtr<CefPostDataElement> element;
      CefPostData::ElementVector::const_iterator it = elements.begin();
      for (; it != elements.end(); ++it) {
        element = (*it);
        if (element->GetType() == PDE_TYPE_BYTES) {
          // the element is composed of bytes
          ss << "\n\tBytes: ";
          if (element->GetBytesCount() == 0) {
            ss << "(empty)";
          } else {
            // retrieve the data.
            size_t size = element->GetBytesCount();
            char* bytes = new char[size];
            element->GetBytes(size, bytes);
            ss << std::string(bytes, size);
            delete [] bytes;
          }
        } else if (element->GetType() == PDE_TYPE_FILE) {
          ss << "\n\tFile: " << std::string(element->GetFile());
        }
      }
    }
  }

  return ss.str();
}

std::string GetDataURI(const std::string& data,
                       const std::string& mime_type) {
  return "data:" + mime_type + ";base64," +
      CefURIEncode(CefBase64Encode(data.data(), data.size()), false).ToString();
}

std::string GetErrorString(cef_errorcode_t code) {
  // Case condition that returns |code| as a string.
  #define CASE(code) case code: return #code

  switch (code) {
    CASE(ERR_NONE);
    CASE(ERR_FAILED);
    CASE(ERR_ABORTED);
    CASE(ERR_INVALID_ARGUMENT);
    CASE(ERR_INVALID_HANDLE);
    CASE(ERR_FILE_NOT_FOUND);
    CASE(ERR_TIMED_OUT);
    CASE(ERR_FILE_TOO_BIG);
    CASE(ERR_UNEXPECTED);
    CASE(ERR_ACCESS_DENIED);
    CASE(ERR_NOT_IMPLEMENTED);
    CASE(ERR_CONNECTION_CLOSED);
    CASE(ERR_CONNECTION_RESET);
    CASE(ERR_CONNECTION_REFUSED);
    CASE(ERR_CONNECTION_ABORTED);
    CASE(ERR_CONNECTION_FAILED);
    CASE(ERR_NAME_NOT_RESOLVED);
    CASE(ERR_INTERNET_DISCONNECTED);
    CASE(ERR_SSL_PROTOCOL_ERROR);
    CASE(ERR_ADDRESS_INVALID);
    CASE(ERR_ADDRESS_UNREACHABLE);
    CASE(ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
    CASE(ERR_TUNNEL_CONNECTION_FAILED);
    CASE(ERR_NO_SSL_VERSIONS_ENABLED);
    CASE(ERR_SSL_VERSION_OR_CIPHER_MISMATCH);
    CASE(ERR_SSL_RENEGOTIATION_REQUESTED);
    CASE(ERR_CERT_COMMON_NAME_INVALID);
    CASE(ERR_CERT_DATE_INVALID);
    CASE(ERR_CERT_AUTHORITY_INVALID);
    CASE(ERR_CERT_CONTAINS_ERRORS);
    CASE(ERR_CERT_NO_REVOCATION_MECHANISM);
    CASE(ERR_CERT_UNABLE_TO_CHECK_REVOCATION);
    CASE(ERR_CERT_REVOKED);
    CASE(ERR_CERT_INVALID);
    CASE(ERR_CERT_END);
    CASE(ERR_INVALID_URL);
    CASE(ERR_DISALLOWED_URL_SCHEME);
    CASE(ERR_UNKNOWN_URL_SCHEME);
    CASE(ERR_TOO_MANY_REDIRECTS);
    CASE(ERR_UNSAFE_REDIRECT);
    CASE(ERR_UNSAFE_PORT);
    CASE(ERR_INVALID_RESPONSE);
    CASE(ERR_INVALID_CHUNKED_ENCODING);
    CASE(ERR_METHOD_NOT_SUPPORTED);
    CASE(ERR_UNEXPECTED_PROXY_AUTH);
    CASE(ERR_EMPTY_RESPONSE);
    CASE(ERR_RESPONSE_HEADERS_TOO_BIG);
    CASE(ERR_CACHE_MISS);
    CASE(ERR_INSECURE_RESPONSE);
    default:
      return "UNKNOWN";
  }
}

CefRefPtr<CefResourceHandler> GetResourceHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request) {
  std::string url = request->GetURL();
  if (url.find(kTestOrigin) == 0) {
    // Handle URLs in the test origin.
    std::string file_name, mime_type;
    if (ParseTestUrl(url, &file_name, &mime_type)) {
      if (file_name == "request.html") {
        // Show the request contents.
        const std::string& dump = DumpRequestContents(request);
        std::string str = "<html><body bgcolor=\"white\"><pre>" + dump +
                          "</pre></body></html>";
        CefRefPtr<CefStreamReader> stream =
            CefStreamReader::CreateForData(
                static_cast<void*>(const_cast<char*>(str.c_str())),
                str.size());
        DCHECK(stream.get());
        return new CefStreamResourceHandler("text/html", stream);
      } else {
        // Load the resource from file.
        CefRefPtr<CefStreamReader> stream =
            GetBinaryResourceReader(file_name.c_str());
        if (stream.get())
          return new CefStreamResourceHandler(mime_type, stream);
      }
    }
  }

  return NULL;
}

void Alert(CefRefPtr<CefBrowser> browser, const std::string& message) {
  // Escape special characters in the message.
  std::string msg = StringReplace(message, "\\", "\\\\");
  msg = StringReplace(msg, "'", "\\'");

  // Execute a JavaScript alert().
  CefRefPtr<CefFrame> frame = browser->GetMainFrame();
  frame->ExecuteJavaScript("alert('" + msg + "');", frame->GetURL(), 0);
}

void CreateMessageHandlers(MessageHandlerSet& handlers) {
  // Create the dialog test handlers.
  dialog_test::CreateMessageHandlers(handlers);

  // Create the binding test handlers.
  binding_test::CreateMessageHandlers(handlers);

  // Create the urlrequest test handlers.
  urlrequest_test::CreateMessageHandlers(handlers);

  // Create the window test handlers.
  window_test::CreateMessageHandlers(handlers);
}

void RegisterSchemeHandlers() {
  // Register the scheme handler.
  scheme_test::RegisterSchemeHandlers();
}

}  // namespace test_runner
}  // namespace client
