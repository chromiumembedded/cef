// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_wrapper.h"
#include "resource_util.h"
#include "scheme_test.h"
#include "string_util.h"
#include "util.h"

#ifdef _WIN32
#include "resource.h"
#endif // _WIN32


// Implementation of the schema handler for client:// requests.
class ClientSchemeHandler : public CefThreadSafeBase<CefSchemeHandler>
{
public:
  ClientSchemeHandler() : size_(0), offset_(0), bytes_(NULL) {}

  // Process the request. All response generation should take place in this
  // method. If there is no response set |response_length| to zero and
  // ReadResponse() will not be called. If the response length is not known then
  // set |response_length| to -1 and ReadResponse() will be called until it
  // returns false or until the value of |bytes_read| is set to 0. Otherwise,
  // set |response_length| to a positive value and ReadResponse() will be called
  // until it returns false, the value of |bytes_read| is set to 0 or the
  // specified number of bytes have been read. If there is a response set
  // |mime_type| to the mime type for the response.
  virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
                              CefString& mime_type, int* response_length)
  {
    REQUIRE_IO_THREAD();

    bool handled = false;

    Lock();
    std::string url = request->GetURL();
    if(strstr(url.c_str(), "handler.html") != NULL) {
      // Build the response html
      html_ = "<html><head><title>Client Scheme Handler</title></head><body>"
              "This contents of this page page are served by the "
              "ClientSchemeHandler class handling the client:// protocol.";
      
#ifdef _WIN32
      html_.append("<br/>You should see an image:"
                   "<br/><img src=\"client://tests/client.gif\">");
#endif // _WIN32
      
      html_.append("<pre>");
      
      // Output a string representation of the request
      std::string dump;
      DumpRequestContents(request, dump);
      html_.append(dump);

      html_.append("</pre><br/>Try the test form:"
                   "<form method=\"POST\" action=\"handler.html\">"
                   "<input type=\"text\" name=\"field1\">"
                   "<input type=\"text\" name=\"field2\">"
                   "<input type=\"submit\">"
                   "</form></body></html>");

      handled = true;
      size_ = html_.size();
      bytes_ = html_.c_str();

      // Set the resulting mime type
      mime_type = "text/html";
    }
#ifdef _WIN32
    else if(strstr(url.c_str(), "client.gif") != NULL) {
      // Load the response image
      DWORD dwSize;
      LPBYTE pBytes;
      if(LoadBinaryResource(IDS_LOGO, dwSize, pBytes)) {
        size_ = dwSize;
        bytes_ = reinterpret_cast<const char*>(pBytes);
        handled = true;
        // Set the resulting mime type
        mime_type = "image/jpg";
      }
    }
#endif // _WIN32

    // Set the resulting response length
    *response_length = size_;
    Unlock();

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

    Lock();

    if(offset_ < size_) {
      // Copy the next block of data into the buffer.
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size_ - offset_));
      memcpy(data_out, bytes_ + offset_, transfer_size);
      offset_ += transfer_size;

      *bytes_read = transfer_size;
      has_data = true;
    }

    Unlock();

    return has_data;
  }

private:
  size_t size_, offset_;
  const char* bytes_;
  std::string html_;
};

// Implementation of the factory for for creating schema handlers.
class ClientSchemeHandlerFactory :
  public CefThreadSafeBase<CefSchemeHandlerFactory>
{
public:
  // Return a new scheme handler instance to handle the request.
  virtual CefRefPtr<CefSchemeHandler> Create()
  {
    REQUIRE_IO_THREAD();
    return new ClientSchemeHandler();
  }
};

void InitSchemeTest()
{
  CefRegisterScheme("client", "tests", new ClientSchemeHandlerFactory());
}

void RunSchemeTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL("client://tests/handler.html");
}
