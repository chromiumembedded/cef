// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "ctocpp/request_ctocpp.h"
#include "transfer_util.h"

std::wstring CefRequestCToCpp::GetURL()
{
  std::wstring str;
  if(CEF_MEMBER_MISSING(struct_, get_url))
    return str;
  
  cef_string_t cef_str = struct_->get_url(struct_);
  if(cef_str) {
    str = cef_str;
    cef_string_free(cef_str);
  }
  return str;
}

void CefRequestCToCpp::SetURL(const std::wstring& url)
{
  if(CEF_MEMBER_MISSING(struct_, set_url))
    return;

  struct_->set_url(struct_, url.c_str());
}

std::wstring CefRequestCToCpp::GetFrame()
{
  std::wstring str;
  if(CEF_MEMBER_MISSING(struct_, get_frame))
    return str;
  
  cef_string_t cef_str = struct_->get_frame(struct_);
  if(cef_str) {
    str = cef_str;
    cef_string_free(cef_str);
  }
  return str;
}

void CefRequestCToCpp::SetFrame(const std::wstring& frame)
{
  if(CEF_MEMBER_MISSING(struct_, set_frame))
    return;

  struct_->set_frame(struct_, frame.c_str());
}

std::wstring CefRequestCToCpp::GetMethod()
{
  std::wstring str;
  if(CEF_MEMBER_MISSING(struct_, get_method))
    return str;
  
  cef_string_t cef_str = struct_->get_method(struct_);
  if(cef_str) {
    str = cef_str;
    cef_string_free(cef_str);
  }
  return str;
}

void CefRequestCToCpp::SetMethod(const std::wstring& method)
{
  if(CEF_MEMBER_MISSING(struct_, set_method))
    return;

  struct_->set_method(struct_, method.c_str());
}

CefRefPtr<CefPostData> CefRequestCToCpp::GetPostData()
{
  if(CEF_MEMBER_MISSING(struct_, get_post_data))
    return NULL;

  cef_post_data_t* postDataStruct = struct_->get_post_data(struct_);
  if(!postDataStruct)
    return NULL;

  CefPostDataCToCpp* pdp = new CefPostDataCToCpp(postDataStruct);
  CefRefPtr<CefPostData> postDataPtr(pdp);
  pdp->UnderlyingRelease();

  return postDataPtr;
}

void CefRequestCToCpp::SetPostData(CefRefPtr<CefPostData> postData)
{
  if(CEF_MEMBER_MISSING(struct_, set_post_data))
    return;

  cef_post_data_t* postDataStruct = NULL;
  if(postData.get()) {
    CefPostDataCToCpp* pdp = static_cast<CefPostDataCToCpp*>(postData.get());
    pdp->UnderlyingAddRef();
    postDataStruct = pdp->GetStruct();
  }
  struct_->set_post_data(struct_, postDataStruct);
}

void CefRequestCToCpp::GetHeaderMap(HeaderMap& headerMap)
{
  if(CEF_MEMBER_MISSING(struct_, get_header_map))
    return;

  cef_string_map_t map = cef_string_map_alloc();
  if(!map)
    return;

  struct_->get_header_map(struct_, map);
  transfer_string_map_contents(map, headerMap);
  cef_string_map_free(map);
}

void CefRequestCToCpp::SetHeaderMap(const HeaderMap& headerMap)
{
  if(CEF_MEMBER_MISSING(struct_, set_header_map))
    return;

  cef_string_map_t map = NULL;
  if(!headerMap.empty()) {
    map = cef_string_map_alloc();
    if(!map)
      return;
    transfer_string_map_contents(headerMap, map);
  }

  struct_->set_header_map(struct_, map);
  
  if(map)
    cef_string_map_free(map);
}

void CefRequestCToCpp::Set(const std::wstring& url,
                           const std::wstring& frame,
                           const std::wstring& method,
                           CefRefPtr<CefPostData> postData,
                           const HeaderMap& headerMap)
{
  if(CEF_MEMBER_MISSING(struct_, set))
    return;

  cef_post_data_t* postDataStruct = NULL;
  if(postData.get()) {
    CefPostDataCToCpp* pdp = static_cast<CefPostDataCToCpp*>(postData.get());
    pdp->UnderlyingAddRef();
    postDataStruct = pdp->GetStruct();
  }

  cef_string_map_t map = NULL;
  if(!headerMap.empty()) {
    map = cef_string_map_alloc();
    if(!map)
      return;
    transfer_string_map_contents(headerMap, map);
  }

  struct_->set(struct_, url.c_str(), frame.c_str(), method.c_str(),
      postDataStruct, map);
  
  if(map)
    cef_string_map_free(map);
}

#ifdef _DEBUG
long CefCToCpp<CefRequest, cef_request_t>::DebugObjCt = 0;
#endif



size_t CefPostDataCToCpp::GetElementCount()
{
  if(CEF_MEMBER_MISSING(struct_, get_element_count))
    return 0;

  return struct_->get_element_count(struct_);
}

void CefPostDataCToCpp::GetElements(ElementVector& elements)
{
  if(CEF_MEMBER_MISSING(struct_, get_element))
    return;

  int count = (int)GetElementCount();

  cef_post_data_element_t* structPtr;
  CefPostDataElementCToCpp* pdep;
  for(int i = 0; i < count; ++i) {
    structPtr = struct_->get_element(struct_, i);
    if(!structPtr)
      continue;

    pdep = new CefPostDataElementCToCpp(structPtr);
    CefRefPtr<CefPostDataElement> elementPtr(pdep);
    pdep->UnderlyingRelease();
    elements.push_back(elementPtr);
  }
}

bool CefPostDataCToCpp::RemoveElement(CefRefPtr<CefPostDataElement> element)
{
  DCHECK(element.get());
  if(CEF_MEMBER_MISSING(struct_, remove_element) || !element.get())
    return false;

  CefPostDataElementCToCpp* pdep =
      static_cast<CefPostDataElementCToCpp*>(element.get());
  pdep->UnderlyingAddRef();
  return struct_->remove_element(struct_, pdep->GetStruct());
}

bool CefPostDataCToCpp::AddElement(CefRefPtr<CefPostDataElement> element)
{
  DCHECK(element.get());
  if(CEF_MEMBER_MISSING(struct_, add_element) || !element.get())
    return false;

  CefPostDataElementCToCpp* pdep =
      static_cast<CefPostDataElementCToCpp*>(element.get());
  pdep->UnderlyingAddRef();
  return struct_->add_element(struct_, pdep->GetStruct());
}

void CefPostDataCToCpp::RemoveElements()
{
  if(CEF_MEMBER_MISSING(struct_, remove_elements))
    return;

  return struct_->remove_elements(struct_);
}

#ifdef _DEBUG
long CefCToCpp<CefPostData, cef_post_data_t>::DebugObjCt = 0;
#endif


void CefPostDataElementCToCpp::SetToEmpty()
{
  if(CEF_MEMBER_MISSING(struct_, set_to_empty))
    return;

  return struct_->set_to_empty(struct_);
}

void CefPostDataElementCToCpp::SetToFile(const std::wstring& fileName)
{
  if(CEF_MEMBER_MISSING(struct_, set_to_file))
    return;

  return struct_->set_to_file(struct_, fileName.c_str());
}

void CefPostDataElementCToCpp::SetToBytes(size_t size, const void* bytes)
{
  if(CEF_MEMBER_MISSING(struct_, set_to_bytes))
    return;

  return struct_->set_to_bytes(struct_, size, bytes);
}

CefPostDataElement::Type CefPostDataElementCToCpp::GetType()
{
  if(CEF_MEMBER_MISSING(struct_, get_type))
    return PDE_TYPE_EMPTY;

  return struct_->get_type(struct_);
}

std::wstring CefPostDataElementCToCpp::GetFile()
{
  std::wstring str;
  if(CEF_MEMBER_MISSING(struct_, get_file))
    return str;

  cef_string_t cef_str = struct_->get_file(struct_);
  if(cef_str) {
    str = cef_str;
    cef_string_free(cef_str);
  }

  return str;
}

size_t CefPostDataElementCToCpp::GetBytesCount()
{
  if(CEF_MEMBER_MISSING(struct_, get_bytes_count))
    return 0;

  return struct_->get_bytes_count(struct_);
}

size_t CefPostDataElementCToCpp::GetBytes(size_t size, void *bytes)
{
  if(CEF_MEMBER_MISSING(struct_, get_bytes))
    return 0;

  return struct_->get_bytes(struct_, size, bytes);
}

#ifdef _DEBUG
long CefCToCpp<CefPostDataElement, cef_post_data_element_t>::DebugObjCt = 0;
#endif
