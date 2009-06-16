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
#include "cef_string_list.h"
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

// Register a new V8 extension with the specified JavaScript extension code and
// handler. Functions implemented by the handler are prototyped using the
// keyword 'native'. The calling of a native function is restricted to the scope
// in which the prototype of the native function is defined.
//
// Example JavaScript extension code:
//
//   // create the 'example' global object if it doesn't already exist.
//   if (!example)
//     example = {};
//   // create the 'example.test' global object if it doesn't already exist.
//   if (!example.test)
//     example.test = {};
//   (function() {
//     // Define the function 'example.test.myfunction'.
//     example.test.myfunction = function() {
//       // Call CefV8Handler::Execute() with the function name 'MyFunction'
//       // and no arguments.
//       native function MyFunction();
//       return MyFunction();
//     };
//     // Define the getter function for parameter 'example.test.myparam'.
//     example.test.__defineGetter__('myparam', function() {
//       // Call CefV8Handler::Execute() with the function name 'GetMyParam'
//       // and no arguments.
//       native function GetMyParam();
//       return GetMyParam();
//     });
//     // Define the setter function for parameter 'example.test.myparam'.
//     example.test.__defineSetter__('myparam', function(b) {
//       // Call CefV8Handler::Execute() with the function name 'SetMyParam'
//       // and a single argument.
//       native function SetMyParam();
//       if(b) SetMyParam(b);
//     });
//
//     // Extension definitions can also contain normal JavaScript variables
//     // and functions.
//     var myint = 0;
//     example.test.increment = function() {
//       myint += 1;
//       return myint;
//     };
//   })();
//
// Example usage in the page:
//
//   // Call the function.
//   example.test.myfunction();
//   // Set the parameter.
//   example.test.myparam = value;
//   // Get the parameter.
//   value = example.test.myparam;
//   // Call another function.
//   example.test.increment();
//
CEF_EXPORT int cef_register_extension(const wchar_t* extension_name,
                                      const wchar_t* javascript_code,
                                      struct _cef_v8handler_t* handler);


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

  // Set focus for the browser window.  If |enable| is true (1) focus will be
  // set to the window.  Otherwise, focus will be removed.
  void (CEF_CALLBACK *set_focus)(struct _cef_browser_t* browser, int enable);

  // Retrieve the window handle for this browser.
  cef_window_handle_t (CEF_CALLBACK *get_window_handle)(
      struct _cef_browser_t* browser);

  // Returns true (1) if the window is a popup window.
  int (CEF_CALLBACK *is_popup)(struct _cef_browser_t* browser);

  // Returns the handler for this browser.
  struct _cef_handler_t* (CEF_CALLBACK *get_handler)(
      struct _cef_browser_t* browser);

  // Returns the main (top-level) frame for the browser window.
  struct _cef_frame_t* (CEF_CALLBACK *get_main_frame)(
      struct _cef_browser_t* browser);

  // Returns the focused frame for the browser window.
  struct _cef_frame_t* (CEF_CALLBACK *get_focused_frame)(
      struct _cef_browser_t* browser);

  // Returns the frame with the specified name, or NULL if not found.
  struct _cef_frame_t* (CEF_CALLBACK *get_frame)(
      struct _cef_browser_t* browser,
      const wchar_t* name);

  // Reads the names of all existing frames into the provided string list.
  size_t (CEF_CALLBACK *get_frame_names)(struct _cef_browser_t* browser,
      cef_string_list_t list);
} cef_browser_t;


// Structure used to represent a frame in the browser window.  All functions
// exposed by this structure should be thread safe.
typedef struct _cef_frame_t
{
  // Base structure
  cef_base_t base;

  // Execute undo in this frame.
  void (CEF_CALLBACK *undo)(struct _cef_frame_t* frame);
  // Execute redo in this frame.
  void (CEF_CALLBACK *redo)(struct _cef_frame_t* frame);
  // Execute cut in this frame.
  void (CEF_CALLBACK *cut)(struct _cef_frame_t* frame);
  // Execute copy in this frame.
  void (CEF_CALLBACK *copy)(struct _cef_frame_t* frame);
  // Execute paste in this frame.
  void (CEF_CALLBACK *paste)(struct _cef_frame_t* frame);
  // Execute delete in this frame.
  void (CEF_CALLBACK *del)(struct _cef_frame_t* frame);
  // Execute select all in this frame.
  void (CEF_CALLBACK *select_all)(struct _cef_frame_t* frame);

  // Execute printing in the this frame.  The user will be prompted with the
  // print dialog appropriate to the operating system.
  void (CEF_CALLBACK *print)(struct _cef_frame_t* frame);

  // Save this frame's HTML source to a temporary file and open it in the
  // default text viewing application.
  void (CEF_CALLBACK *view_source)(struct _cef_frame_t* frame);

  // Returns this frame's HTML source as a string. The returned string must be
  // released using cef_string_free().
  cef_string_t (CEF_CALLBACK *get_source)(struct _cef_frame_t* frame);

  // Returns this frame's display text as a string. The returned string must be
  // released using cef_string_free().
  cef_string_t (CEF_CALLBACK *get_text)(struct _cef_frame_t* frame);

  // Load the request represented by the |request| object.
  void (CEF_CALLBACK *load_request)(struct _cef_frame_t* frame,
      struct _cef_request_t* request);

  // Load the specified |url|.
  void (CEF_CALLBACK *load_url)(struct _cef_frame_t* frame,
      const wchar_t* url);

  // Load the contents of |string| with the optional dummy target |url|.
  void (CEF_CALLBACK *load_string)(struct _cef_frame_t* frame,
      const wchar_t* string, const wchar_t* url);

  // Load the contents of |stream| with the optional dummy target |url|.
  void (CEF_CALLBACK *load_stream)(struct _cef_frame_t* frame,
      struct _cef_stream_reader_t* stream, const wchar_t* url);

  // Execute a string of JavaScript code in this frame. The |script_url|
  // parameter is the URL where the script in question can be found, if any.
  // The renderer may request this URL to show the developer the source of the
  // error.  The |start_line| parameter is the base line number to use for error
  // reporting.
  void (CEF_CALLBACK *execute_javascript)(struct _cef_frame_t* frame,
      const wchar_t* jsCode, const wchar_t* scriptUrl, int startLine);

  // Returns true (1) if this is the main frame.
  int (CEF_CALLBACK *is_main)(struct _cef_frame_t* frame);

  // Returns true (1) if this is the focused frame.
  int (CEF_CALLBACK *is_focused)(struct _cef_frame_t* frame);

  // Returns this frame's name. The returned string must be released using
  // cef_string_free().
  cef_string_t (CEF_CALLBACK *get_name)(struct _cef_frame_t* frame);

  // Returns the currently loaded URL.  The returned string must be released
  // using cef_string_free().
  cef_string_t (CEF_CALLBACK *get_url)(struct _cef_frame_t* frame);

} cef_frame_t;


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
      cef_frame_t* frame, const wchar_t* url);

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
      cef_frame_t* frame, struct _cef_request_t* request,
      cef_handler_navtype_t navType, int isRedirect);

  // Event called when the browser begins loading a page.  The |frame| pointer
  // will be NULL if the event represents the overall load status and not the
  // load status for a particular frame. The return value is currently ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_load_start)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_frame_t* frame);

  // Event called when the browser is done loading a page.  The |frame| pointer
  // will be NULL if the event represents the overall load status and not the
  // load status for a particular frame. This event will be generated
  // irrespective of whether the request completes successfully. The return
  // value is currently ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_load_end)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_frame_t* frame);

  // Called when the browser fails to load a resource.  |errorCode is the
  // error code number and |failedUrl| is the URL that failed to load.  To
  // provide custom error text assign the text to |errorText| and return
  // RV_HANDLED.  Otherwise, return RV_CONTINUE for the default error text.
  enum cef_retval_t (CEF_CALLBACK *handle_load_error)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_frame_t* frame, cef_handler_errorcode_t errorCode,
      const wchar_t* failedUrl, cef_string_t* errorText);

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
      cef_frame_t* frame, cef_print_info_t* printInfo, const wchar_t* url,
      const wchar_t* title, int currentPage, int maxPages,
      cef_string_t* topLeft, cef_string_t* topCenter, cef_string_t* topRight,
      cef_string_t* bottomLeft, cef_string_t* bottomCenter,
      cef_string_t* bottomRight);

  // Run a JS alert message.  Return RV_CONTINUE to display the default alert
  // or RV_HANDLED if you displayed a custom alert.
  enum cef_retval_t (CEF_CALLBACK *handle_jsalert)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_frame_t* frame, const wchar_t* message);

  // Run a JS confirm request.  Return RV_CONTINUE to display the default alert
  // or RV_HANDLED if you displayed a custom alert.  If you handled the alert
  // set |retval| to true (1) if the user accepted the confirmation.
  enum cef_retval_t (CEF_CALLBACK *handle_jsconfirm)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_frame_t* frame, const wchar_t* message, int* retval);

  // Run a JS prompt request.  Return RV_CONTINUE to display the default prompt
  // or RV_HANDLED if you displayed a custom prompt.  If you handled the prompt
  // set |retval| to true if the user accepted the prompt and request and
  // |result| to the resulting value.
  enum cef_retval_t (CEF_CALLBACK *handle_jsprompt)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_frame_t* frame, const wchar_t* message, const wchar_t* defaultValue,
      int* retval, cef_string_t* result);

  // Called just before a window is closed. The return value is currently
  // ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_before_window_close)(
      struct _cef_handler_t* handler, cef_browser_t* browser);

  // Called when the browser component is about to loose focus. For instance,
  // if focus was on the last HTML element and the user pressed the TAB key.
  // The return value is currently ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_take_focus)(
      struct _cef_handler_t* handler, cef_browser_t* browser, int reverse);

  // Event called for adding values to a frame's JavaScript 'window' object. The
  // return value is currently ignored.
  enum cef_retval_t (CEF_CALLBACK *handle_jsbinding)(
      struct _cef_handler_t* handler, cef_browser_t* browser,
      cef_frame_t* frame, struct _cef_v8value_t* object);

  // Called when the browser component is requesting focus. |isWidget| will be
  // true (1) if the focus is requested for a child widget of the browser
  // window. Return RV_CONTINUE to allow the focus to be set or RV_HANDLED to
  // cancel setting the focus.
  enum cef_retval_t (CEF_CALLBACK *handle_set_focus)(
      struct _cef_handler_t* handler, cef_browser_t* browser, int isWidget);

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
      const wchar_t* method, struct _cef_post_data_t* postData,
      cef_string_map_t headerMap);

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


// Structure that should be implemented to handle V8 function calls.
typedef struct _cef_v8handler_t
{
  // Base structure
  cef_base_t base;

  // Execute a method with the specified argument vector and return
  // value.  Return true if the method was handled.
  int (CEF_CALLBACK *execute)(struct _cef_v8handler_t* v8handler,
      const wchar_t* name, struct _cef_v8value_t* object, size_t numargs,
      struct _cef_v8value_t** args, struct _cef_v8value_t** retval,
      cef_string_t* exception);

} cef_v8handler_t;


// Structure representing a V8 value.
typedef struct _cef_v8value_t
{
  // Base structure
  cef_base_t base;

  // Check the value type.
  int (CEF_CALLBACK *is_undefined)(struct _cef_v8value_t* v8value);
  int (CEF_CALLBACK *is_null)(struct _cef_v8value_t* v8value);
  int (CEF_CALLBACK *is_bool)(struct _cef_v8value_t* v8value);
  int (CEF_CALLBACK *is_int)(struct _cef_v8value_t* v8value);
  int (CEF_CALLBACK *is_double)(struct _cef_v8value_t* v8value);
  int (CEF_CALLBACK *is_string)(struct _cef_v8value_t* v8value);
  int (CEF_CALLBACK *is_object)(struct _cef_v8value_t* v8value);
  int (CEF_CALLBACK *is_array)(struct _cef_v8value_t* v8value);
  int (CEF_CALLBACK *is_function)(struct _cef_v8value_t* v8value);
  
  // Return a primitive value type.  The underlying data will be converted to
  // the requested type if necessary.
  int (CEF_CALLBACK *get_bool_value)(struct _cef_v8value_t* v8value);
  int (CEF_CALLBACK *get_int_value)(struct _cef_v8value_t* v8value);
  double (CEF_CALLBACK *get_double_value)(struct _cef_v8value_t* v8value);
  // The returned string must be released using cef_string_free().
  cef_string_t (CEF_CALLBACK *get_string_value)(struct _cef_v8value_t* v8value);


  // OBJECT METHODS - These methods are only available on objects. Arrays and
  // functions are also objects. String- and integer-based keys can be used
  // interchangably with the framework converting between them as necessary.
  // Keys beginning with "Cef::" and "v8::" are reserved by the system.

  // Returns true if the object has a value with the specified identifier.
  int (CEF_CALLBACK *has_value_bykey)(struct _cef_v8value_t* v8value,
      const wchar_t* key);
  int (CEF_CALLBACK *has_value_byindex)(struct _cef_v8value_t* v8value,
      int index);
  
  // Delete the value with the specified identifier.
  int (CEF_CALLBACK *delete_value_bykey)(struct _cef_v8value_t* v8value,
      const wchar_t* key);
  int (CEF_CALLBACK *delete_value_byindex)(struct _cef_v8value_t* v8value,
      int index);
  
  // Returns the value with the specified identifier.
  struct _cef_v8value_t* (CEF_CALLBACK *get_value_bykey)(
      struct _cef_v8value_t* v8value,
      const wchar_t* key);
  struct _cef_v8value_t* (CEF_CALLBACK *get_value_byindex)(
      struct _cef_v8value_t* v8value,
      int index);
  
  // Associate value with the specified identifier.
  int (CEF_CALLBACK *set_value_bykey)(struct _cef_v8value_t* v8value,
      const wchar_t* key, struct _cef_v8value_t* new_value);
  int (CEF_CALLBACK *set_value_byindex)(struct _cef_v8value_t* v8value,
      int index, struct _cef_v8value_t* new_value);
  
  // Read the keys for the object's values into the specified vector. Integer-
  // based keys will also be returned as strings.
  int (CEF_CALLBACK *get_keys)(struct _cef_v8value_t* v8value,
      cef_string_list_t list);

  // Returns the user data, if any, specified when the object was created.
  struct _cef_base_t* (CEF_CALLBACK *get_user_data)(
      struct _cef_v8value_t* v8value);


  // ARRAY METHODS - These methods are only available on arrays.

  // Returns the number of elements in the array.
  int (CEF_CALLBACK *get_array_length)(struct _cef_v8value_t* v8value);
  

  // FUNCTION METHODS - These methods are only available on functions.

  // Returns the function name.  The returned string must be released using
  // cef_string_free().
  cef_string_t (CEF_CALLBACK *get_function_name)(
      struct _cef_v8value_t* v8value);

  // Returns the function handler or NULL if not a CEF-created function.
  struct _cef_v8handler_t* (CEF_CALLBACK *get_function_handler)(
      struct _cef_v8value_t* v8value);

  // Execute the function.
  int (CEF_CALLBACK *execute_function)(struct _cef_v8value_t* v8value,
      struct _cef_v8value_t* object, size_t numargs,
      struct _cef_v8value_t** args, struct _cef_v8value_t** retval,
      cef_string_t* exception);

} cef_v8value_t;


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

// Create a new value of the specified type.  These functions should only be
// called from within the JavaScript context -- either in a
// cef_v8handler_t::execute callback or a cef_handler_t::handle_script_binding
// callback.
CEF_EXPORT cef_v8value_t* cef_create_v8value_undefined();
CEF_EXPORT cef_v8value_t* cef_create_v8value_null();
CEF_EXPORT cef_v8value_t* cef_create_v8value_bool(int value);
CEF_EXPORT cef_v8value_t* cef_create_v8value_int(int value);
CEF_EXPORT cef_v8value_t* cef_create_v8value_double(double value);
CEF_EXPORT cef_v8value_t* cef_create_v8value_string(const wchar_t* value);
CEF_EXPORT cef_v8value_t* cef_create_v8value_object(cef_base_t* user_data);
CEF_EXPORT cef_v8value_t* cef_create_v8value_array();
CEF_EXPORT cef_v8value_t* cef_create_v8value_function(const wchar_t* name,
                                                      cef_v8handler_t* handler);


#ifdef __cplusplus
}
#endif

#endif // _CEF_CAPI_H
