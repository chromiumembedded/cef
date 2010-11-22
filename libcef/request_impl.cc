// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "request_impl.h"
#include "browser_webkit_glue.h"

#include "base/logging.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHTTPHeaderVisitor.h"

using net::HttpResponseHeaders;


CefRefPtr<CefRequest> CefRequest::CreateRequest()
{
  CefRefPtr<CefRequest> request(new CefRequestImpl());
  return request;
}

CefRequestImpl::CefRequestImpl()
{
}

CefString CefRequestImpl::GetURL()
{
  Lock();
  CefString url = url_;
  Unlock();
  return url;
}

void CefRequestImpl::SetURL(const CefString& url)
{
  Lock();
  url_ = url;
  Unlock();
}

CefString CefRequestImpl::GetMethod()
{
  Lock();
  CefString method = method_;
  Unlock();
  return method;
}

void CefRequestImpl::SetMethod(const CefString& method)
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

void CefRequestImpl::Set(const CefString& url,
                         const CefString& method,
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

void CefRequestImpl::Set(URLRequest* request)
{
  SetURL(request->url().spec());
  SetMethod(request->method());

  // Transfer request headers
  HeaderMap headerMap;
  GetHeaderMap(request->extra_request_headers(), headerMap);
  headerMap.insert(std::make_pair(L"Referrer", request->referrer()));
  SetHeaderMap(headerMap);

  // Transfer post data, if any
  net::UploadData* data = request->get_upload();
  if (data) {
    CefRefPtr<CefPostData> postdata(CefPostData::CreatePostData());
    static_cast<CefPostDataImpl*>(postdata.get())->Set(*data);
    SetPostData(postdata);
  }
}


void CefRequestImpl::GetHeaderMap(const net::HttpRequestHeaders& headers, HeaderMap& map)
{
  net::HttpRequestHeaders::Iterator it(headers);
  do {
    map[it.name()] = it.value();
  } while (it.GetNext());
}

void CefRequestImpl::GetHeaderMap(const WebKit::WebURLRequest& request,
                                  HeaderMap& map)
{
  class CefHTTPHeaderVisitor : public WebKit::WebHTTPHeaderVisitor {
    public:
      CefHTTPHeaderVisitor(HeaderMap* map) : map_(map) {}

      virtual void visitHeader(const WebKit::WebString& name,
                               const WebKit::WebString& value) {
        map_->insert(std::make_pair(string16(name), string16(value)));
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
  for(; it != map.end(); ++it)
    request.setHTTPHeaderField(string16(it->first), string16(it->second));
}

std::string CefRequestImpl::GenerateHeaders(const HeaderMap& map)
{
  std::string headers;

  for(HeaderMap::const_iterator header = map.begin();
      header != map.end();
      ++header) {
    const CefString& key = header->first;
    const CefString& value = header->second;

    if(!key.empty()) {
      // Delimit with "\r\n".
      if(!headers.empty())
        headers += "\r\n";

      headers += std::string(key) + ": " + std::string(value);
    }
  }

  return headers;
}

void CefRequestImpl::ParseHeaders(const std::string& header_str, HeaderMap& map)
{
  // Parse the request header values
  std::string headerStr = "HTTP/1.1 200 OK\n";
  headerStr += header_str;
  scoped_refptr<net::HttpResponseHeaders> headers =
      new HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
          headerStr.c_str(), headerStr.length()));
  void* iter = NULL;
  std::string name, value;
  while(headers->EnumerateHeaderLines(&iter, &name, &value))
    map.insert(std::make_pair(name, value));
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

void CefPostDataImpl::Set(net::UploadData& data)
{
  Lock();

  CefRefPtr<CefPostDataElement> postelem;

  std::vector<net::UploadData::Element>* elements = data.elements();
  std::vector<net::UploadData::Element>::const_iterator it = elements->begin();
  for (; it != elements->end(); ++it) {
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
  data.SetElements(data_elements);

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
  memset(&data_, 0, sizeof(data_));
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
    cef_string_clear(&data_.filename);
  type_ = PDE_TYPE_EMPTY;
  memset(&data_, 0, sizeof(data_));
  Unlock();
}

void CefPostDataElementImpl::SetToFile(const CefString& fileName)
{
  Lock();
  // Clear any data currently in the element
  SetToEmpty();

  // Assign the new data
  type_ = PDE_TYPE_FILE;
  cef_string_copy(fileName.c_str(), fileName.length(), &data_.filename);
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

CefString CefPostDataElementImpl::GetFile()
{
  Lock();
  DCHECK(type_ == PDE_TYPE_FILE);
  CefString filename;
  if(type_ == PDE_TYPE_FILE)
    filename.FromString(data_.filename.str, data_.filename.length, false);
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
#if defined(OS_WIN)
    SetToFile(element.file_path().value());
#else
    SetToFile(UTF8ToWide(element.file_path().value()));
#endif
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
    FilePath path = FilePath(CefString(&data_.filename));
    element.SetToFilePath(path);
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
    SetToFile(string16(element.filePath));
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
    element.filePath.assign(string16(CefString(&data_.filename)));
  } else {
    NOTREACHED();
  }

  Unlock();
}
