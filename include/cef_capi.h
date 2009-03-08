// Copyright (c) 2009 Marshall A. Greenblatt. All rights reserved.
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


#ifndef _CEF_CAPI_H
#define _CEF_CAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cef_export.h"
#include "cef_string.h"
#include "cef_string_map.h"
#include "cef_types.h"


// This function should only be called once when the application is started.
// Create the thread to host the UI message loop.  A return value of true
// indicates that it succeeded and false indicates that it failed. Set
// |multi_threaded_message_loop| to true to have the message loop run in
// a separate thread.  If |multi_threaded_message_loop| is false than
// the CefDoMessageLoopWork() function must be called from your message loop.
// Set |cache_path| to the location where cache data will be stored on disk.
// If |cache_path| is NULL or empty an in-memory cache will be used for cache
// data.
CEF_EXPORT int cef_initialize(int multi_threaded_message_loop,
                              const wchar_t* cache_path);

// This function should only be called once before the application exits.
// Shut down the thread hosting the UI message loop and destroy any created
// windows.
CEF_EXPORT void cef_shutdown();

// Perform message loop processing.  Has no affect if the browser UI loop is
// running in a separate thread.
CEF_EXPORT void cef_do_message_loop_work();


typedef struct _cef_base_t
{
  // Size of the data structure
  size_t size;

  // Increment the reference count.
  int (CEF_CALLBACK *add_ref)(struct _cef_base_t* base);
  // Decrement the reference count.  Delete this object when no references
  // remain.
  int (CEF_CALLBACK *release)(struct _cef_base_t* base);
  // Returns the current number of references.
  int (CEF_CALLBACK *get_refct)(struct _cef_base_t* base);
} cef_base_t;


// Check that the structure |s|, which is defined with a cef_base_t member named
// |base|, is large enough to contain the specified member |f|.
#define CEF_MEMBER_EXISTS(s, f)   \
  ((int)&((s)->f) - (int)(s) + sizeof((s)->f) <= (s)->base.size)

#define CEF_MEMBER_MISSING(s, f)  (!CEF_MEMBER_EXISTS(s, f) || !((s)->f))

// Structure used to represent a browser window.  All functions exposed by this
// structure should be thread safe.
typedef struct _cef_browser_t
{
  // Base structure
  cef_base_t base;

  // Returns true (1) if the browser can navigate backwards.
  int (CEF_CALLBACK *can_go_back)(struct _cef_browser_t* browser);
  // Navigate backwards.
  void (CEF_CALLBACK *go_back)(struct _cef_browser_t* browser);
  // Returns true (1) if the browser can navigate forwards.
  int (CEF_CALLBACK *can_go_forward)(struct _cef_browser_t* browser);
  // Navigate backwards.
  void (CEF_CALLBACK *go_forward)(struct _cef_browser_t* browser);
  // Reload the current page.
  void (CEF_CALLBACK *reload)(struct _cef_browser_t* browser);
  // Stop loading the page.
  void (CEF_CALLBACK *stop_load)(struct _cef_browser_t* browser);

  // Execute undo in the target frame.
  void (CEF_CALLBACK *undo)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);
  // Execute redo in the target frame.
  void (CEF_CALLBACK *redo)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);
  // Execute cut in the target frame.
  void (CEF_CALLBACK *cut)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);
  // Execute copy in the target frame.
  void (CEF_CALLBACK *copy)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);
  // Execute paste in the target frame.
  void (CEF_CALLBACK *paste)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);
  // Execute delete in the target frame.
  void (CEF_CALLBACK *del)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);
  // Execute select all in the target frame.
  void (CEF_CALLBACK *select_all)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);

  // Set focus for the browser window.  If |enable| is true (1) focus will be
  // set to the window.  Otherwise, focus will be removed.
  void (CEF_CALLBACK *set_focus)(struct _cef_browser_t* browser, int enable);

  // Execute printing in the target frame.  The user will be prompted with
  // the print dialog appropriate to the operating system.
  void (CEF_CALLBACK *print)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);

  // Save the target frame's HTML source to a temporary file and open it in
  // the default text viewing application.
  void (CEF_CALLBACK *view_source)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);

  // Returns the target frame's HTML source as a string. The returned string
  // must be released using cef_string_free().
  cef_string_t (CEF_CALLBACK *get_source)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);

  // Returns the target frame's display text as a string. The returned string
  // must be released using cef_string_free().
  cef_string_t (CEF_CALLBACK *get_text)(struct _cef_browser_t* browser,
      enum cef_targetframe_t targetFrame);

  // Load the request represented by the |request| object.
  void (CEF_CALLBACK *load_request)(struct _cef_browser_t* browser,
      struct _cef_request_t* request);

  // Convenience method for loading the specified |url| in the optional target
  // |frame|.
  void (CEF_CALLBACK *load_url)(struct _cef_browser_t* browser,
      const wchar_t* url, const wchar_t* frame);

  // Load the contents of |string| with the optional dummy target |url|.
  void (CEF_CALLBACK *load_string)(struct _cef_browser_t* browser,
      const wchar_t* string, const wchar_t* url);

  // Load the contents of |stream| with the optional dummy target |url|.
  void (CEF_CALLBACK *load_stream)(struct _cef_browser_t* browser,
      struct _cef_stream_reader_t* stream, const wchar_t* url);

  // Execute a string of JavaScript code in the specified target frame. The
  // |script_url| parameter is the URL where the script in question can be
  // found, if any. The renderer may request this URL to show the developer the
  // source of the error.  The |start_line| parameter is the base line number
  // to use for error reporting.
  void (CEF_CALLBACK *execute_javascript)(struct _cef_browser_t* browser,
      const wchar_t* jsCode, const wchar_t* scriptUrl, int startLine,
      enum cef_targetframe_t targetFrame);

  // Register a new handler tied to the specified JS object |name|. Returns
  // true if the handler is registered successfully.
  // A JS handler will be accessible to JavaScript as window.<classname>.
  int (CEF_CALLBACK *add_jshandler)(struct _cef_browser_t* browser,
      const wchar_t* classname, struct _cef_jshandler_t* handler);

  // Returns true if a JS handler with the specified |name| is currently
  // registered.
  int (CEF_CALLBACK *has_jshandler)(struct _cef_browser_t* browser,
      const wchar_t* classname);

  // Returns the JS handler registered with the specified |name|.
  struct _cef_jshandler_t* (CEF_CALLBACK *get_jshandler)(
      struct _cef_browser_t* browser, const wchar_t* classname);

  // Unregister the JS handler registered with the specified |name|.  Returns
  // true if the handler is unregistered successfully.
  int (CEF_CALLBACK *remove_jshandler)(struct _cef_browser_t* browser,
      const wchar_t* classname);

  // Unregister all JS handlers that are currently registered.
  void (CEF_CALLBACK *remove_all_jshandlers)(struct _cef_browser_t* browser);

  // Retrieve the window handle for this browser.
  cef_window_handle_t (CEF_CALLBACK *get_window_handle)(
      struct _cef_browser_t* browser);

  // Returns true (1) if the window is a popup window.
  int (CEF_CALLBACK *is_popup)(struct _cef_browser_t* browser);

  // Returns the handler for this browser.
  struct _cef_handler_t* (CEF_CALLBACK *get_handler)(
      struct _cef_browser_t* browser);

  // Return the currently loaded URL.  The returned string must be released
  // using cef_string_free().
  cef_string_t (CEF_CALLBACK *get_url)(struct _cef_browser_t* browser);

} cef_browser_t;


// Structure used to handle events generated by the browser window.  All methods
// functions by this class should be thread safe.
typedef struct _cef_handler_t
{
  // Base structure
  cef_base_t base;

  // Event called before a new window is created. The |parentBrowser| parameter
  // will point to the parent browser window, if any. The |popup| parameter
  // will be true (1) if the new window is a popup window. If you create the
  // window yourself you should populate the window handle member of
  // |createInfo| and return RV_HANDLED.  Otherwise, return RV_CONTINUE and the
  // framework will create the window.  By default, a newly created window will
  // recieve the same handler as the parent window.  To change the handler for
  // the new window modify the object that |handler| points to.
  enum cef_retval_t (CEF_CALLBACK *handle_before_created)(
      struct _cef_handler_t* handler, cef_browser_t* parentBrowser,
      cef_window_info_t* windowInfo, int popup,
      struct _cef_handler_t** newHandler, cef_string_t* url);

  // Event called after a new window is created. The return value is currently
  // ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_after_created)(
      struct _cef_handler_t* handler, cef_browser_t* browser);

  // Event called when the address bar changes. The return value is currently
  // ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_address_change)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      const wchar_t* url);

  // Event called when the page title changes. The return value is currently
  // ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_title_change)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      const wchar_t* title);

  // Event called before browser navigation. The client has an opportunity to
  // modify the |request| object if desired.  Return RV_HANDLED to cancel
  // navigation.
  enum cef_retval_t (CEF_CALLBACK *handle_before_browse)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      struct _cef_request_t* request, cef_handler_navtype_t navType,
      int isRedirect);

  // Event called when the browser begins loading a page.  The return value is
  // currently ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_load_start)(
      struct _cef_handler_t* handler, cef_browser_t* browser);

  // Event called when the browser is done loading a page.  This event will
  // be generated irrespective of whether the request completes successfully.
  // The return value is currently ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_load_end)(
      struct _cef_handler_t* handler, cef_browser_t* browser);

  // Called when the browser fails to load a resource.  |errorCode is the
  // error code number and |failedUrl| is the URL that failed to load.  To
  // provide custom error text assign the text to |errorText| and return
  // RV_HANDLED.  Otherwise, return RV_CONTINUE for the default error text.
  enum cef_retval_t (CEF_CALLBACK *handle_load_error)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_handler_errorcode_t errorCode, const wchar_t* failedUrl,
      cef_string_t* errorText);

  // Event called before a resource is loaded.  To allow the resource to load
  // normally return RV_CONTINUE. To redirect the resource to a new url
  // populate the |redirectUrl| value and return RV_CONTINUE.  To specify
  // data for the resource return a CefStream object in |resourceStream|, set
  // 'mimeType| to the resource stream's mime type, and return RV_CONTINUE.
  // To cancel loading of the resource return RV_HANDLED.
  enum cef_retval_t (CEF_CALLBACK *handle_before_resource_load)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      struct _cef_request_t* request, cef_string_t* redirectUrl,
      struct _cef_stream_reader_t** resourceStream, cef_string_t* mimeType,
      int loadFlags);

  // Event called before a context menu is displayed.  To cancel display of the
  // default context menu return RV_HANDLED.
  enum cef_retval_t (CEF_CALLBACK *handle_before_menu)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      const cef_handler_menuinfo_t* menuInfo);

  // Event called to optionally override the default text for a context menu
  // item.  |label| contains the default text and may be modified to substitute
  // alternate text.  The return value is currently ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_get_menu_label)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_handler_menuid_t menuId, cef_string_t* label);

  // Event called when an option is selected from the default context menu.
  // Return RV_HANDLED to cancel default handling of the action.
  enum cef_retval_t (CEF_CALLBACK *handle_menu_action)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_handler_menuid_t menuId);

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
  enum cef_retval_t (CEF_CALLBACK *handle_print_header_footer)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_print_info_t* printInfo, const wchar_t* url, const wchar_t* title,
      int currentPage, int maxPages, cef_string_t* topLeft,
      cef_string_t* topCenter, cef_string_t* topRight,
      cef_string_t* bottomLeft, cef_string_t* bottomCenter,
      cef_string_t* bottomRight);

  // Run a JS alert message.  Return RV_CONTINUE to display the default alert
  // or RV_HANDLED if you displayed a custom alert.
  enum cef_retval_t (CEF_CALLBACK *handle_jsalert)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      const wchar_t* message);

  // Run a JS confirm request.  Return RV_CONTINUE to display the default alert
  // or RV_HANDLED if you displayed a custom alert.  If you handled the alert
  // set |retval| to true (1) if the user accepted the confirmation.
  enum cef_retval_t (CEF_CALLBACK *handle_jsconfirm)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      const wchar_t* message, int* retval);

  // Run a JS prompt request.  Return RV_CONTINUE to display the default prompt
  // or RV_HANDLED if you displayed a custom prompt.  If you handled the prompt
  // set |retval| to true if the user accepted the prompt and request and
  // |result| to the resulting value.
  enum cef_retval_t (CEF_CALLBACK *handle_jsprompt)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      const wchar_t* message, const wchar_t* defaultValue, int* retval,
      cef_string_t* result);

  // Called just before a window is closed. The return value is currently
  // ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_before_window_close)(
      struct _cef_handler_t* handler, cef_browser_t* browser);

  // Called when the browser component is about to loose focus. For instance,
  // if focus was on the last HTML element and the user pressed the TAB key.
  // The return value is currently ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_take_focus)(
      struct _cef_handler_t* handler, cef_browser_t* browser, int reverse);

} cef_handler_t;


// Structure used to represent a web request.
typedef struct _cef_request_t
{
  // Base structure
  cef_base_t base;

  // Fully qualified URL to load.
  cef_string_t (CEF_CALLBACK *get_url)(struct _cef_request_t* request);
  void (CEF_CALLBACK *set_url)(struct _cef_request_t* request,
      const wchar_t* url);

  // Optional name of the target frame.
  cef_string_t (CEF_CALLBACK *get_frame)(struct _cef_request_t* request);
  void (CEF_CALLBACK *set_frame)(struct _cef_request_t* request,
      const wchar_t* frame);

  // Optional request method type, defaulting to POST if post data is provided
  // and GET otherwise.
  cef_string_t (CEF_CALLBACK *get_method)(struct _cef_request_t* request);
  void (CEF_CALLBACK *set_method)(struct _cef_request_t* request,
      const wchar_t* method);

  // Optional post data.
  struct _cef_post_data_t* (CEF_CALLBACK *get_post_data)(
      struct _cef_request_t* request);
  void (CEF_CALLBACK *set_post_data)(struct _cef_request_t* request,
      struct _cef_post_data_t* postData);

  // Optional header values.
  void (CEF_CALLBACK *get_header_map)(struct _cef_request_t* request,
      cef_string_map_t headerMap);
  void (CEF_CALLBACK *set_header_map)(struct _cef_request_t* request,
      cef_string_map_t headerMap);

  // Set all values at one time.
  void (CEF_CALLBACK *set)(struct _cef_request_t* request, const wchar_t* url,
      const wchar_t* frame, const wchar_t* method,
      struct _cef_post_data_t* postData, cef_string_map_t headerMap);

} cef_request_t;


// Structure used to represent post data for a web request.
typedef struct _cef_post_data_t
{
  // Base structure
  cef_base_t base;

  // Returns the number of existing post data elements.
  size_t (CEF_CALLBACK *get_element_count)(struct _cef_post_data_t* postData);

  // Retrieve the post data element at the specified zero-based index.
  struct _cef_post_data_element_t* (CEF_CALLBACK *get_element)(
      struct _cef_post_data_t* postData, int index);

  // Remove the specified post data element.  Returns true (1) if the removal
  // succeeds.
  int (CEF_CALLBACK *remove_element)(struct _cef_post_data_t* postData,
      struct _cef_post_data_element_t* element);

  // Add the specified post data element.  Returns true (1) if the add succeeds.
  int (CEF_CALLBACK *add_element)(struct _cef_post_data_t* postData,
      struct _cef_post_data_element_t* element);

  // Remove all existing post data elements.
  void (CEF_CALLBACK *remove_elements)(struct _cef_post_data_t* postData);

} cef_post_data_t;


// Structure used to represent a single element in the request post data.
typedef struct _cef_post_data_element_t
{
  // Base structure
  cef_base_t base;

  // Remove all contents from the post data element.
  void (CEF_CALLBACK *set_to_empty)(
      struct _cef_post_data_element_t* postDataElement);

  // The post data element will represent a file.
  void (CEF_CALLBACK *set_to_file)(
      struct _cef_post_data_element_t* postDataElement,
      const wchar_t* fileName);

  // The post data element will represent bytes.  The bytes passed
  // in will be copied.
  void (CEF_CALLBACK *set_to_bytes)(
      struct _cef_post_data_element_t* postDataElement, size_t size,
      const void* bytes);

  // Return the type of this post data element.
  cef_postdataelement_type_t (CEF_CALLBACK *get_type)(
      struct _cef_post_data_element_t* postDataElement);

  // Return the file name.
  cef_string_t (CEF_CALLBACK *get_file)(
      struct _cef_post_data_element_t* postDataElement);

  // Return the number of bytes.
  size_t (CEF_CALLBACK *get_bytes_count)(
      struct _cef_post_data_element_t* postDataElement);

  // Read up to |size| bytes into |bytes| and return the number of bytes
  // actually read.
  size_t (CEF_CALLBACK *get_bytes)(
      struct _cef_post_data_element_t* postDataElement, size_t size,
      void *bytes);

} cef_post_data_element_t;


// Structure used to read data from a stream.
typedef struct _cef_stream_reader_t
{
  // Base structure
  cef_base_t base;

  // Read raw binary data.
  size_t (CEF_CALLBACK *read)(struct _cef_stream_reader_t* stream, void *ptr,
      size_t size, size_t n);
	
  // Seek to the specified offset position. |whence| may be any one of
  // SEEK_CUR, SEEK_END or SEEK_SET.
  int (CEF_CALLBACK *seek)(struct _cef_stream_reader_t* stream, long offset,
      int whence);
	
  // Return the current offset position.
  long (CEF_CALLBACK *tell)(struct _cef_stream_reader_t* stream);

  // Return non-zero if at end of file.
  int (CEF_CALLBACK *eof)(struct _cef_stream_reader_t* stream);

} cef_stream_reader_t;


// Structure used to write data to a stream.
typedef struct _cef_stream_writer_t
{
  // Base structure
  cef_base_t base;

  size_t (CEF_CALLBACK *write)(struct _cef_stream_writer_t* stream,
      const void *ptr, size_t size, size_t n);
	
  // Seek to the specified offset position. |whence| may be any one of
  // SEEK_CUR, SEEK_END or SEEK_SET.
  int (CEF_CALLBACK *seek)(struct _cef_stream_writer_t* stream, long offset,
      int whence);
	
  // Return the current offset position.
  long (CEF_CALLBACK *tell)(struct _cef_stream_writer_t* stream);

  // Flush the stream.
  int (CEF_CALLBACK *flush)(struct _cef_stream_writer_t* stream);

} cef_stream_writer_t;


// Structure for implementing external JavaScript objects.
typedef struct _cef_jshandler_t
{
  // Base structure
  cef_base_t base;

  // Return true if the specified method exists.
  bool (CEF_CALLBACK *has_method)(struct _cef_jshandler_t* jshandler,
      cef_browser_t* browser, const wchar_t* name);
  
  // Return true if the specified property exists.
  bool (CEF_CALLBACK *has_property)(struct _cef_jshandler_t* jshandler,
      cef_browser_t* browser, const wchar_t* name);

  // Set the property value. Return true if the property is accepted.
  bool (CEF_CALLBACK *set_property)(struct _cef_jshandler_t* jshandler,
      cef_browser_t* browser, const wchar_t* name,
      struct _cef_variant_t* value);
  
  // Get the property value. Return true if the value is returned.
  bool (CEF_CALLBACK *get_property)(struct _cef_jshandler_t* jshandler,
      cef_browser_t* browser, const wchar_t* name,
      struct _cef_variant_t* value);

  // Execute a method with the specified argument vector and return
  // value.  Return true if the method was handled.
  bool (CEF_CALLBACK *execute_method)(struct _cef_jshandler_t* jshandler,
      cef_browser_t* browser, const wchar_t* name, size_t numargs,
      struct _cef_variant_t** args, struct _cef_variant_t* retval);

} cef_jshandler_t;


typedef struct _cef_variant_t
{
  // Base structure
  cef_base_t base;

  // Return the variant data type.
  cef_variant_type_t (CEF_CALLBACK *get_type)(struct _cef_variant_t* variant);

  // Assign various data types.
  void (CEF_CALLBACK *set_null)(struct _cef_variant_t* variant);
  void (CEF_CALLBACK *set_bool)(struct _cef_variant_t* variant, int val);
  void (CEF_CALLBACK *set_int)(struct _cef_variant_t* variant, int val);
  void (CEF_CALLBACK *set_double)(struct _cef_variant_t* variant, double val);
  void (CEF_CALLBACK *set_string)(struct _cef_variant_t* variant,
      const wchar_t* val);
  void (CEF_CALLBACK *set_bool_array)(struct _cef_variant_t* variant,
      size_t count, const int* vals);
  void (CEF_CALLBACK *set_int_array)(struct _cef_variant_t* variant,
      size_t count, const int* vals);
  void (CEF_CALLBACK *set_double_array)(struct _cef_variant_t* variant,
      size_t count, const double* vals);
  void (CEF_CALLBACK *set_string_array)(struct _cef_variant_t* variant,
      size_t count, const cef_string_t* vals);

  // Retrieve various data types.
  int (CEF_CALLBACK *get_bool)(struct _cef_variant_t* variant);
  int (CEF_CALLBACK *get_int)(struct _cef_variant_t* variant);
  double (CEF_CALLBACK *get_double)(struct _cef_variant_t* variant);
  cef_string_t (CEF_CALLBACK *get_string)(struct _cef_variant_t* variant);
  
  // Returns the number of values in the array.  Returns -1 if the variant
  // is not an array type.
  int (CEF_CALLBACK *get_array_size)(struct _cef_variant_t* variant);

  // Reads up to |maxcount| values into the specified |vals| array.  Returns
  // the number of values actually read in.
  size_t (CEF_CALLBACK *get_bool_array)(struct _cef_variant_t* variant,
      size_t maxcount, int* vals);
  size_t (CEF_CALLBACK *get_int_array)(struct _cef_variant_t* variant,
      size_t maxcount, int* vals);
  size_t (CEF_CALLBACK *get_double_array)(struct _cef_variant_t* variant,
      size_t maxcount, double* vals);
  size_t (CEF_CALLBACK *get_string_array)(struct _cef_variant_t* variant,
      size_t maxcount, cef_string_t* vals);

} cef_variant_t;


// Create a new browser window using the window parameters specified
// by |windowInfo|. All values will be copied internally and the actual
// window will be created on the UI thread.  The |popup| parameter should
// be true (1) if the new window is a popup window. This method call will not
// block.
CEF_EXPORT int cef_create_browser(cef_window_info_t* windowInfo, int popup,
                                  cef_handler_t* handler, const wchar_t* url);

// Create a new browser window using the window parameters specified
// by |windowInfo|. The |popup| parameter should be true (1) if the new window
// is a popup window. This method call will block and can only be used if
// the |multi_threaded_message_loop| parameter to CefInitialize() is false.
CEF_EXPORT cef_browser_t* cef_create_browser_sync(cef_window_info_t* windowInfo,
                                                  int popup,
                                                  cef_handler_t* handler,
                                                  const wchar_t* url);

// Create a new request structure.
CEF_EXPORT cef_request_t* cef_create_request();

// Create a new post data structure.
CEF_EXPORT cef_post_data_t* cef_create_post_data();

// Create a new post data element structure.
CEF_EXPORT cef_post_data_element_t* cef_create_post_data_element();

// Create a new stream reader structure for reading from the specified file.
CEF_EXPORT cef_stream_reader_t* cef_create_stream_reader_for_file(
    const wchar_t* fileName);

// Create a new stream reader structure for reading from the specified data.
CEF_EXPORT cef_stream_reader_t* cef_create_stream_reader_for_data(void *data,
    size_t size);


#ifdef __cplusplus
}
#endif

#endif // _CEF_CAPI_H
