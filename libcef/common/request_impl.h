// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_REQUEST_IMPL_H_
#define CEF_LIBCEF_COMMON_REQUEST_IMPL_H_
#pragma once

#include "include/cef_request.h"

#include "base/synchronization/lock.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"

namespace net {
class HttpRequestHeaders;
class UploadData;
class UploadDataStream;
class UploadElement;
class UploadElementReader;
class URLRequest;
};

namespace blink {
class WebURLRequest;
}

// Implementation of CefRequest
class CefRequestImpl : public CefRequest {
 public:
  CefRequestImpl();

  bool IsReadOnly() override;
  CefString GetURL() override;
  void SetURL(const CefString& url) override;
  CefString GetMethod() override;
  void SetMethod(const CefString& method) override;
  CefRefPtr<CefPostData> GetPostData() override;
  void SetPostData(CefRefPtr<CefPostData> postData) override;
  void GetHeaderMap(HeaderMap& headerMap) override;
  void SetHeaderMap(const HeaderMap& headerMap) override;
  void Set(const CefString& url,
           const CefString& method,
           CefRefPtr<CefPostData> postData,
           const HeaderMap& headerMap) override;
  int GetFlags() override;
  void SetFlags(int flags) override;
  CefString GetFirstPartyForCookies() override;
  void SetFirstPartyForCookies(const CefString& url) override;
  ResourceType GetResourceType() override;
  TransitionType GetTransitionType() override;
  uint64 GetIdentifier() override;

  // Populate this object from the URLRequest object.
  void Set(net::URLRequest* request);

  // Populate the URLRequest object from this object.
  void Get(net::URLRequest* request);

  // Populate this object from a WebURLRequest object.
  void Set(const blink::WebURLRequest& request);

  // Populate the WebURLRequest object from this object.
  void Get(blink::WebURLRequest& request);

  void SetReadOnly(bool read_only);

  static void GetHeaderMap(const net::HttpRequestHeaders& headers,
                           HeaderMap& map);
  static void GetHeaderMap(const blink::WebURLRequest& request,
                           HeaderMap& map);
  static void SetHeaderMap(const HeaderMap& map,
                           blink::WebURLRequest& request);

 protected:
  CefString url_;
  CefString method_;
  CefRefPtr<CefPostData> postdata_;
  HeaderMap headermap_;
  ResourceType resource_type_;
  TransitionType transition_type_;
  uint64 identifier_;

  // The below members are used by CefURLRequest.
  int flags_;
  CefString first_party_for_cookies_;

  // True if this object is read-only.
  bool read_only_;

  base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefRequestImpl);
};

// Implementation of CefPostData
class CefPostDataImpl : public CefPostData {
 public:
  CefPostDataImpl();

  bool IsReadOnly() override;
  size_t GetElementCount() override;
  void GetElements(ElementVector& elements) override;
  bool RemoveElement(CefRefPtr<CefPostDataElement> element) override;
  bool AddElement(CefRefPtr<CefPostDataElement> element) override;
  void RemoveElements() override;

  void Set(const net::UploadData& data);
  void Set(const net::UploadDataStream& data_stream);
  void Get(net::UploadData& data);
  net::UploadDataStream* Get();
  void Set(const blink::WebHTTPBody& data);
  void Get(blink::WebHTTPBody& data);

  void SetReadOnly(bool read_only);

 protected:
  ElementVector elements_;

  // True if this object is read-only.
  bool read_only_;

  base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefPostDataImpl);
};

// Implementation of CefPostDataElement
class CefPostDataElementImpl : public CefPostDataElement {
 public:
  CefPostDataElementImpl();
  ~CefPostDataElementImpl() override;

  bool IsReadOnly() override;
  void SetToEmpty() override;
  void SetToFile(const CefString& fileName) override;
  void SetToBytes(size_t size, const void* bytes) override;
  Type GetType() override;
  CefString GetFile() override;
  size_t GetBytesCount() override;
  size_t GetBytes(size_t size, void* bytes) override;

  void* GetBytes() { return data_.bytes.bytes; }

  void Set(const net::UploadElement& element);
  void Set(const net::UploadElementReader& element_reader);
  void Get(net::UploadElement& element);
  net::UploadElementReader* Get();
  void Set(const blink::WebHTTPBody::Element& element);
  void Get(blink::WebHTTPBody::Element& element);

  void SetReadOnly(bool read_only);

 protected:
  void Cleanup();

  Type type_;
  union {
    struct {
      void* bytes;
      size_t size;
    } bytes;
    cef_string_t filename;
  } data_;

  // True if this object is read-only.
  bool read_only_;

  base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefPostDataElementImpl);
};

#endif  // CEF_LIBCEF_COMMON_REQUEST_IMPL_H_
