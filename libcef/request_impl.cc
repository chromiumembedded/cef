// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "precompiled_libcef.h"
#include "request_impl.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/api/public/WebHTTPHeaderVisitor.h"
#include "webkit/glue/glue_util.h"


CefRefPtr<CefRequest> CefRequest::CreateRequest()
{
  CefRefPtr<CefRequest> request(new CefRequestImpl());
  return request;
}

CefRequestImpl::CefRequestImpl()
{
}

std::wstring CefRequestImpl::GetURL()
{
  Lock();
  std::wstring url = url_;
  Unlock();
  return url;
}

void CefRequestImpl::SetURL(const std::wstring& url)
{
  Lock();
  url_ = url;
  Unlock();
}

std::wstring CefRequestImpl::GetMethod()
{
  Lock();
  std::wstring method = method_;
  Unlock();
  return method;
}

void CefRequestImpl::SetMethod(const std::wstring& method)
{
  Lock();
  method_ = method;
  Unlock();
}

CefRefPtr<CefPostData> CefRequestImpl::GetPostData()
{
  Lock();
  CefRefPtr<CefPostData> postData = postdata_;
  Unlock();
  return postData;
}

void CefRequestImpl::SetPostData(CefRefPtr<CefPostData> postData)
{
  Lock();
  postdata_ = postData;
  Unlock();
}

void CefRequestImpl::GetHeaderMap(HeaderMap& headerMap)
{
  Lock();
  headerMap = headermap_;
  Unlock();
}

void CefRequestImpl::SetHeaderMap(const HeaderMap& headerMap)
{
  Lock();
  headermap_ = headerMap;
  Unlock();
}

void CefRequestImpl::Set(const std::wstring& url,
                         const std::wstring& method,
                         CefRefPtr<CefPostData> postData,
                         const HeaderMap& headerMap)
{
  Lock();
  url_ = url;
  method_ = method;
  postdata_ = postData;
  headermap_ = headerMap;
  Unlock();
}

void CefRequestImpl::GetHeaderMap(const WebKit::WebURLRequest& request,
                                  HeaderMap& map)
{
  class CefHTTPHeaderVisitor : public WebKit::WebHTTPHeaderVisitor {
    public:
      CefHTTPHeaderVisitor(HeaderMap* map) : map_(map) {}

      virtual void visitHeader(const WebKit::WebString& name,
                               const WebKit::WebString& value) {
        map_->insert(
            std::make_pair(
                UTF8ToWide(webkit_glue::WebStringToStdString(name)),
                UTF8ToWide(webkit_glue::WebStringToStdString(value))));
      }

    private:
      HeaderMap* map_;
  };

  CefHTTPHeaderVisitor visitor(&map);
  request.visitHTTPHeaderFields(&visitor);
}

void CefRequestImpl::SetHeaderMap(const HeaderMap& map,
                                  WebKit::WebURLRequest& request)
{
  HeaderMap::const_iterator it = map.begin();
  for(; it != map.end(); ++it) {
    request.setHTTPHeaderField(
        webkit_glue::StdStringToWebString(WideToUTF8(it->first.c_str())),
        webkit_glue::StdStringToWebString(WideToUTF8(it->second.c_str())));
  }
}

CefRefPtr<CefPostData> CefPostData::CreatePostData()
{
  CefRefPtr<CefPostData> postdata(new CefPostDataImpl());
  return postdata;
}

CefPostDataImpl::CefPostDataImpl()
{
}

size_t CefPostDataImpl::GetElementCount()
{
  Lock();
  size_t ct = elements_.size();
  Unlock();
  return ct;
}

void CefPostDataImpl::GetElements(ElementVector& elements)
{
  Lock();
  elements = elements_;
  Unlock();
}

bool CefPostDataImpl::RemoveElement(CefRefPtr<CefPostDataElement> element)
{
  bool deleted = false;

  Lock();

  ElementVector::iterator it = elements_.begin();
  for(; it != elements_.end(); ++it) {
    if(it->get() == element.get()) {
      elements_.erase(it);
      deleted = true;
      break;
    }
  }

  Unlock();
  
  return deleted;
}

bool CefPostDataImpl::AddElement(CefRefPtr<CefPostDataElement> element)
{
  bool found = false;
  
  Lock();
  
  // check that the element isn't already in the list before adding
  ElementVector::const_iterator it = elements_.begin();
  for(; it != elements_.end(); ++it) {
    if(it->get() == element.get()) {
      found = true;
      break;
    }
  }

  if(!found)
    elements_.push_back(element);
 
  Unlock();
  return !found;
}

void CefPostDataImpl::RemoveElements()
{
  Lock();
  elements_.clear();
  Unlock();
}

void CefPostDataImpl::Set(const net::UploadData& data)
{
  Lock();

  CefRefPtr<CefPostDataElement> postelem;
  const std::vector<net::UploadData::Element>& elements = data.elements();
  std::vector<net::UploadData::Element>::const_iterator it = elements.begin();
  for (; it != elements.end(); ++it) {
    postelem = CefPostDataElement::CreatePostDataElement();
    static_cast<CefPostDataElementImpl*>(postelem.get())->Set(*it);
    AddElement(postelem);
  }

  Unlock();
}

void CefPostDataImpl::Get(net::UploadData& data)
{
  Lock();

  net::UploadData::Element element;
  std::vector<net::UploadData::Element> data_elements;
  ElementVector::iterator it = elements_.begin();
  for(; it != elements_.end(); ++it) {
    static_cast<CefPostDataElementImpl*>(it->get())->Get(element);
    data_elements.push_back(element);
  }
  data.set_elements(data_elements);

  Unlock();
}

void CefPostDataImpl::Set(const WebKit::WebHTTPBody& data)
{
  Lock();

  CefRefPtr<CefPostDataElement> postelem;
  WebKit::WebHTTPBody::Element element;
  size_t size = data.elementCount();
  for (size_t i = 0; i < size; ++i) {
    if (data.elementAt(i, element)) {
      postelem = CefPostDataElement::CreatePostDataElement();
      static_cast<CefPostDataElementImpl*>(postelem.get())->Set(element);
      AddElement(postelem);
    }
  }

  Unlock();
}

void CefPostDataImpl::Get(WebKit::WebHTTPBody& data)
{
  Lock();

  WebKit::WebHTTPBody::Element element;
  ElementVector::iterator it = elements_.begin();
  for(; it != elements_.end(); ++it) {
    static_cast<CefPostDataElementImpl*>(it->get())->Get(element);
    if(element.type == WebKit::WebHTTPBody::Element::TypeData) {
      data.appendData(element.data);
    } else if(element.type == WebKit::WebHTTPBody::Element::TypeFile) {
      data.appendFile(element.filePath);
    } else {
      NOTREACHED();
    }
  }

  Unlock();
}

CefRefPtr<CefPostDataElement> CefPostDataElement::CreatePostDataElement()
{
  CefRefPtr<CefPostDataElement> element(new CefPostDataElementImpl());
  return element;
}

CefPostDataElementImpl::CefPostDataElementImpl()
{
  type_ = PDE_TYPE_EMPTY;
}

CefPostDataElementImpl::~CefPostDataElementImpl()
{
  SetToEmpty();
}

void CefPostDataElementImpl::SetToEmpty()
{
  Lock();
  if(type_ == PDE_TYPE_BYTES)
    free(data_.bytes.bytes);
  else if(type_ == PDE_TYPE_FILE)
    free(data_.filename);
  type_ = PDE_TYPE_EMPTY;
  Unlock();
}

void CefPostDataElementImpl::SetToFile(const std::wstring& fileName)
{
  Lock();
  // Clear any data currently in the element
  SetToEmpty();

  // Assign the new file name
  size_t size = fileName.size();
  wchar_t* data = static_cast<wchar_t*>(malloc((size + 1) * sizeof(wchar_t)));
  DCHECK(data != NULL);
  if(data == NULL)
    return;

  memcpy(static_cast<void*>(data), static_cast<const void*>(fileName.c_str()),
      size * sizeof(wchar_t));
  data[size] = 0;

  // Assign the new data
  type_ = PDE_TYPE_FILE;
  data_.filename = data;
  Unlock();
}

void CefPostDataElementImpl::SetToBytes(size_t size, const void* bytes)
{
  Lock();
  // Clear any data currently in the element
  SetToEmpty();

  // Assign the new data
  void* data = malloc(size);
  DCHECK(data != NULL);
  if(data == NULL)
    return;

  memcpy(data, bytes, size);
  
  type_ = PDE_TYPE_BYTES;
  data_.bytes.bytes = data;
  data_.bytes.size = size;
  Unlock();
}

CefPostDataElement::Type CefPostDataElementImpl::GetType()
{
  Lock();
  CefPostDataElement::Type type = type_;
  Unlock();
  return type;
}

std::wstring CefPostDataElementImpl::GetFile()
{
  Lock();
  DCHECK(type_ == PDE_TYPE_FILE);
  std::wstring filename;
  if(type_ == PDE_TYPE_FILE)
    filename = data_.filename;
  Unlock();
  return filename;
}

size_t CefPostDataElementImpl::GetBytesCount()
{
  Lock();
  DCHECK(type_ == PDE_TYPE_BYTES);
  size_t size = 0;
  if(type_ == PDE_TYPE_BYTES)
    size = data_.bytes.size;
  Unlock();
  return size;
}

size_t CefPostDataElementImpl::GetBytes(size_t size, void* bytes)
{
  Lock();
  DCHECK(type_ == PDE_TYPE_BYTES);
  size_t rv = 0;
  if(type_ == PDE_TYPE_BYTES) {
    rv = (size < data_.bytes.size ? size : data_.bytes.size);
    memcpy(bytes, data_.bytes.bytes, rv);
  }
  Unlock();
  return rv;
}

void CefPostDataElementImpl::Set(const net::UploadData::Element& element)
{
  Lock();

  if (element.type() == net::UploadData::TYPE_BYTES) {
    SetToBytes(element.bytes().size(),
        static_cast<const void*>(
            std::string(element.bytes().begin(),
            element.bytes().end()).c_str()));
  } else if (element.type() == net::UploadData::TYPE_FILE) {
    SetToFile(element.file_path().value());
  } else {
    NOTREACHED();
  }

  Unlock();
}

void CefPostDataElementImpl::Get(net::UploadData::Element& element)
{
  Lock();

  if(type_ == PDE_TYPE_BYTES) {
    element.SetToBytes(static_cast<char*>(data_.bytes.bytes), data_.bytes.size);
  } else if(type_ == PDE_TYPE_FILE) {
    element.SetToFilePath(FilePath(data_.filename));
  } else {
    NOTREACHED();
  }

  Unlock();
}

void CefPostDataElementImpl::Set(const WebKit::WebHTTPBody::Element& element)
{
  Lock();

  if(element.type == WebKit::WebHTTPBody::Element::TypeData) {
    SetToBytes(element.data.size(),
        static_cast<const void*>(element.data.data()));
  } else if(element.type == WebKit::WebHTTPBody::Element::TypeFile) {
    SetToFile(UTF8ToWide(webkit_glue::WebStringToStdString(element.filePath)));
  } else {
    NOTREACHED();
  }

  Unlock();
}

void CefPostDataElementImpl::Get(WebKit::WebHTTPBody::Element& element)
{
  Lock();

  if(type_ == PDE_TYPE_BYTES) {
    element.type = WebKit::WebHTTPBody::Element::TypeData;
    element.data.assign(
        static_cast<char*>(data_.bytes.bytes), data_.bytes.size);
  } else if(type_ == PDE_TYPE_FILE) {
    element.type = WebKit::WebHTTPBody::Element::TypeFile;
    element.filePath.assign(
        webkit_glue::StdStringToWebString(WideToUTF8(data_.filename)));
  } else {
    NOTREACHED();
  }

  Unlock();
}
