// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/request_cpptoc.h"
#include "transfer_util.h"


cef_string_t CEF_CALLBACK request_get_url(struct _cef_request_t* request)
{
  DCHECK(request);
  if(!request)
    return NULL;

  std::wstring urlStr = CefRequestCppToC::Get(request)->GetURL();
  if(!urlStr.empty())
    return cef_string_alloc(urlStr.c_str());
  return NULL;
}

void CEF_CALLBACK request_set_url(struct _cef_request_t* request,
                                  const wchar_t* url)
{
  DCHECK(request);
  if(!request)
    return;

  std::wstring urlStr;
  if(url)
    urlStr = url;
  CefRequestCppToC::Get(request)->SetURL(urlStr);
}

cef_string_t CEF_CALLBACK request_get_method(struct _cef_request_t* request)
{
  DCHECK(request);
  if(!request)
    return NULL;

  std::wstring methodStr = CefRequestCppToC::Get(request)->GetMethod();
  if(!methodStr.empty())
    return cef_string_alloc(methodStr.c_str());
  return NULL;
}

void CEF_CALLBACK request_set_method(struct _cef_request_t* request,
                                     const wchar_t* method)
{
  DCHECK(request);
  if(!request)
    return;

  std::wstring methodStr;
  if(method)
    methodStr = method;
  CefRequestCppToC::Get(request)->SetMethod(methodStr);
}

struct _cef_post_data_t* CEF_CALLBACK request_get_post_data(
    struct _cef_request_t* request)
{
  DCHECK(request);
  if(!request)
    return NULL;

  CefRefPtr<CefPostData> postDataPtr =
      CefRequestCppToC::Get(request)->GetPostData();
  if(!postDataPtr.get())
    return NULL;

  return CefPostDataCppToC::Wrap(postDataPtr);
}

void CEF_CALLBACK request_set_post_data(struct _cef_request_t* request,
                                        struct _cef_post_data_t* postData)
{
  DCHECK(request);
  if(!request)
    return;

  CefRefPtr<CefPostData> postDataPtr;
  if(postData)
    postDataPtr = CefPostDataCppToC::Unwrap(postData);
  
  CefRequestCppToC::Get(request)->SetPostData(postDataPtr);
}

void CEF_CALLBACK request_get_header_map(struct _cef_request_t* request,
                                         cef_string_map_t headerMap)
{
  DCHECK(request);
  if(!request)
    return;

  CefRequest::HeaderMap map;
  CefRequestCppToC::Get(request)->GetHeaderMap(map);
  transfer_string_map_contents(map, headerMap);
}

void CEF_CALLBACK request_set_header_map(struct _cef_request_t* request,
                                         cef_string_map_t headerMap)
{
  DCHECK(request);
  if(!request)
    return;

  CefRequest::HeaderMap map;
  if(headerMap)
    transfer_string_map_contents(headerMap, map);

  CefRequestCppToC::Get(request)->SetHeaderMap(map);
}

void CEF_CALLBACK request_set(struct _cef_request_t* request,
                              const wchar_t* url, const wchar_t* method,
                              struct _cef_post_data_t* postData,
                              cef_string_map_t headerMap)
{
  DCHECK(request);
  if(!request)
    return;

  std::wstring urlStr, methodStr;
  CefRefPtr<CefPostData> postDataPtr;
  CefRequest::HeaderMap map;

  if(url)
    urlStr = url;
  if(method)
    methodStr = method;  
  if(postData)
    postDataPtr = CefPostDataCppToC::Unwrap(postData);
  if(headerMap)
    transfer_string_map_contents(headerMap, map);

  CefRequestCppToC::Get(request)->Set(urlStr, methodStr, postDataPtr, map);
}


CefRequestCppToC::CefRequestCppToC(CefRequest* cls)
    : CefCppToC<CefRequestCppToC, CefRequest, cef_request_t>(cls)
{
  struct_.struct_.get_url = request_get_url;
  struct_.struct_.set_url = request_set_url;
  struct_.struct_.get_method = request_get_method;
  struct_.struct_.set_method = request_set_method;
  struct_.struct_.get_post_data = request_get_post_data;
  struct_.struct_.set_post_data = request_set_post_data;
  struct_.struct_.get_header_map = request_get_header_map;
  struct_.struct_.set_header_map = request_set_header_map;
  struct_.struct_.set = request_set;
}

#ifdef _DEBUG
long CefCppToC<CefRequestCppToC, CefRequest, cef_request_t>::DebugObjCt = 0;
#endif


size_t CEF_CALLBACK post_data_get_element_count(
    struct _cef_post_data_t* postData)
{
  DCHECK(postData);
  if(!postData)
    return 0;

  return CefPostDataCppToC::Get(postData)->GetElementCount();
}

struct _cef_post_data_element_t* CEF_CALLBACK post_data_get_element(
    struct _cef_post_data_t* postData, int index)
{
  DCHECK(postData);
  if(!postData)
    return NULL;

  CefPostData::ElementVector elements;
  CefPostDataCppToC::Get(postData)->GetElements(elements);

  if(index < 0 || index >= (int)elements.size())
    return NULL;

  return CefPostDataElementCppToC::Wrap(elements[index]);
}

int CEF_CALLBACK post_data_remove_element(struct _cef_post_data_t* postData,
    struct _cef_post_data_element_t* element)
{
  DCHECK(postData);
  DCHECK(element);
  if(!postData || !element)
    return 0;

  CefRefPtr<CefPostDataElement> postDataElementPtr =
      CefPostDataElementCppToC::Unwrap(element);
  return CefPostDataCppToC::Get(postData)->RemoveElement(postDataElementPtr);
}

int CEF_CALLBACK post_data_add_element(struct _cef_post_data_t* postData,
    struct _cef_post_data_element_t* element)
{
  DCHECK(postData);
  DCHECK(element);
  if(!postData || !element)
    return 0;

  CefRefPtr<CefPostDataElement> postDataElementPtr =
      CefPostDataElementCppToC::Unwrap(element);
  return CefPostDataCppToC::Get(postData)->AddElement(postDataElementPtr);
}

void CEF_CALLBACK post_data_remove_elements(struct _cef_post_data_t* postData)
{
  DCHECK(postData);
  if(!postData)
    return;

  CefPostDataCppToC::Get(postData)->RemoveElements();
}


CefPostDataCppToC::CefPostDataCppToC(CefPostData* cls)
    : CefCppToC<CefPostDataCppToC, CefPostData, cef_post_data_t>(cls)
{
  struct_.struct_.get_element_count = post_data_get_element_count;
  struct_.struct_.get_element = post_data_get_element;
  struct_.struct_.remove_element = post_data_remove_element;
  struct_.struct_.add_element = post_data_add_element;
  struct_.struct_.remove_elements = post_data_remove_elements;
}

#ifdef _DEBUG
long CefCppToC<CefPostDataCppToC, CefPostData, cef_post_data_t>::DebugObjCt
    = 0;
#endif


void CEF_CALLBACK post_data_element_set_to_empty(
    struct _cef_post_data_element_t* postDataElement)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return;

  CefPostDataElementCppToC::Get(postDataElement)->SetToEmpty();
}

void CEF_CALLBACK post_data_element_set_to_file(
    struct _cef_post_data_element_t* postDataElement,
    const wchar_t* fileName)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return;

  std::wstring fileNameStr;
  if(fileName)
    fileNameStr = fileName;

  CefPostDataElementCppToC::Get(postDataElement)->SetToFile(fileNameStr);
}

void CEF_CALLBACK post_data_element_set_to_bytes(
    struct _cef_post_data_element_t* postDataElement, size_t size,
    const void* bytes)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return;

  CefPostDataElementCppToC::Get(postDataElement)->SetToBytes(size, bytes);
}

cef_postdataelement_type_t CEF_CALLBACK post_data_element_get_type(
    struct _cef_post_data_element_t* postDataElement)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return PDE_TYPE_EMPTY;

  return CefPostDataElementCppToC::Get(postDataElement)->GetType();
}

cef_string_t CEF_CALLBACK post_data_element_get_file(
    struct _cef_post_data_element_t* postDataElement)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return NULL;

  std::wstring fileNameStr =
      CefPostDataElementCppToC::Get(postDataElement)->GetFile();
  if(!fileNameStr.empty())
    return cef_string_alloc(fileNameStr.c_str());
  return NULL;
}

size_t CEF_CALLBACK post_data_element_get_bytes_count(
    struct _cef_post_data_element_t* postDataElement)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return 0;

  return CefPostDataElementCppToC::Get(postDataElement)->GetBytesCount();
}

size_t CEF_CALLBACK post_data_element_get_bytes(
    struct _cef_post_data_element_t* postDataElement, size_t size,
    void *bytes)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return 0;

  return CefPostDataElementCppToC::Get(postDataElement)->GetBytes(size, bytes);
}


CefPostDataElementCppToC::CefPostDataElementCppToC(CefPostDataElement* cls)
    : CefCppToC<CefPostDataElementCppToC, CefPostDataElement,
                cef_post_data_element_t>(cls)
{
  struct_.struct_.set_to_empty = post_data_element_set_to_empty;
  struct_.struct_.set_to_file = post_data_element_set_to_file;
  struct_.struct_.set_to_bytes = post_data_element_set_to_bytes;
  struct_.struct_.get_type = post_data_element_get_type;
  struct_.struct_.get_file = post_data_element_get_file;
  struct_.struct_.get_bytes_count = post_data_element_get_bytes_count;
  struct_.struct_.get_bytes = post_data_element_get_bytes;
}

#ifdef _DEBUG
long CefCppToC<CefPostDataElementCppToC, CefPostDataElement,
    cef_post_data_element_t>::DebugObjCt = 0;
#endif
