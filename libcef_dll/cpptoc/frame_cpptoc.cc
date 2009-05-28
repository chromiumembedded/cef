// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/frame_cpptoc.h"
#include "cpptoc/request_cpptoc.h"
#include "cpptoc/stream_cpptoc.h"


void CEF_CALLBACK frame_undo(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return;

  CefFrameCppToC::Get(frame)->Undo();
}

void CEF_CALLBACK frame_redo(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return;

  CefFrameCppToC::Get(frame)->Redo();
}

void CEF_CALLBACK frame_cut(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return;

  CefFrameCppToC::Get(frame)->Cut();
}

void CEF_CALLBACK frame_copy(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return;

  CefFrameCppToC::Get(frame)->Copy();
}

void CEF_CALLBACK frame_paste(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return;

  CefFrameCppToC::Get(frame)->Paste();
}

void CEF_CALLBACK frame_delete(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return;

  CefFrameCppToC::Get(frame)->Delete();
}

void CEF_CALLBACK frame_select_all(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return;

  CefFrameCppToC::Get(frame)->SelectAll();
}

void CEF_CALLBACK frame_print(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return;

  CefFrameCppToC::Get(frame)->Print();
}

void CEF_CALLBACK frame_view_source(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return;

  CefFrameCppToC::Get(frame)->ViewSource();
}

cef_string_t CEF_CALLBACK frame_get_source(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return NULL;

  std::wstring sourceStr = CefFrameCppToC::Get(frame)->GetSource();
  if(!sourceStr.empty())
    return cef_string_alloc(sourceStr.c_str());
  return NULL;
}

cef_string_t CEF_CALLBACK frame_get_text(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return NULL;

  std::wstring textStr = CefFrameCppToC::Get(frame)->GetText();
  if(!textStr.empty())
    return cef_string_alloc(textStr.c_str());
  return NULL;
}

void CEF_CALLBACK frame_load_request(cef_frame_t* frame,
                                     cef_request_t* request)
{
  DCHECK(frame);
  DCHECK(request);
  if(!frame || !request)
    return;

  CefRefPtr<CefRequest> requestPtr = CefRequestCppToC::Unwrap(request);
  CefFrameCppToC::Get(frame)->LoadRequest(requestPtr);
}

void CEF_CALLBACK frame_load_url(cef_frame_t* frame, const wchar_t* url)
{
  DCHECK(frame);
  if(!frame)
    return;

  std::wstring urlStr;
  if(url)
    urlStr = url;
  CefFrameCppToC::Get(frame)->LoadURL(urlStr);
}

void CEF_CALLBACK frame_load_string(cef_frame_t* frame,
                                    const wchar_t* string,
                                    const wchar_t* url)
{
  DCHECK(frame);
  if(!frame)
    return;

  std::wstring stringStr, urlStr;
  if(string)
    stringStr = string;
  if(url)
    urlStr = url;
  CefFrameCppToC::Get(frame)->LoadString(stringStr, urlStr);
}

void CEF_CALLBACK frame_load_stream(cef_frame_t* frame,
                                    cef_stream_reader_t* stream,
                                    const wchar_t* url)
{
  DCHECK(frame);
  DCHECK(stream);
  if(!frame || !stream)
    return;

  CefRefPtr<CefStreamReader> streamPtr = CefStreamReaderCppToC::Unwrap(stream);
  std::wstring urlStr;
  if(url)
    urlStr = url;
  
  CefFrameCppToC::Get(frame)->LoadStream(streamPtr, urlStr);
}

void CEF_CALLBACK frame_execute_javascript(cef_frame_t* frame,
                                           const wchar_t* jsCode,
                                           const wchar_t* scriptUrl,
                                           int startLine)
{
  DCHECK(frame);
  if(!frame)
    return;

  std::wstring jsCodeStr, scriptUrlStr;
  if(jsCode)
    jsCodeStr = jsCode;
  if(scriptUrl)
    scriptUrlStr = scriptUrl;

  CefFrameCppToC::Get(frame)->ExecuteJavaScript(jsCodeStr, scriptUrlStr,
      startLine);
}

int CEF_CALLBACK frame_is_main(struct _cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return 0;

  return CefFrameCppToC::Get(frame)->IsMain();
}

int CEF_CALLBACK frame_is_focused(struct _cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return 0;

  return CefFrameCppToC::Get(frame)->IsFocused();
}


cef_string_t CEF_CALLBACK frame_get_name(struct _cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return 0;

  std::wstring nameStr = CefFrameCppToC::Get(frame)->GetName();
  if(!nameStr.empty())
    return cef_string_alloc(nameStr.c_str());
  return NULL;
}

cef_string_t CEF_CALLBACK frame_get_url(cef_frame_t* frame)
{
  DCHECK(frame);
  if(!frame)
    return NULL;

  std::wstring urlStr = CefFrameCppToC::Get(frame)->GetURL();
  if(!urlStr.empty())
    return cef_string_alloc(urlStr.c_str());
  return NULL;
}

CefFrameCppToC::CefFrameCppToC(CefFrame* cls)
    : CefCppToC<CefFrameCppToC, CefFrame, cef_frame_t>(cls)
{
  struct_.struct_.undo = frame_undo;
  struct_.struct_.redo = frame_redo;
  struct_.struct_.cut = frame_cut;
  struct_.struct_.copy = frame_copy;
  struct_.struct_.paste = frame_paste;
  struct_.struct_.del = frame_delete;
  struct_.struct_.select_all = frame_select_all;
  struct_.struct_.print = frame_print;
  struct_.struct_.view_source = frame_view_source;
  struct_.struct_.get_source = frame_get_source;
  struct_.struct_.get_text = frame_get_text;
  struct_.struct_.load_request = frame_load_request;
  struct_.struct_.load_url = frame_load_url;
  struct_.struct_.load_string = frame_load_string;
  struct_.struct_.load_stream = frame_load_stream;
  struct_.struct_.execute_javascript = frame_execute_javascript;
  struct_.struct_.is_main = frame_is_main;
  struct_.struct_.is_focused = frame_is_focused;
  struct_.struct_.get_name = frame_get_name;
  struct_.struct_.get_url = frame_get_url;
}

#ifdef _DEBUG
long CefCppToC<CefFrameCppToC, CefFrame, cef_frame_t>::DebugObjCt = 0;
#endif
