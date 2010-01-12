// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _REQUEST_IMPL_H
#define _REQUEST_IMPL_H

#include "../include/cef.h"
#include "net/base/upload_data.h"
#include "third_party/WebKit/WebKit/chromium/public/WebHTTPBody.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"

class URLRequest;

// Implementation of CefRequest
class CefRequestImpl : public CefThreadSafeBase<CefRequest>
{
public:
  CefRequestImpl();
  ~CefRequestImpl() {}

  virtual std::wstring GetURL();
  virtual void SetURL(const std::wstring& url);
  virtual std::wstring GetMethod();
  virtual void SetMethod(const std::wstring& method);
  virtual CefRefPtr<CefPostData> GetPostData();
  virtual void SetPostData(CefRefPtr<CefPostData> postData);
  virtual void GetHeaderMap(HeaderMap& headerMap);
  virtual void SetHeaderMap(const HeaderMap& headerMap);
  virtual void Set(const std::wstring& url,
                   const std::wstring& method,
                   CefRefPtr<CefPostData> postData,
                   const HeaderMap& headerMap);

  void Set(URLRequest* request);

  static void GetHeaderMap(const WebKit::WebURLRequest& request,
                           HeaderMap& map);
  static void SetHeaderMap(const HeaderMap& map,
                           WebKit::WebURLRequest& request);

  static void GetHeaderMap(const std::string& header_str, HeaderMap& map);

protected:
  std::wstring url_;
  std::wstring method_;
  CefRefPtr<CefPostData> postdata_;
  HeaderMap headermap_;
};

// Implementation of CefPostData
class CefPostDataImpl : public CefThreadSafeBase<CefPostData>
{
public:
  CefPostDataImpl();
  ~CefPostDataImpl() {}

  virtual size_t GetElementCount();
  virtual void GetElements(ElementVector& elements);
  virtual bool RemoveElement(CefRefPtr<CefPostDataElement> element);
  virtual bool AddElement(CefRefPtr<CefPostDataElement> element);
  virtual void RemoveElements();

  void Set(const net::UploadData& data);
  void Get(net::UploadData& data);
  void Set(const WebKit::WebHTTPBody& data);
  void Get(WebKit::WebHTTPBody& data);

protected:
  ElementVector elements_;
};

// Implementation of CefPostDataElement
class CefPostDataElementImpl : public CefThreadSafeBase<CefPostDataElement>
{
public:
  CefPostDataElementImpl();
  ~CefPostDataElementImpl();

  virtual void SetToEmpty();
  virtual void SetToFile(const std::wstring& fileName);
  virtual void SetToBytes(size_t size, const void* bytes);
  virtual Type GetType();
  virtual std::wstring GetFile();
  virtual size_t GetBytesCount();
  virtual size_t GetBytes(size_t size, void* bytes);

  void* GetBytes() { return data_.bytes.bytes; }

  void Set(const net::UploadData::Element& element);
  void Get(net::UploadData::Element& element);
  void Set(const WebKit::WebHTTPBody::Element& element);
  void Get(WebKit::WebHTTPBody::Element& element);

protected:
  Type type_;
  union {
    struct {
      void* bytes;
      size_t size;
    } bytes;
    wchar_t* filename;
  } data_;
};

#endif // _REQUEST_IMPL_H
