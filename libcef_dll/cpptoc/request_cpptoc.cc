// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/request_cpptoc.h"
#include "transfer_util.h"
#include "base/logging.h"


cef_string_t CEF_CALLBACK request_get_url(struct _cef_request_t* request)
{
  DCHECK(request);
  if(!request)
    return NULL;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);

  std::wstring urlStr = impl->class_->GetClass()->GetURL();
  if(urlStr.empty())
    return NULL;
  return cef_string_alloc(urlStr.c_str());
}

void CEF_CALLBACK request_set_url(struct _cef_request_t* request,
                                  const wchar_t* url)
{
  DCHECK(request);
  if(!request)
    return;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);

  std::wstring urlStr;
  if(url)
    urlStr = url;
  impl->class_->GetClass()->SetURL(urlStr);
}

cef_string_t CEF_CALLBACK request_get_frame(struct _cef_request_t* request)
{
  DCHECK(request);
  if(!request)
    return NULL;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);

  std::wstring frameStr = impl->class_->GetClass()->GetFrame();
  if(frameStr.empty())
    return NULL;
  return cef_string_alloc(frameStr.c_str());
}

void CEF_CALLBACK request_set_frame(struct _cef_request_t* request,
                            const wchar_t* frame)
{
  DCHECK(request);
  if(!request)
    return;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);
  
  std::wstring frameStr;
  if(frame)
    frameStr = frame;
  impl->class_->GetClass()->SetFrame(frameStr);
}

cef_string_t CEF_CALLBACK request_get_method(struct _cef_request_t* request)
{
  DCHECK(request);
  if(!request)
    return NULL;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);
  
  std::wstring methodStr = impl->class_->GetClass()->GetMethod();
  if(methodStr.empty())
    return NULL;
  return cef_string_alloc(methodStr.c_str());
}

void CEF_CALLBACK request_set_method(struct _cef_request_t* request,
                                     const wchar_t* method)
{
  DCHECK(request);
  if(!request)
    return;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);
  
  std::wstring methodStr;
  if(method)
    methodStr = method;
  impl->class_->GetClass()->SetMethod(methodStr);
}

struct _cef_post_data_t* CEF_CALLBACK request_get_post_data(
    struct _cef_request_t* request)
{
  DCHECK(request);
  if(!request)
    return NULL;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);
  
  CefRefPtr<CefPostData> postdata =
      impl->class_->GetClass()->GetPostData();
  if(!postdata.get())
    return NULL;

  CefPostDataCppToC* rp = new CefPostDataCppToC(postdata);
  rp->AddRef();
  return rp->GetStruct();
}

void CEF_CALLBACK request_set_post_data(struct _cef_request_t* request,
                                        struct _cef_post_data_t* postData)
{
  DCHECK(request);
  if(!request)
    return;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);
  CefPostDataCppToC::Struct* postStructPtr =
      reinterpret_cast<CefPostDataCppToC::Struct*>(postData);
  
  CefRefPtr<CefPostData> postDataPtr;
  if(postStructPtr)
    postDataPtr = postStructPtr->class_->GetClass();
  
  impl->class_->GetClass()->SetPostData(postDataPtr);
}

void CEF_CALLBACK request_get_header_map(struct _cef_request_t* request,
                                         cef_string_map_t headerMap)
{
  DCHECK(request);
  if(!request)
    return;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);
  
  CefRequest::HeaderMap map;
  impl->class_->GetClass()->GetHeaderMap(map);

  transfer_string_map_contents(map, headerMap);
}

void CEF_CALLBACK request_set_header_map(struct _cef_request_t* request,
                                         cef_string_map_t headerMap)
{
  DCHECK(request);
  if(!request)
    return;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);
  
  CefRequest::HeaderMap map;
  if(headerMap)
    transfer_string_map_contents(headerMap, map);

  impl->class_->GetClass()->SetHeaderMap(map);
}

void CEF_CALLBACK request_set(struct _cef_request_t* request,
                              const wchar_t* url, const wchar_t* frame,
                              const wchar_t* method,
                              struct _cef_post_data_t* postData,
                              cef_string_map_t headerMap)
{
  DCHECK(request);
  if(!request)
    return;

  CefRequestCppToC::Struct* impl =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);
  CefPostDataCppToC::Struct* postStructPtr =
      reinterpret_cast<CefPostDataCppToC::Struct*>(postData);
  
  std::wstring urlStr, frameStr, methodStr;
  CefRefPtr<CefPostData> postPtr;
  CefRequest::HeaderMap map;

  if(url)
    urlStr = url;
  if(frame)
    frameStr = frame;
  if(method)
    methodStr = method;  
  if(postStructPtr)
    postPtr = postStructPtr->class_->GetClass();
  if(headerMap)
    transfer_string_map_contents(headerMap, map);

  impl->class_->GetClass()->Set(urlStr, frameStr, methodStr, postPtr, map);
}


CefRequestCppToC::CefRequestCppToC(CefRefPtr<CefRequest> cls)
    : CefCppToC<CefRequest, cef_request_t>(cls)
{
  struct_.struct_.get_url = request_get_url;
  struct_.struct_.set_url = request_set_url;
  struct_.struct_.get_frame = request_get_frame;
  struct_.struct_.set_frame = request_set_frame;
  struct_.struct_.get_method = request_get_method;
  struct_.struct_.set_method = request_set_method;
  struct_.struct_.get_post_data = request_get_post_data;
  struct_.struct_.set_post_data = request_set_post_data;
  struct_.struct_.get_header_map = request_get_header_map;
  struct_.struct_.set_header_map = request_set_header_map;
  struct_.struct_.set = request_set;
}



size_t CEF_CALLBACK post_data_get_element_count(
    struct _cef_post_data_t* postData)
{
  DCHECK(postData);
  if(!postData)
    return 0;

  CefPostDataCppToC::Struct* impl =
      reinterpret_cast<CefPostDataCppToC::Struct*>(postData);
  return impl->class_->GetClass()->GetElementCount();
}

struct _cef_post_data_element_t* CEF_CALLBACK post_data_get_element(
    struct _cef_post_data_t* postData, int index)
{
  DCHECK(postData);
  if(!postData)
    return NULL;

  CefPostDataCppToC::Struct* impl =
      reinterpret_cast<CefPostDataCppToC::Struct*>(postData);
  
  CefPostData::ElementVector elements;
  impl->class_->GetClass()->GetElements(elements);

  if(index < 0 || index >= (int)elements.size())
    return NULL;

  CefPostDataElementCppToC* rp = new CefPostDataElementCppToC(elements[index]);
  rp->AddRef();
  return rp->GetStruct();
}

int CEF_CALLBACK post_data_remove_element(struct _cef_post_data_t* postData,
    struct _cef_post_data_element_t* element)
{
  DCHECK(postData);
  DCHECK(element);
  if(!postData || !element)
    return 0;

  CefPostDataCppToC::Struct* impl =
      reinterpret_cast<CefPostDataCppToC::Struct*>(postData);
  CefPostDataElementCppToC::Struct* structPtr =
      reinterpret_cast<CefPostDataElementCppToC::Struct*>(element);
  
  return impl->class_->GetClass()->RemoveElement(structPtr->class_->GetClass());
}

int CEF_CALLBACK post_data_add_element(struct _cef_post_data_t* postData,
    struct _cef_post_data_element_t* element)
{
  DCHECK(postData);
  DCHECK(element);
  if(!postData || !element)
    return 0;

  CefPostDataCppToC::Struct* impl =
      reinterpret_cast<CefPostDataCppToC::Struct*>(postData);
  CefPostDataElementCppToC::Struct* structPtr =
      reinterpret_cast<CefPostDataElementCppToC::Struct*>(element);

   return impl->class_->GetClass()->AddElement(structPtr->class_->GetClass());
}

void CEF_CALLBACK post_data_remove_elements(struct _cef_post_data_t* postData)
{
  DCHECK(postData);
  if(!postData)
    return;

  CefPostDataCppToC::Struct* impl =
      reinterpret_cast<CefPostDataCppToC::Struct*>(postData);
  
  impl->class_->GetClass()->RemoveElements();
}


CefPostDataCppToC::CefPostDataCppToC(CefRefPtr<CefPostData> cls)
    : CefCppToC<CefPostData, cef_post_data_t>(cls)
{
  struct_.struct_.get_element_count = post_data_get_element_count;
  struct_.struct_.get_element = post_data_get_element;
  struct_.struct_.remove_element = post_data_remove_element;
  struct_.struct_.add_element = post_data_add_element;
  struct_.struct_.remove_elements = post_data_remove_elements;
}



void CEF_CALLBACK post_data_element_set_to_empty(
    struct _cef_post_data_element_t* postDataElement)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return;

  CefPostDataElementCppToC::Struct* impl =
      reinterpret_cast<CefPostDataElementCppToC::Struct*>(postDataElement);
  impl->class_->GetClass()->SetToEmpty();
}

void CEF_CALLBACK post_data_element_set_to_file(
    struct _cef_post_data_element_t* postDataElement,
    const wchar_t* fileName)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return;

  CefPostDataElementCppToC::Struct* impl =
      reinterpret_cast<CefPostDataElementCppToC::Struct*>(postDataElement);
  
  std::wstring fileNameStr;
  if(fileName)
    fileNameStr = fileName;
  impl->class_->GetClass()->SetToFile(fileNameStr);
}

void CEF_CALLBACK post_data_element_set_to_bytes(
    struct _cef_post_data_element_t* postDataElement, size_t size,
    const void* bytes)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return;

  CefPostDataElementCppToC::Struct* impl =
      reinterpret_cast<CefPostDataElementCppToC::Struct*>(postDataElement);
  impl->class_->GetClass()->SetToBytes(size, bytes);
}

cef_postdataelement_type_t CEF_CALLBACK post_data_element_get_type(
    struct _cef_post_data_element_t* postDataElement)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return PDE_TYPE_EMPTY;

  CefPostDataElementCppToC::Struct* impl =
      reinterpret_cast<CefPostDataElementCppToC::Struct*>(postDataElement);
  return impl->class_->GetClass()->GetType();
}

cef_string_t CEF_CALLBACK post_data_element_get_file(
    struct _cef_post_data_element_t* postDataElement)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return NULL;

  CefPostDataElementCppToC::Struct* impl =
      reinterpret_cast<CefPostDataElementCppToC::Struct*>(postDataElement);
  
  std::wstring fileNameStr = impl->class_->GetClass()->GetFile();
  if(fileNameStr.empty())
    return NULL;
  return cef_string_alloc(fileNameStr.c_str());
}

size_t CEF_CALLBACK post_data_element_get_bytes_count(
    struct _cef_post_data_element_t* postDataElement)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return 0;

  CefPostDataElementCppToC::Struct* impl =
      reinterpret_cast<CefPostDataElementCppToC::Struct*>(postDataElement);
  return impl->class_->GetClass()->GetBytesCount();
}

size_t CEF_CALLBACK post_data_element_get_bytes(
    struct _cef_post_data_element_t* postDataElement, size_t size,
    void *bytes)
{
  DCHECK(postDataElement);
  if(!postDataElement)
    return 0;

  CefPostDataElementCppToC::Struct* impl =
      reinterpret_cast<CefPostDataElementCppToC::Struct*>(postDataElement);
  return impl->class_->GetClass()->GetBytes(size, bytes);
}


CefPostDataElementCppToC::CefPostDataElementCppToC(
    CefRefPtr<CefPostDataElement> cls)
    : CefCppToC<CefPostDataElement, cef_post_data_element_t>(cls)
{
  struct_.struct_.set_to_empty = post_data_element_set_to_empty;
  struct_.struct_.set_to_file = post_data_element_set_to_file;
  struct_.struct_.set_to_bytes = post_data_element_set_to_bytes;
  struct_.struct_.get_type = post_data_element_get_type;
  struct_.struct_.get_file = post_data_element_get_file;
  struct_.struct_.get_bytes_count = post_data_element_get_bytes_count;
  struct_.struct_.get_bytes = post_data_element_get_bytes;
}
