// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_REQUEST_IMPL_H_
#define CEF_LIBCEF_COMMON_REQUEST_IMPL_H_
#pragma once

#include <stdint.h>

#include <memory>

#include "include/cef_request.h"

#include "base/synchronization/lock.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "url/gurl.h"

namespace navigation_interception {
class NavigationParams;
}

namespace net {
class HttpRequestHeaders;
class UploadData;
class UploadDataStream;
class UploadElement;
class UploadElementReader;
class URLFetcher;
class URLRequest;
};

namespace blink {
class WebURLRequest;
}

struct CefMsg_LoadRequest_Params;
struct CefNavigateParams;

// Implementation of CefRequest
class CefRequestImpl : public CefRequest {
 public:
  enum Changes {
    kChangedNone =                  0,
    kChangedUrl =                   1 << 0,
    kChangedMethod =                1 << 1,
    kChangedReferrer =              1 << 2,
    kChangedPostData =              1 << 3,
    kChangedHeaderMap =             1 << 4,
    kChangedFlags =                 1 << 5,
    kChangedFirstPartyForCookies =  1 << 6,
  };

  CefRequestImpl();

  bool IsReadOnly() override;
  CefString GetURL() override;
  void SetURL(const CefString& url) override;
  CefString GetMethod() override;
  void SetMethod(const CefString& method) override;
  void SetReferrer(const CefString& referrer_url,
                   ReferrerPolicy policy) override;
  CefString GetReferrerURL() override;
  ReferrerPolicy GetReferrerPolicy() override;
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
  // If |changed_only| is true then only the changed fields will be updated.
  void Get(net::URLRequest* request, bool changed_only) const;

  // Populate this object from the NavigationParams object.
  // TODO(cef): Remove the |is_main_frame| argument once NavigationParams is
  // reliable in reporting that value.
  // Called from content_browser_client.cc NavigationOnUIThread().
  void Set(const navigation_interception::NavigationParams& params,
           bool is_main_frame);

  // Populate this object from a WebURLRequest object.
  // Called from CefContentRendererClient::HandleNavigation().
  void Set(const blink::WebURLRequest& request);

  // Populate the WebURLRequest object from this object.
  // Called from CefRenderURLRequest::Context::Start().
  void Get(blink::WebURLRequest& request, int64& upload_data_size) const;

  // Populate the WebURLRequest object based on the contents of |params|.
  // Called from CefBrowserImpl::LoadRequest().
  static void Get(const CefMsg_LoadRequest_Params& params,
                  blink::WebURLRequest& request);

  // Populate the CefNavigateParams object from this object.
  // Called from CefBrowserHostImpl::LoadRequest().
  void Get(CefNavigateParams& params) const;

  // Populate the URLFetcher object from this object.
  // Called from CefBrowserURLRequest::Context::ContinueOnOriginatingThread().
  void Get(net::URLFetcher& fetcher, int64& upload_data_size) const;

  void SetReadOnly(bool read_only);

  void SetTrackChanges(bool track_changes);
  uint8_t GetChanges() const;

 private:
  void Changed(uint8_t changes);
  bool ShouldSet(uint8_t changes, bool changed_only) const;

  void Reset();

  GURL url_;
  std::string method_;
  GURL referrer_url_;
  ReferrerPolicy referrer_policy_;
  CefRefPtr<CefPostData> postdata_;
  HeaderMap headermap_;
  ResourceType resource_type_;
  TransitionType transition_type_;
  uint64 identifier_;

  // The below members are used by CefURLRequest.
  int flags_;
  GURL first_party_for_cookies_;

  // True if this object is read-only.
  bool read_only_;

  // True if this object should track changes.
  bool track_changes_;

  // Bitmask of |Changes| values which indicate which fields have changed.
  uint8_t changes_;

  mutable base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefRequestImpl);
};

// Implementation of CefPostData
class CefPostDataImpl : public CefPostData {
 public:
  CefPostDataImpl();

  bool IsReadOnly() override;
  bool HasExcludedElements() override;
  size_t GetElementCount() override;
  void GetElements(ElementVector& elements) override;
  bool RemoveElement(CefRefPtr<CefPostDataElement> element) override;
  bool AddElement(CefRefPtr<CefPostDataElement> element) override;
  void RemoveElements() override;

  void Set(const net::UploadData& data);
  void Set(const net::UploadDataStream& data_stream);
  void Get(net::UploadData& data) const;
  std::unique_ptr<net::UploadDataStream> Get() const;
  void Set(const blink::WebHTTPBody& data);
  void Get(blink::WebHTTPBody& data) const;

  void SetReadOnly(bool read_only);

  void SetTrackChanges(bool track_changes);
  bool HasChanges() const;

 private:
  void Changed();

  ElementVector elements_;

  // True if this object is read-only.
  bool read_only_;

  // True if this object has excluded elements.
  bool has_excluded_elements_;

  // True if this object should track changes.
  bool track_changes_;

  // True if this object has changes.
  bool has_changes_;

  mutable base::Lock lock_;

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
  void Get(net::UploadElement& element) const;
  std::unique_ptr<net::UploadElementReader> Get() const;
  void Set(const blink::WebHTTPBody::Element& element);
  void Get(blink::WebHTTPBody::Element& element) const;

  void SetReadOnly(bool read_only);

  void SetTrackChanges(bool track_changes);
  bool HasChanges() const;

 private:
  void Changed();
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

  // True if this object should track changes.
  bool track_changes_;

  // True if this object has changes.
  bool has_changes_;

  mutable base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefPostDataElementImpl);
};

#endif  // CEF_LIBCEF_COMMON_REQUEST_IMPL_H_
