// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_wrapper.h"
#include "resource_util.h"
#include "scheme_test.h"
#include "string_util.h"
#include "util.h"

#if defined(OS_WIN)
#include "resource.h"
#endif


// Implementation of the schema handler for client:// requests.
class ClientSchemeHandler : public CefSchemeHandler
{
public:
  ClientSchemeHandler() : offset_(0) {}

  // Process the request. All response generation should take place in this
  // method. If there is no response set |response_length| to zero or return
  // false and ReadResponse() will not be called. If the response length is not
  // known set |response_length| to -1 and ReadResponse() will be called until
  // it returns false or until the value of |bytes_read| is set to 0. If the
  // response length is known set |response_length| to a positive value and
  // ReadResponse() will be called until it returns false, the value of
  // |bytes_read| is set to 0 or the specified number of bytes have been read.
  // Use the |response| object to set the mime type, http status code and
  // optional header values for the response and return true. To redirect the
  // request to a new URL set |redirectUrl| to the new URL and return true.
  virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
                              CefString& redirectUrl,
                              CefRefPtr<CefResponse> response,
                              int* response_length)
  {
    REQUIRE_IO_THREAD();

    bool handled = false;

    AutoLock lock_scope(this);
    
    std::string url = request->GetURL();
    if(strstr(url.c_str(), "handler.html") != NULL) {
      // Build the response html
      data_ = "<html><head><title>Client Scheme Handler</title></head><body>"
              "This contents of this page page are served by the "
              "ClientSchemeHandler class handling the client:// protocol."
              "<br/>You should see an image:"
              "<br/><img src=\"client://tests/client.png\"><pre>";
      
      // Output a string representation of the request
      std::string dump;
      DumpRequestContents(request, dump);
      data_.append(dump);

      data_.append("</pre><br/>Try the test form:"
                   "<form method=\"POST\" action=\"handler.html\">"
                   "<input type=\"text\" name=\"field1\">"
                   "<input type=\"text\" name=\"field2\">"
                   "<input type=\"submit\">"
                   "</form></body></html>");

      handled = true;

      // Set the resulting mime type
      response->SetMimeType("text/html");
      response->SetStatus(200);
    }
    else if(strstr(url.c_str(), "client.png") != NULL) {
      // Load the response image
#if defined(OS_WIN)
      DWORD dwSize;
      LPBYTE pBytes;
      if(LoadBinaryResource(IDS_LOGO, dwSize, pBytes)) {
        data_ = std::string(reinterpret_cast<const char*>(pBytes), dwSize);
        handled = true;
        // Set the resulting mime type
        response->SetMimeType("image/jpg");
        response->SetStatus(200);
      }
#elif defined(OS_MACOSX)
      if(LoadBinaryResource("logo.png", data_)) {
        handled = true;
        response->SetMimeType("image/png");
        response->SetStatus(200);
      }
#endif
    }

    // Set the resulting response length
    *response_length = data_.length();
   
    return handled;
  }

  // Cancel processing of the request.
  virtual void Cancel()
  {
    REQUIRE_IO_THREAD();
  }

  // Copy up to |bytes_to_read| bytes into |data_out|. If the copy succeeds
  // set |bytes_read| to the number of bytes copied and return true. If the
  // copy fails return false and ReadResponse() will not be called again.
  virtual bool ReadResponse(void* data_out, int bytes_to_read,
                            int* bytes_read)
  {
    REQUIRE_IO_THREAD();

    bool has_data = false;
    *bytes_read = 0;

    AutoLock lock_scope(this);

    if(offset_ < data_.length()) {
      // Copy the next block of data into the buffer.
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(data_.length() - offset_));
      memcpy(data_out, data_.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      *bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

private:
  std::string data_;
  size_t offset_;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandler);
  IMPLEMENT_LOCKING(ClientSchemeHandler);
};

// Implementation of the factory for for creating schema handlers.
class ClientSchemeHandlerFactory : public CefSchemeHandlerFactory
{
public:
  // Return a new scheme handler instance to handle the request.
  virtual CefRefPtr<CefSchemeHandler> Create(const CefString& scheme_name,
                                             CefRefPtr<CefRequest> request)
  {
    REQUIRE_IO_THREAD();
    return new ClientSchemeHandler();
  }

  IMPLEMENT_REFCOUNTING(ClientSchemeHandlerFactory);
};

void InitSchemeTest()
{
  CefRegisterCustomScheme("client", true, false, false);
  CefRegisterSchemeHandlerFactory("client", "tests",
      new ClientSchemeHandlerFactory());
}

void RunSchemeTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL("client://tests/handler.html");
}
