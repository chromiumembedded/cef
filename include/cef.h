// Copyright (c) 2008 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#ifndef _CEF_H
#define _CEF_H

#include <string>
#include <map>
#include <vector>
#include "cef_ptr.h"


// This function should only be called once when the application is started.
// Create the thread to host the UI message loop.  A return value of true
// indicates that it succeeded and false indicates that it failed.
bool CefInitialize();

// This function should only be called once before the application exits.
// Shut down the thread hosting the UI message loop and destroy any created
// windows.
void CefShutdown();


// Interface defining the the reference count implementation methods. All
// framework classes must implement the CefBase class.
class CefBase
{
public:
  // The AddRef method increments the reference count for the object. It should
  // be called for every new copy of a pointer to a given object. The resulting
  // reference count value is returned and should be used for diagnostic/testing
  // purposes only.
  virtual int AddRef() =0;

  // The Release method decrements the reference count for the object. If the
  // reference count on the object falls to 0, then the object should free
  // itself from memory.  The resulting reference count value is returned and
  // should be used for diagnostic/testing purposes only.
  virtual int Release() =0;
};


// Bring in platform-specific definitions.
#ifdef _WIN32
#include "cef_win.h"
#endif


// Template that provides atomic implementations of AddRef() and Release()
// along with Lock() and Unlock() methods to protect critical sections of
// code from simultaneous access by multiple threads.
//
// The below example demonstrates how to use the CefThreadSafeBase template.
//
// class MyHandler : public CefThreadSafeBase<CefHandler>
// {
//    std::wstring m_title;
//
//    virtual RetVal HandleTitleChange(const std::wstring& title)
//    {
//       Lock();   // Begin protecting code
//       m_title = title;
//       Unlock(); // Done protecting code
//       return RV_HANDLED;
//    }
//    ...
// }
//
template <class ClassName>
class CefThreadSafeBase : public ClassName
{
public:
  CefThreadSafeBase()
  {
    m_dwRef = 0L;
  }
  ~CefThreadSafeBase()
  {
  }

  // Atomic reference increment.
  int AddRef()
  {
    return CefAtomicIncrement(&m_dwRef);
  }

  // Atomic reference decrement.  Delete this object when no references remain.
  int Release()
  {
    int retval = CefAtomicDecrement(&m_dwRef);
    if(retval == 0)
      delete this;
    return retval;
  }

  // Use the Lock() and Unlock() methods to protect a section of code from
  // simultaneous access by multiple threads.
  void Lock() { m_critsec.Lock(); }
  void Unlock() { m_critsec.Unlock(); }

protected:
  long m_dwRef;
  CefCriticalSection m_critsec;
};


class CefHandler;
class CefRequest;
class CefStreamReader;
class CefStreamWriter;
class CefPostData;
class CefPostDataElement;
class CefJSHandler;
class CefVariant;


// Class used to represent a browser window.  All methods exposed by this
// class should be thread safe.
class CefBrowser : public CefBase
{
public:
  // Create a new browser window using the window parameters specified
  // by |windowInfo|. All values will be copied internally and the actual
  // window will be created on the UI thread.  The |popup| parameter should
  // be true if the new window is a popup window. This method call will not
  // block.
  static bool CreateBrowser(CefWindowInfo& windowInfo, bool popup,
                            CefRefPtr<CefHandler> handler,
                            const std::wstring& url);

  // Returns true if the browser can navigate backwards.
  virtual bool CanGoBack() =0;
  // Navigate backwards.
  virtual void GoBack() =0;
  // Returns true if the browser can navigate forwards.
  virtual bool CanGoForward() =0;
  // Navigate backwards.
  virtual void GoForward() =0;
  // Reload the current page.
  virtual void Reload() =0;
  // Stop loading the page.
  virtual void StopLoad() =0;

  // Define frame target types. Using TF_FOCUSED will target the focused
  // frame and using TF_MAIN will target the main frame.
  enum TargetFrame
  {
    TF_FOCUSED   = 0,
    TF_MAIN      = 1
  };

  // Execute undo in the target frame.
  virtual void Undo(TargetFrame targetFrame) =0;
  // Execute redo in the target frame.
  virtual void Redo(TargetFrame targetFrame) =0;
  // Execute cut in the target frame.
  virtual void Cut(TargetFrame targetFrame) =0;
  // Execute copy in the target frame.
  virtual void Copy(TargetFrame targetFrame) =0;
  // Execute paste in the target frame.
  virtual void Paste(TargetFrame targetFrame) =0;
  // Execute delete in the target frame.
  virtual void Delete(TargetFrame targetFrame) =0;
  // Execute select all in the target frame.
  virtual void SelectAll(TargetFrame targetFrame) =0;

  // Execute printing in the target frame.  The user will be prompted with
  // the print dialog appropriate to the operating system.
  virtual void Print(TargetFrame targetFrame) =0;

  // Save the target frame's HTML source to a temporary file and open it in
  // the default text viewing application.
  virtual void ViewSource(TargetFrame targetFrame) =0;

  // Returns the target frame's HTML source as a string.
  virtual std::wstring GetSource(TargetFrame targetFrame) =0;

  // Returns the target frame's display text as a string.
  virtual std::wstring GetText(TargetFrame targetFrame) =0;

  // Load the request represented by the |request| object.
  virtual void LoadRequest(CefRefPtr<CefRequest> request) =0;

  // Convenience method for loading the specified |url| in the optional target
  // |frame|.
  virtual void LoadURL(const std::wstring& url, const std::wstring& frame) =0;
  
  // Load the contents of |string| with the optional dummy target |url|.
  virtual void LoadString(const std::wstring& string,
                          const std::wstring& url) =0;

  // Load the contents of |stream| with the optional dummy target |url|.
  virtual void LoadStream(CefRefPtr<CefStreamReader> stream,
                          const std::wstring& url) =0;

  // Register a new handler tied to the specified JS object |name|. Returns
  // true if the handler is registered successfully.
  // A JS handler will be accessible to JavaScript as window.<classname>.
  virtual bool AddJSHandler(const std::wstring& classname,
                            CefRefPtr<CefJSHandler> handler) =0;
  
  // Returns true if a JS handler with the specified |name| is currently
  // registered.
  virtual bool HasJSHandler(const std::wstring& classname) =0;

  // Returns the JS handler registered with the specified |name|.
  virtual CefRefPtr<CefJSHandler> GetJSHandler(const std::wstring& classname) =0;

  // Unregister the JS handler registered with the specified |name|.  Returns
  // true if the handler is unregistered successfully.
  virtual bool RemoveJSHandler(const std::wstring& classname) =0;

  // Unregister all JS handlers that are currently registered.
  virtual void RemoveAllJSHandlers() =0;

  // Retrieve the window handle for this browser.
  virtual CefWindowHandle GetWindowHandle() =0;

  // Returns true if the window is a popup window.
  virtual bool IsPopup() =0;

  // Returns the handler for this browser.
  virtual CefRefPtr<CefHandler> GetHandler() =0;

  // Return the currently loaded URL.
  virtual std::wstring GetURL() =0;
};


// Interface that should be implemented to handle events generated by the
// browser window.  All methods exposed by this class should be thread safe.
// Each method in the interface returns a RetVal value.
class CefHandler : public CefBase
{
public:
  // Define handler return value types. Returning RV_HANDLED indicates
  // that the implementation completely handled the method and that no further
  // processing is required.  Returning RV_CONTINUE indicates that the
  // implementation did not handle the method and that the default handler
  // should be called.
  enum RetVal
  {
    RV_HANDLED   = 0,
    RV_CONTINUE  = 1
  };

  // Event called before a new window is created. The |parentBrowser| parameter
  // will point to the parent browser window, if any. The |popup| parameter
  // will be true if the new window is a popup window. If you create the window
  // yourself you should populate the window handle member of |createInfo| and
  // return RV_HANDLED.  Otherwise, return RV_CONTINUE and the framework will
  // create the window.  By default, a newly created window will recieve the
  // same handler as the parent window.  To change the handler for the new
  // window modify the object that |handler| points to.
  virtual RetVal HandleBeforeCreated(CefRefPtr<CefBrowser> parentBrowser,
                                     CefWindowInfo& createInfo, bool popup,
                                     CefRefPtr<CefHandler>& handler,
                                     std::wstring& url) =0;

  // Event called after a new window is created. The return value is currently
  // ignored.
  virtual RetVal HandleAfterCreated(CefRefPtr<CefBrowser> browser) =0;

  // Event called when the address bar changes. The return value is currently
  // ignored.
  virtual RetVal HandleAddressChange(CefRefPtr<CefBrowser> browser,
                                     const std::wstring& url) =0;

  // Event called when the page title changes. The return value is currently
  // ignored.
  virtual RetVal HandleTitleChange(CefRefPtr<CefBrowser> browser,
                                   const std::wstring& title) =0;

  // Various browser navigation types supported by chrome.
  enum NavType
  {
    NAVTYPE_LINKCLICKED = 0,
    NAVTYPE_FORMSUBMITTED,
    NAVTYPE_BACKFORWARD,
    NAVTYPE_RELOAD,
    NAVTYPE_FORMRESUBMITTED,
    NAVTYPE_OTHER
  };

  // Event called before browser navigation. The client has an opportunity to
  // modify the |request| object if desired.  Return RV_HANDLED to cancel
  // navigation.
  virtual RetVal HandleBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefRequest> request,
                                    NavType navType, bool isRedirect) =0;

  // Event called when the browser begins loading a page.  The return value is
  // currently ignored.
  virtual RetVal HandleLoadStart(CefRefPtr<CefBrowser> browser) =0;

  // Event called when the browser is done loading a page.  This event will
  // be generated irrespective of whether the request completes successfully.
  // The return value is currently ignored.
  virtual RetVal HandleLoadEnd(CefRefPtr<CefBrowser> browser) =0;

  // Supported error code values. See net\base\net_error_list.h for complete
  // descriptions of the error codes.
  enum ErrorCode
  {
    ERR_FAILED = -2,
    ERR_ABORTED = -3,
    ERR_INVALID_ARGUMENT = -4,
    ERR_INVALID_HANDLE = -5,
    ERR_FILE_NOT_FOUND = -6,
    ERR_TIMED_OUT = -7,
    ERR_FILE_TOO_BIG = -8,
    ERR_UNEXPECTED = -9,
    ERR_ACCESS_DENIED = -10,
    ERR_NOT_IMPLEMENTED = -11,
    ERR_CONNECTION_CLOSED = -100,
    ERR_CONNECTION_RESET = -101,
    ERR_CONNECTION_REFUSED = -102,
    ERR_CONNECTION_ABORTED = -103,
    ERR_CONNECTION_FAILED = -104,
    ERR_NAME_NOT_RESOLVED = -105,
    ERR_INTERNET_DISCONNECTED = -106,
    ERR_SSL_PROTOCOL_ERROR = -107,
    ERR_ADDRESS_INVALID = -108,
    ERR_ADDRESS_UNREACHABLE = -109,
    ERR_SSL_CLIENT_AUTH_CERT_NEEDED = -110,
    ERR_TUNNEL_CONNECTION_FAILED = -111,
    ERR_NO_SSL_VERSIONS_ENABLED = -112,
    ERR_SSL_VERSION_OR_CIPHER_MISMATCH = -113,
    ERR_SSL_RENEGOTIATION_REQUESTED = -114,
    ERR_CERT_COMMON_NAME_INVALID = -200,
    ERR_CERT_DATE_INVALID = -201,
    ERR_CERT_AUTHORITY_INVALID = -202,
    ERR_CERT_CONTAINS_ERRORS = -203,
    ERR_CERT_NO_REVOCATION_MECHANISM = -204,
    ERR_CERT_UNABLE_TO_CHECK_REVOCATION = -205,
    ERR_CERT_REVOKED = -206,
    ERR_CERT_INVALID = -207,
    ERR_CERT_END = -208,
    ERR_INVALID_URL = -300,
    ERR_DISALLOWED_URL_SCHEME = -301,
    ERR_UNKNOWN_URL_SCHEME = -302,
    ERR_TOO_MANY_REDIRECTS = -310,
    ERR_UNSAFE_REDIRECT = -311,
    ERR_UNSAFE_PORT = -312,
    ERR_INVALID_RESPONSE = -320,
    ERR_INVALID_CHUNKED_ENCODING = -321,
    ERR_METHOD_NOT_SUPPORTED = -322,
    ERR_UNEXPECTED_PROXY_AUTH = -323,
    ERR_EMPTY_RESPONSE = -324,
    ERR_RESPONSE_HEADERS_TOO_BIG = -325,
    ERR_CACHE_MISS = -400,
    ERR_INSECURE_RESPONSE = -501,
  };

  // Called when the browser fails to load a resource.  |errorCode is the
  // error code number and |failedUrl| is the URL that failed to load.  To
  // provide custom error text assign the text to |errorText| and return
  // RV_HANDLED.  Otherwise, return RV_CONTINUE for the default error text.
  virtual RetVal HandleLoadError(CefRefPtr<CefBrowser> browser,
                                 ErrorCode errorCode,
                                 const std::wstring& failedUrl,
                                 std::wstring& errorText) =0;

  // Event called before a resource is loaded.  To allow the resource to load
  // normally return RV_CONTINUE. To redirect the resource to a new url
  // populate the |redirectUrl| value and return RV_CONTINUE.  To specify
  // data for the resource return a CefStream object in |resourceStream|, set
  // 'mimeType| to the resource stream's mime type, and return RV_CONTINUE.
  // To cancel loading of the resource return RV_HANDLED.
  virtual RetVal HandleBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefRequest> request,
                                          std::wstring& redirectUrl,
                                          CefRefPtr<CefStreamReader>& resourceStream,
                                          std::wstring& mimeType,
                                          int loadFlags) =0;

  // Structure representing menu information
  struct MenuInfo
  {
    int typeFlags;
    int x;
    int y;
    std::wstring linkUrl;
    std::wstring imageUrl;
    std::wstring pageUrl;
    std::wstring frameUrl;
    std::wstring selectionText;
    std::wstring misspelledWord;
    int editFlags;
    std::string securityInfo;
  };

  // The MenuInfo typeFlags value will be a combination of the following
  enum MenuTypeBits {
    // No node is selected
    MENUTYPE_NONE = 0x0,
    // The top page is selected
    MENUTYPE_PAGE = 0x1,
    // A subframe page is selected
    MENUTYPE_FRAME = 0x2,
    // A link is selected
    MENUTYPE_LINK = 0x4,
    // An image is selected
    MENUTYPE_IMAGE = 0x8,
    // There is a textual or mixed selection that is selected
    MENUTYPE_SELECTION = 0x10,
    // An editable element is selected
    MENUTYPE_EDITABLE = 0x20,
    // A misspelled word is selected
    MENUTYPE_MISSPELLED_WORD = 0x40,
  };

  // The MenuInfo editFlags value will be a combination of the following
  enum MenuCapabilityBits
  {
    CAN_DO_NONE = 0x0,
    CAN_UNDO = 0x1,
    CAN_REDO = 0x2,
    CAN_CUT = 0x4,
    CAN_COPY = 0x8,
    CAN_PASTE = 0x10,
    CAN_DELETE = 0x20,
    CAN_SELECT_ALL = 0x40,
    CAN_GO_FORWARD = 0x80,
    CAN_GO_BACK = 0x100,
  };

  // Event called before a context menu is displayed.  To cancel display of the
  // default context menu return RV_HANDLED.
  virtual RetVal HandleBeforeMenu(CefRefPtr<CefBrowser> browser,
                                  const MenuInfo& menuInfo) =0;

  // Supported menu ID values
  enum MenuId
  {
    ID_NAV_BACK = 10,
    ID_NAV_FORWARD = 11,
    ID_NAV_RELOAD = 12,
    ID_NAV_STOP = 13,
    ID_UNDO = 20,
    ID_REDO = 21,
    ID_CUT = 22,
    ID_COPY = 23,
    ID_PASTE = 24,
    ID_DELETE = 25,
    ID_SELECTALL = 26,
    ID_PRINT = 30,
    ID_VIEWSOURCE = 31,
  };

  // Event called to optionally override the default text for a context menu
  // item.  |label| contains the default text and may be modified to substitute
  // alternate text.  The return value is currently ignored.
  virtual RetVal HandleGetMenuLabel(CefRefPtr<CefBrowser> browser,
                                    MenuId menuId, std::wstring& label) =0;

  // Event called when an option is selected from the default context menu.
  // Return RV_HANDLED to cancel default handling of the action.
  virtual RetVal HandleMenuAction(CefRefPtr<CefBrowser> browser,
                                  MenuId menuId) =0;

  // Event called to format print headers and footers.  |printInfo| contains
  // platform-specific information about the printer context.  |url| is the
  // URL if the currently printing page, |title| is the title of the currently
  // printing page, |currentPage| is the current page number and |maxPages| is
  // the total number of pages.  Six default header locations are provided
  // by the implementation: top left, top center, top right, bottom left,
  // bottom center and bottom right.  To use one of these default locations
  // just assign a string to the appropriate variable.  To draw the header
  // and footer yourself return RV_HANDLED.  Otherwise, populate the approprate
  // variables and return RV_CONTINUE.
  virtual RetVal HandlePrintHeaderFooter(CefRefPtr<CefBrowser> browser,
                                         CefPrintInfo& printInfo,
                                         const std::wstring& url,
                                         const std::wstring& title,
                                         int currentPage, int maxPages,
                                         std::wstring& topLeft,
                                         std::wstring& topCenter,
                                         std::wstring& topRight,
                                         std::wstring& bottomLeft,
                                         std::wstring& bottomCenter,
                                         std::wstring& bottomRight) =0;

  // Run a JS alert message.  Return RV_CONTINUE to display the default alert
  // or RV_HANDLED if you displayed a custom alert.
  virtual RetVal HandleJSAlert(CefRefPtr<CefBrowser> browser,
                               const std::wstring& message) =0;

  // Run a JS confirm request.  Return RV_CONTINUE to display the default alert
  // or RV_HANDLED if you displayed a custom alert.  If you handled the alert
  // set |retval| to true if the user accepted the confirmation.
  virtual RetVal HandleJSConfirm(CefRefPtr<CefBrowser> browser,
                                 const std::wstring& message, bool& retval) =0;

  // Run a JS prompt request.  Return RV_CONTINUE to display the default prompt
  // or RV_HANDLED if you displayed a custom prompt.  If you handled the prompt
  // set |retval| to true if the user accepted the prompt and request and
  // |result| to the resulting value.
  virtual RetVal HandleJSPrompt(CefRefPtr<CefBrowser> browser,
                                const std::wstring& message,
                                const std::wstring& default_value,
                                bool& retval,
                                std::wstring& result) =0;

};


// Class used to represent a web request.
class CefRequest : public CefBase
{
public:
  typedef std::map<std::wstring, std::wstring> HeaderMap;

  // Create a new CefRequest object.
  static CefRefPtr<CefRequest> CreateRequest();

  // Fully qualified URL to load.
  virtual std::wstring GetURL() =0;
  virtual void SetURL(const std::wstring& url) =0;

  // Optional name of the target frame.
  virtual std::wstring GetFrame() =0;
  virtual void SetFrame(const std::wstring& url) =0;

  // Optional request method type, defaulting to POST if post data is provided
  // and GET otherwise.
  virtual std::wstring GetMethod() =0;
  virtual void SetMethod(const std::wstring& method) =0;

  // Optional post data.
  virtual CefRefPtr<CefPostData> GetPostData() =0;
  virtual void SetPostData(CefRefPtr<CefPostData> postData) =0;

  // Optional header values.
  virtual void GetHeaderMap(HeaderMap& headerMap) =0;
  virtual void SetHeaderMap(const HeaderMap& headerMap) =0;

  // Set all values at one time.
  virtual void Set(const std::wstring& url,
                   const std::wstring& frame,
                   const std::wstring& method,
                   CefRefPtr<CefPostData> postData,
                   const HeaderMap& headerMap) =0;
};


// Class used to represent post data for a web request.
class CefPostData : public CefBase
{
public:
  typedef std::vector<CefRefPtr<CefPostDataElement> > ElementVector;

  // Create a new CefPostData object.
  static CefRefPtr<CefPostData> CreatePostData();

  // Returns the number of existing post data elements.
  virtual size_t GetElementCount() =0;

  // Retrieve the post data elements.
  virtual void GetElements(ElementVector& elements) =0;

  // Remove the specified post data element.  Returns true if the removal
  // succeeds.
  virtual bool RemoveElement(CefRefPtr<CefPostDataElement> element) =0;

  // Add the specified post data element.  Returns true if the add
  // succeeds.
  virtual bool AddElement(CefRefPtr<CefPostDataElement> element) =0;

  // Remove all existing post data elements.
  virtual void RemoveElements() =0;
};


// Class used to represent a single element in the request post data.
class CefPostDataElement : public CefBase
{
public:
  // Post data elements may represent either bytes or files.
  enum Type
  {
    TYPE_EMPTY  = 0,
    TYPE_BYTES,
    TYPE_FILE
  };

  // Create a new CefPostDataElement object.
  static CefRefPtr<CefPostDataElement> CreatePostDataElement();

  // Remove all contents from the post data element.
  virtual void SetToEmpty() =0;

  // The post data element will represent a file.
  virtual void SetToFile(const std::wstring& fileName) =0;

  // The post data element will represent bytes.  The bytes passed
  // in will be copied.
  virtual void SetToBytes(size_t size, const void* bytes) =0;

  // Return the type of this post data element.
  virtual Type GetType() =0;

  // Return the file name.
  virtual std::wstring GetFile() =0;

  // Return the number of bytes.
  virtual size_t GetBytesCount() =0;

  // Read up to |size| bytes into |bytes| and return the number of bytes
  // actually read.
  virtual size_t GetBytes(size_t size, void *bytes) =0;
};


// Class used to read data from a stream.
class CefStreamReader : public CefBase
{
public:
  static CefRefPtr<CefStreamReader> CreateForFile(const std::wstring& fileName);
  static CefRefPtr<CefStreamReader> CreateForData(void *data, size_t size);

  // Read raw binary data.
  virtual size_t Read(void *ptr, size_t size, size_t n) =0;
	
  // Seek to the specified offset position. |whence| may be any one of
  // SEEK_CUR, SEEK_END or SEEK_SET.
  virtual int Seek(long offset, int whence) =0;
	
  // Return the current offset position.
  virtual long Tell() =0;

  // Return non-zero if at end of file.
  virtual int Eof() =0;
};


// Class used to write data to a stream.
class CefStreamWriter : public CefBase
{
public:
  // Write raw binary data.
  virtual size_t Write(const void *ptr, size_t size, size_t n) =0;
	
  // Seek to the specified offset position. |whence| may be any one of
  // SEEK_CUR, SEEK_END or SEEK_SET.
  virtual int Seek(long offset, int whence) =0;
	
  // Return the current offset position.
  virtual long Tell() =0;

  // Flush the stream.
  virtual int Flush() =0;
};


// Class for implementing external JavaScript objects.
class CefJSHandler : public CefBase
{
public:
  typedef std::vector<CefRefPtr<CefVariant> > VariantVector;

  // Return true if the specified method exists.
  virtual bool HasMethod(CefRefPtr<CefBrowser> browser,
                         const std::wstring& name) =0;
  
  // Return true if the specified property exists.
  virtual bool HasProperty(CefRefPtr<CefBrowser> browser,
                           const std::wstring& name) =0;

  // Set the property value. Return true if the property is accepted.
  virtual bool SetProperty(CefRefPtr<CefBrowser> browser,
                           const std::wstring& name,
                           const CefRefPtr<CefVariant> value) =0;
  
  // Get the property value. Return true if the value is returned.
  virtual bool GetProperty(CefRefPtr<CefBrowser> browser,
                           const std::wstring& name,
                           CefRefPtr<CefVariant> value) =0;

  // Execute a method with the specified argument vector and return
  // value.  Return true if the method was handled.
  virtual bool ExecuteMethod(CefRefPtr<CefBrowser> browser,
                             const std::wstring& name,
                             const VariantVector& args,
                             CefRefPtr<CefVariant> retval) =0;
};


// Class that represents multiple data types as a single object.
class CefVariant : public CefBase
{
public:
  enum Type
  {
    TYPE_NULL = 0,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_BOOL_ARRAY,
    TYPE_INT_ARRAY,
    TYPE_DOUBLE_ARRAY,
    TYPE_STRING_ARRAY
  };

  // Return the variant data type.
  virtual Type GetType() =0;

  // Assign various data types.
  virtual void SetNull() =0;
  virtual void SetBool(bool val) =0;
  virtual void SetInt(int val) =0;
  virtual void SetDouble(double val) =0;
  virtual void SetString(const std::wstring& val) =0;
  virtual void SetBoolArray(const std::vector<bool>& val) =0;
  virtual void SetIntArray(const std::vector<int>& val) =0;
  virtual void SetDoubleArray(const std::vector<double>& val) =0;
  virtual void SetStringArray(const std::vector<std::wstring>& val) =0;

  // Retrieve various data types.
  virtual bool GetBool() =0;
  virtual int GetInt() =0;
  virtual double GetDouble() =0;
  virtual std::wstring GetString() =0;
  virtual bool GetBoolArray(std::vector<bool>& val) =0;
  virtual bool GetIntArray(std::vector<int>& val) =0;
  virtual bool GetDoubleArray(std::vector<double>& val) =0;
  virtual bool GetStringArray(std::vector<std::wstring>& val) =0;
};

#endif // _CEF_H
