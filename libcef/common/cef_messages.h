// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for CEF.
// Multiply-included message file, hence no include guard.

#include "base/shared_memory.h"
#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/referrer.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/upload_data.h"

// TODO(cef): Re-using the message start for extensions may be problematic in
// the future. It would be better if ipc_message_utils.h contained a value
// reserved for consumers of the content API.
// See: http://crbug.com/110911
#define IPC_MESSAGE_START ExtensionMsgStart


// Common types.

// Parameters structure for a request.
IPC_STRUCT_BEGIN(Cef_Request_Params)
  // Unique request id to match requests and responses.
  IPC_STRUCT_MEMBER(int, request_id)

  // Unique id of the target frame. -1 if unknown / invalid.
  IPC_STRUCT_MEMBER(int64, frame_id)

  // True if the request is user-initiated instead of internal.
  IPC_STRUCT_MEMBER(bool, user_initiated)

  // True if a response is expected.
  IPC_STRUCT_MEMBER(bool, expect_response)

  // Message name.
  IPC_STRUCT_MEMBER(std::string, name)

  // List of message arguments.
  IPC_STRUCT_MEMBER(ListValue, arguments)
IPC_STRUCT_END()

// Parameters structure for a response.
IPC_STRUCT_BEGIN(Cef_Response_Params)
  // Unique request id to match requests and responses.
  IPC_STRUCT_MEMBER(int, request_id)

  // True if a response ack is expected.
  IPC_STRUCT_MEMBER(bool, expect_response_ack)

  // True on success.
  IPC_STRUCT_MEMBER(bool, success)

  // Response or error string depending on the value of |success|.
  IPC_STRUCT_MEMBER(std::string, response)
IPC_STRUCT_END()

// Parameters structure for a cross-origin white list entry.
IPC_STRUCT_BEGIN(Cef_CrossOriginWhiteListEntry_Params)
  IPC_STRUCT_MEMBER(std::string, source_origin)
  IPC_STRUCT_MEMBER(std::string, target_protocol)
  IPC_STRUCT_MEMBER(std::string, target_domain)
  IPC_STRUCT_MEMBER(bool, allow_target_subdomains)
IPC_STRUCT_END()


// Messages sent from the browser to the renderer.

// Parameters for a resource request.
IPC_STRUCT_BEGIN(CefMsg_LoadRequest_Params)
  // The request method: GET, POST, etc.
  IPC_STRUCT_MEMBER(std::string, method)

  // The requested URL.
  IPC_STRUCT_MEMBER(GURL, url)

  // The URL to send in the "Referer" header field. Can be empty if there is
  // no referrer.
  IPC_STRUCT_MEMBER(GURL, referrer)
  // One of the WebKit::WebReferrerPolicy values.
  IPC_STRUCT_MEMBER(int, referrer_policy)

  // Identifies the frame within the RenderView that sent the request.
  // -1 if unknown / invalid.
  IPC_STRUCT_MEMBER(int64, frame_id)

  // Usually the URL of the document in the top-level window, which may be
  // checked by the third-party cookie blocking policy. Leaving it empty may
  // lead to undesired cookie blocking. Third-party cookie blocking can be
  // bypassed by setting first_party_for_cookies = url, but this should ideally
  // only be done if there really is no way to determine the correct value.
  IPC_STRUCT_MEMBER(GURL, first_party_for_cookies)

  // Additional HTTP request headers.
  IPC_STRUCT_MEMBER(std::string, headers)

  // net::URLRequest load flags (0 by default).
  IPC_STRUCT_MEMBER(int, load_flags)

  // Optional upload data (may be null).
  IPC_STRUCT_MEMBER(scoped_refptr<net::UploadData>, upload_data)
IPC_STRUCT_END()

// Tell the renderer to load a request.
IPC_MESSAGE_ROUTED1(CefMsg_LoadRequest,
                    CefMsg_LoadRequest_Params)

// Sent when the browser has a request for the renderer. The renderer may
// respond with a CefHostMsg_Response.
IPC_MESSAGE_ROUTED1(CefMsg_Request,
                    Cef_Request_Params)

// Optional message sent in response to a CefHostMsg_Request.
IPC_MESSAGE_ROUTED1(CefMsg_Response,
                    Cef_Response_Params)

// Optional Ack message sent to the browser to notify that a CefHostMsg_Response
// has been processed.
IPC_MESSAGE_ROUTED1(CefMsg_ResponseAck,
                    int /* request_id */)

// Sent to child processes to add or remove a cross-origin whitelist entry.
IPC_MESSAGE_CONTROL2(CefProcessMsg_ModifyCrossOriginWhitelistEntry,
                     bool /* add */,
                     Cef_CrossOriginWhiteListEntry_Params /* params */)

// Sent to child processes to clear the cross-origin whitelist.
IPC_MESSAGE_CONTROL0(CefProcessMsg_ClearCrossOriginWhitelist)


// Messages sent from the renderer to the browser.

// Parameters for a newly created render thread.
IPC_STRUCT_BEGIN(CefProcessHostMsg_GetNewRenderThreadInfo_Params)
  IPC_STRUCT_MEMBER(std::vector<Cef_CrossOriginWhiteListEntry_Params>,
                    cross_origin_whitelist_entries)

  IPC_STRUCT_MEMBER(ListValue, extra_info)
IPC_STRUCT_END()

// Retrieve information about a newly created render thread.
IPC_SYNC_MESSAGE_CONTROL0_1(
    CefProcessHostMsg_GetNewRenderThreadInfo,
    CefProcessHostMsg_GetNewRenderThreadInfo_Params /* params*/)

// Parameters for a newly created browser window.
IPC_STRUCT_BEGIN(CefProcessHostMsg_GetNewBrowserInfo_Params)
  IPC_STRUCT_MEMBER(int, browser_id)
  IPC_STRUCT_MEMBER(bool, is_popup)
IPC_STRUCT_END()

// Retrieve information about a newly created browser window.
IPC_SYNC_MESSAGE_CONTROL1_1(
    CefProcessHostMsg_GetNewBrowserInfo,
    int /* routing_id */,
    CefProcessHostMsg_GetNewBrowserInfo_Params /* params*/)

// Sent when a frame is identified for the first time.
IPC_MESSAGE_ROUTED3(CefHostMsg_FrameIdentified,
                    int64 /* frame_id */,
                    int64 /* parent_frame_id */,
                    string16 /* frame_name */)

// Sent when a frame has been detached.
IPC_MESSAGE_ROUTED1(CefHostMsg_FrameDetached,
                    int64 /* frame_id */)

// Sent when a new frame has been given focus.
IPC_MESSAGE_ROUTED1(CefHostMsg_FrameFocusChange,
                    int64 /* frame_id */)

// Sent when a frame has finished loading. Based on ViewHostMsg_DidFinishLoad.
IPC_MESSAGE_ROUTED4(CefHostMsg_DidFinishLoad,
                    int64 /* frame_id */,
                    GURL /* validated_url */,
                    bool /* is_main_frame */,
                    int /* http_status_code */)

// Sent when a new URL is about to be loaded in the main frame. Used for the
// cookie manager.
IPC_MESSAGE_ROUTED1(CefHostMsg_LoadingURLChange,
                    GURL /* loading_url */)

// Sent when the renderer has a request for the browser. The browser may respond
// with a CefMsg_Response.
IPC_MESSAGE_ROUTED1(CefHostMsg_Request,
                    Cef_Request_Params)

// Optional message sent in response to a CefMsg_Request.
IPC_MESSAGE_ROUTED1(CefHostMsg_Response,
                    Cef_Response_Params)

// Optional Ack message sent to the browser to notify that a CefMsg_Response
// has been processed.
IPC_MESSAGE_ROUTED1(CefHostMsg_ResponseAck,
                    int /* request_id */)


// Singly-included section for struct and custom IPC traits.
#ifndef CEF_LIBCEF_COMMON_CEF_MESSAGES_H_
#define CEF_LIBCEF_COMMON_CEF_MESSAGES_H_

namespace IPC {

// Extracted from chrome/common/automation_messages.h.
template <>
struct ParamTraits<scoped_refptr<net::UploadData> > {
  typedef scoped_refptr<net::UploadData> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CEF_LIBCEF_COMMON_CEF_MESSAGES_H_
