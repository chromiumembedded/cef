// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for CEF.
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include "libcef/common/net/upload_data.h"

#include "base/memory/shared_memory_mapping.h"
#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/referrer.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

// Singly-included section for enums and custom IPC traits.
#ifndef CEF_LIBCEF_COMMON_CEF_MESSAGES_H_
#define CEF_LIBCEF_COMMON_CEF_MESSAGES_H_

namespace IPC {

// Extracted from chrome/common/automation_messages.h.
template <>
struct ParamTraits<scoped_refptr<net::UploadData>> {
  typedef scoped_refptr<net::UploadData> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CEF_LIBCEF_COMMON_CEF_MESSAGES_H_

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

  // True if the request is user-initiated instead of internal.
  IPC_STRUCT_MEMBER(bool, user_initiated)

  // True if a response is expected.
  IPC_STRUCT_MEMBER(bool, expect_response)

  // Message name.
  IPC_STRUCT_MEMBER(std::string, name)

  // List of message arguments.
  IPC_STRUCT_MEMBER(base::ListValue, arguments)
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

// Parameters structure for a draggable region.
IPC_STRUCT_BEGIN(Cef_DraggableRegion_Params)
  IPC_STRUCT_MEMBER(gfx::Rect, bounds)
  IPC_STRUCT_MEMBER(bool, draggable)
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
  // One of the cef_referrer_policy_t values.
  IPC_STRUCT_MEMBER(int, referrer_policy)

  // Usually the URL of the document in the top-level window, which may be
  // checked by the third-party cookie blocking policy. Leaving it empty may
  // lead to undesired cookie blocking. Third-party cookie blocking can be
  // bypassed by setting site_for_cookies = url, but this should ideally
  // only be done if there really is no way to determine the correct value.
  IPC_STRUCT_MEMBER(net::SiteForCookies, site_for_cookies)

  // Additional HTTP request headers.
  IPC_STRUCT_MEMBER(std::string, headers)

  // net::URLRequest load flags (0 by default).
  IPC_STRUCT_MEMBER(int, load_flags)

  // Optional upload data (may be null).
  IPC_STRUCT_MEMBER(scoped_refptr<net::UploadData>, upload_data)
IPC_STRUCT_END()

// Tell the renderer to load a request.
IPC_MESSAGE_ROUTED1(CefMsg_LoadRequest, CefMsg_LoadRequest_Params)

// Sent when the browser has a request for the renderer. The renderer may
// respond with a CefHostMsg_Response.
IPC_MESSAGE_ROUTED1(CefMsg_Request, Cef_Request_Params)

// Optional message sent in response to a CefHostMsg_Request.
IPC_MESSAGE_ROUTED1(CefMsg_Response, Cef_Response_Params)

// Optional Ack message sent to the browser to notify that a CefHostMsg_Response
// has been processed.
IPC_MESSAGE_ROUTED1(CefMsg_ResponseAck, int /* request_id */)

// Tells the renderer that loading has stopped.
IPC_MESSAGE_ROUTED0(CefMsg_DidStopLoading)

// Tells the render frame to load all blocked plugins with the given identifier.
// Based on ChromeViewMsg_LoadBlockedPlugins.
IPC_MESSAGE_ROUTED1(CefViewMsg_LoadBlockedPlugins, std::string /* identifier */)

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

  IPC_STRUCT_MEMBER(base::ListValue, extra_info)
IPC_STRUCT_END()

// Retrieve information about a newly created render thread.
IPC_SYNC_MESSAGE_CONTROL0_1(
    CefProcessHostMsg_GetNewRenderThreadInfo,
    CefProcessHostMsg_GetNewRenderThreadInfo_Params /* params*/)

// Parameters for a newly created browser window.
IPC_STRUCT_BEGIN(CefProcessHostMsg_GetNewBrowserInfo_Params)
  IPC_STRUCT_MEMBER(int, browser_id)
  IPC_STRUCT_MEMBER(bool, is_popup)
  IPC_STRUCT_MEMBER(bool, is_windowless)
  IPC_STRUCT_MEMBER(bool, is_guest_view)
  IPC_STRUCT_MEMBER(base::DictionaryValue, extra_info)
IPC_STRUCT_END()

// Retrieve information about a newly created browser.
IPC_SYNC_MESSAGE_CONTROL1_1(
    CefProcessHostMsg_GetNewBrowserInfo,
    int /* render_frame_routing_id */,
    CefProcessHostMsg_GetNewBrowserInfo_Params /* params*/)

// Sent by the renderer when the frame can begin receiving messages.
IPC_MESSAGE_ROUTED0(CefHostMsg_FrameAttached)

// Sent when a frame has finished loading. Based on ViewHostMsg_DidFinishLoad.
IPC_MESSAGE_ROUTED2(CefHostMsg_DidFinishLoad,
                    GURL /* validated_url */,
                    int /* http_status_code */)

// Sent when the renderer has a request for the browser. The browser may respond
// with a CefMsg_Response.
IPC_MESSAGE_ROUTED1(CefHostMsg_Request, Cef_Request_Params)

// Optional message sent in response to a CefMsg_Request.
IPC_MESSAGE_ROUTED1(CefHostMsg_Response, Cef_Response_Params)

// Optional Ack message sent to the browser to notify that a CefMsg_Response
// has been processed.
IPC_MESSAGE_ROUTED1(CefHostMsg_ResponseAck, int /* request_id */)

// Sent by the renderer when the draggable regions are updated.
IPC_MESSAGE_ROUTED1(CefHostMsg_UpdateDraggableRegions,
                    std::vector<Cef_DraggableRegion_Params> /* regions */)
