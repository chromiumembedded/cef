// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "libcef/browser/navigate_params.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/net/http_header_utils.h"
#include "libcef/common/net/upload_data.h"
#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/request_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_type.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_security_policy.h"

namespace {

const char kReferrerLowerCase[] = "referer";
const char kContentTypeLowerCase[] = "content-type";
const char kCacheControlLowerCase[] = "cache-control";
const char kCacheControlDirectiveNoCacheLowerCase[] = "no-cache";
const char kCacheControlDirectiveNoStoreLowerCase[] = "no-store";
const char kCacheControlDirectiveOnlyIfCachedLowerCase[] = "only-if-cached";

// Mask of values that configure the cache policy.
const int kURCachePolicyMask =
    (UR_FLAG_SKIP_CACHE | UR_FLAG_ONLY_FROM_CACHE | UR_FLAG_DISABLE_CACHE);

// A subclass of net::UploadBytesElementReader that keeps the associated
// UploadElement alive until the request completes.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  explicit BytesElementReader(std::unique_ptr<net::UploadElement> element)
      : net::UploadBytesElementReader(element->bytes(),
                                      element->bytes_length()),
        element_(std::move(element)) {
    DCHECK_EQ(net::UploadElement::TYPE_BYTES, element_->type());
  }

 private:
  std::unique_ptr<net::UploadElement> element_;

  DISALLOW_COPY_AND_ASSIGN(BytesElementReader);
};

scoped_refptr<base::SequencedTaskRunner> GetFileTaskRunner() {
  return base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

// A subclass of net::UploadFileElementReader that keeps the associated
// UploadElement alive until the request completes.
class FileElementReader : public net::UploadFileElementReader {
 public:
  explicit FileElementReader(std::unique_ptr<net::UploadElement> element)
      : net::UploadFileElementReader(
            GetFileTaskRunner().get(),
            element->file_path(),
            element->file_range_offset(),
            element->file_range_length(),
            element->expected_file_modification_time()),
        element_(std::move(element)) {
    DCHECK_EQ(net::UploadElement::TYPE_FILE, element_->type());
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<net::UploadElement> element_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

std::string GetURLRequestReferrer(const GURL& referrer_url) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!referrer_url.is_valid() ||
      command_line->HasSwitch(network::switches::kNoReferrers)) {
    return std::string();
  }

  return referrer_url.spec();
}

void MakeASCIILower(std::string* str) {
  std::transform(str->begin(), str->end(), str->begin(), ::tolower);
}

// Returns the cef_urlrequest_flags_t policy specified by the Cache-Control
// request header directives, if any. The directives are case-insensitive and
// some have an optional argument. Multiple directives are comma-separated.
// See https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cache-Control
// for details.
int GetCacheControlHeaderPolicy(CefRequest::HeaderMap headerMap) {
  std::string line;

  // Extract the Cache-Control header line.
  {
    CefRequest::HeaderMap::const_iterator it = headerMap.begin();
    for (; it != headerMap.end(); ++it) {
      if (base::LowerCaseEqualsASCII(it->first.ToString(),
                                     kCacheControlLowerCase)) {
        line = it->second;
        break;
      }
    }
  }

  int flags = 0;

  if (!line.empty()) {
    MakeASCIILower(&line);

    std::vector<base::StringPiece> pieces = base::SplitStringPiece(
        line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& piece : pieces) {
      if (base::LowerCaseEqualsASCII(piece,
                                     kCacheControlDirectiveNoCacheLowerCase)) {
        flags |= UR_FLAG_SKIP_CACHE;
      } else if (base::LowerCaseEqualsASCII(
                     piece, kCacheControlDirectiveOnlyIfCachedLowerCase)) {
        flags |= UR_FLAG_ONLY_FROM_CACHE;
      } else if (base::LowerCaseEqualsASCII(
                     piece, kCacheControlDirectiveNoStoreLowerCase)) {
        flags |= UR_FLAG_DISABLE_CACHE;
      }
    }
  }

  return flags;
}

// Convert cef_urlrequest_flags_t to blink::WebCachePolicy.
blink::mojom::FetchCacheMode GetFetchCacheMode(int ur_flags) {
  const bool skip_cache{ur_flags & UR_FLAG_SKIP_CACHE};
  const bool only_from_cache{ur_flags & UR_FLAG_ONLY_FROM_CACHE};
  const bool disable_cache{ur_flags & UR_FLAG_DISABLE_CACHE};
  if (only_from_cache && (skip_cache || disable_cache)) {
    // The request will always fail because only_from_cache and
    // skip_cache/disable_cache are mutually exclusive.
    return blink::mojom::FetchCacheMode::kUnspecifiedForceCacheMiss;
  } else if (disable_cache) {
    // This additionally implies the skip_cache behavior.
    return blink::mojom::FetchCacheMode::kNoStore;
  } else if (skip_cache) {
    return blink::mojom::FetchCacheMode::kBypassCache;
  } else if (only_from_cache) {
    return blink::mojom::FetchCacheMode::kOnlyIfCached;
  }
  return blink::mojom::FetchCacheMode::kDefault;
}

blink::WebString FilePathStringToWebString(
    const base::FilePath::StringType& str) {
#if defined(OS_POSIX)
  return blink::WebString::FromUTF8(str);
#elif defined(OS_WIN)
  return blink::WebString::FromUTF16(str);
#endif
}

// Read |headers| into |map|.
void GetHeaderMap(const net::HttpRequestHeaders& headers,
                  CefRequest::HeaderMap& map) {
  map.clear();

  if (headers.IsEmpty())
    return;

  net::HttpRequestHeaders::Iterator it(headers);
  while (it.GetNext()) {
    const std::string& name = it.name();

    // Do not include Referer in the header map.
    if (!base::LowerCaseEqualsASCII(name, kReferrerLowerCase))
      map.insert(std::make_pair(name, it.value()));
  };
}

// Read |source| into |map|.
void GetHeaderMap(const CefRequest::HeaderMap& source,
                  CefRequest::HeaderMap& map) {
  map.clear();

  CefRequest::HeaderMap::const_iterator it = source.begin();
  for (; it != source.end(); ++it) {
    const CefString& name = it->first;

    // Do not include Referer in the header map.
    if (!base::LowerCaseEqualsASCII(name.ToString(), kReferrerLowerCase))
      map.insert(std::make_pair(name, it->second));
  }
}

// Read |map| into |request|.
void SetHeaderMap(const CefRequest::HeaderMap& map,
                  blink::WebURLRequest& request) {
  CefRequest::HeaderMap::const_iterator it = map.begin();
  for (; it != map.end(); ++it) {
    request.SetHttpHeaderField(
        blink::WebString::FromUTF16(it->first.ToString16()),
        blink::WebString::FromUTF16(it->second.ToString16()));
  }
}

// Finds the first instance of |nameLower| (already lower-case) in |map| with
// case-insensitive comparison.
CefRequest::HeaderMap::iterator FindHeaderInMap(const std::string& nameLower,
                                                CefRequest::HeaderMap& map) {
  for (auto it = map.begin(); it != map.end(); ++it) {
    if (base::EqualsCaseInsensitiveASCII(it->first.ToString(), nameLower))
      return it;
  }

  return map.end();
}

// Type used in UploadDataStream.
typedef std::vector<std::unique_ptr<net::UploadElementReader>>
    UploadElementReaders;

}  // namespace

#define CHECK_READONLY_RETURN(val)         \
  if (read_only_) {                        \
    NOTREACHED() << "object is read only"; \
    return val;                            \
  }

#define CHECK_READONLY_RETURN_VOID()       \
  if (read_only_) {                        \
    NOTREACHED() << "object is read only"; \
    return;                                \
  }

#define SETBOOLFLAG(obj, flags, method, FLAG) \
  obj.method((flags & (FLAG)) == (FLAG))

// CefRequest -----------------------------------------------------------------

// static
CefRefPtr<CefRequest> CefRequest::Create() {
  CefRefPtr<CefRequest> request(new CefRequestImpl());
  return request;
}

// CefRequestImpl -------------------------------------------------------------

CefRequestImpl::CefRequestImpl() {
  // Verify that our enum matches Chromium's values.
  static_assert(static_cast<int>(REFERRER_POLICY_LAST_VALUE) ==
                    static_cast<int>(net::URLRequest::MAX_REFERRER_POLICY),
                "enum mismatch");

  base::AutoLock lock_scope(lock_);
  Reset();
}

bool CefRequestImpl::IsReadOnly() {
  base::AutoLock lock_scope(lock_);
  return read_only_;
}

CefString CefRequestImpl::GetURL() {
  base::AutoLock lock_scope(lock_);
  return url_.spec();
}

void CefRequestImpl::SetURL(const CefString& url) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  const GURL& new_url = GURL(url.ToString());
  if (url_ != new_url) {
    Changed(kChangedUrl);
    url_ = new_url;
  }
}

CefString CefRequestImpl::GetMethod() {
  base::AutoLock lock_scope(lock_);
  return method_;
}

void CefRequestImpl::SetMethod(const CefString& method) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  const std::string& new_method = method;
  if (method_ != new_method) {
    Changed(kChangedMethod);
    method_ = new_method;
  }
}

void CefRequestImpl::SetReferrer(const CefString& referrer_url,
                                 ReferrerPolicy policy) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  // Call GetAsReferrer here for consistency since the same logic will later be
  // applied by URLRequest::SetReferrer().
  const GURL& new_referrer_url = GURL(referrer_url.ToString()).GetAsReferrer();
  if (referrer_url_ != new_referrer_url || referrer_policy_ != policy) {
    Changed(kChangedReferrer);
    referrer_url_ = new_referrer_url;
    referrer_policy_ = policy;
  }
}

CefString CefRequestImpl::GetReferrerURL() {
  base::AutoLock lock_scope(lock_);
  return referrer_url_.spec();
}

CefRequestImpl::ReferrerPolicy CefRequestImpl::GetReferrerPolicy() {
  base::AutoLock lock_scope(lock_);
  return referrer_policy_;
}

CefRefPtr<CefPostData> CefRequestImpl::GetPostData() {
  base::AutoLock lock_scope(lock_);
  return postdata_;
}

void CefRequestImpl::SetPostData(CefRefPtr<CefPostData> postData) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  Changed(kChangedPostData);
  postdata_ = postData;
}

void CefRequestImpl::GetHeaderMap(HeaderMap& headerMap) {
  base::AutoLock lock_scope(lock_);
  headerMap = headermap_;
}

void CefRequestImpl::SetHeaderMap(const HeaderMap& headerMap) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  Changed(kChangedHeaderMap);
  ::GetHeaderMap(headerMap, headermap_);
}

CefString CefRequestImpl::GetHeaderByName(const CefString& name) {
  base::AutoLock lock_scope(lock_);

  std::string nameLower = name;
  MakeASCIILower(&nameLower);

  auto it = FindHeaderInMap(nameLower, headermap_);
  if (it != headermap_.end())
    return it->second;

  return CefString();
}

void CefRequestImpl::SetHeaderByName(const CefString& name,
                                     const CefString& value,
                                     bool overwrite) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  std::string nameLower = name;
  MakeASCIILower(&nameLower);

  // Do not include Referer in the header map.
  if (nameLower == kReferrerLowerCase)
    return;

  Changed(kChangedHeaderMap);

  // There may be multiple values, so remove any first.
  for (auto it = headermap_.begin(); it != headermap_.end();) {
    if (base::EqualsCaseInsensitiveASCII(it->first.ToString(), nameLower)) {
      if (!overwrite)
        return;
      it = headermap_.erase(it);
    } else {
      ++it;
    }
  }

  headermap_.insert(std::make_pair(name, value));
}

void CefRequestImpl::Set(const CefString& url,
                         const CefString& method,
                         CefRefPtr<CefPostData> postData,
                         const HeaderMap& headerMap) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  const GURL& new_url = GURL(url.ToString());
  if (url_ != new_url) {
    Changed(kChangedUrl);
    url_ = new_url;
  }
  const std::string& new_method = method;
  if (method_ != new_method) {
    Changed(kChangedMethod);
    method_ = new_method;
  }
  if (postdata_ != postData) {
    Changed(kChangedPostData);
    postdata_ = postData;
  }
  Changed(kChangedHeaderMap);
  ::GetHeaderMap(headerMap, headermap_);
}

int CefRequestImpl::GetFlags() {
  base::AutoLock lock_scope(lock_);
  return flags_;
}

void CefRequestImpl::SetFlags(int flags) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  if (flags_ != flags) {
    Changed(kChangedFlags);
    flags_ = flags;
  }
}

CefString CefRequestImpl::GetFirstPartyForCookies() {
  base::AutoLock lock_scope(lock_);
  return first_party_for_cookies_.spec();
}

void CefRequestImpl::SetFirstPartyForCookies(const CefString& url) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  const GURL& new_url = GURL(url.ToString());
  if (first_party_for_cookies_ != new_url) {
    Changed(kChangedFirstPartyForCookies);
    first_party_for_cookies_ = new_url;
  }
}

CefRequestImpl::ResourceType CefRequestImpl::GetResourceType() {
  base::AutoLock lock_scope(lock_);
  return resource_type_;
}

CefRequestImpl::TransitionType CefRequestImpl::GetTransitionType() {
  base::AutoLock lock_scope(lock_);
  return transition_type_;
}

uint64 CefRequestImpl::GetIdentifier() {
  base::AutoLock lock_scope(lock_);
  return identifier_;
}

void CefRequestImpl::Set(const network::ResourceRequest* request,
                         uint64 identifier) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  Reset();

  url_ = request->url;
  method_ = request->method;
  identifier_ = identifier;

  // Our consumer should have made sure that this is a safe referrer. See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (request->referrer.is_valid()) {
    referrer_url_ = request->referrer;
    referrer_policy_ =
        static_cast<cef_referrer_policy_t>(request->referrer_policy);
  }

  // Transfer request headers.
  ::GetHeaderMap(request->headers, headermap_);

  // Transfer post data, if any.
  if (request->request_body) {
    postdata_ = CefPostData::Create();
    static_cast<CefPostDataImpl*>(postdata_.get())->Set(*request->request_body);
  }

  first_party_for_cookies_ = request->site_for_cookies;

  resource_type_ = static_cast<cef_resource_type_t>(request->resource_type);
  transition_type_ =
      static_cast<cef_transition_type_t>(request->transition_type);
}

void CefRequestImpl::Get(network::ResourceRequest* request,
                         bool changed_only) const {
  base::AutoLock lock_scope(lock_);

  if (ShouldSet(kChangedUrl, changed_only))
    request->url = url_;

  if (ShouldSet(kChangedMethod, changed_only))
    request->method = method_;

  if (ShouldSet(kChangedReferrer, changed_only)) {
    request->referrer = referrer_url_;
    request->referrer_policy =
        static_cast<net::URLRequest::ReferrerPolicy>(referrer_policy_);
  }

  if (ShouldSet(kChangedHeaderMap, changed_only)) {
    net::HttpRequestHeaders headers;
    headers.AddHeadersFromString(HttpHeaderUtils::GenerateHeaders(headermap_));
    request->headers.Swap(&headers);
  }

  if (ShouldSet(kChangedPostData, changed_only)) {
    if (postdata_.get()) {
      request->request_body =
          static_cast<CefPostDataImpl*>(postdata_.get())->GetBody();
    } else if (request->request_body) {
      request->request_body = nullptr;
    }
  }

  if (!first_party_for_cookies_.is_empty() &&
      ShouldSet(kChangedFirstPartyForCookies, changed_only)) {
    request->site_for_cookies = first_party_for_cookies_;
  }

  if (ShouldSet(kChangedFlags, changed_only)) {
    int flags = flags_;
    if (!(flags & kURCachePolicyMask)) {
      // Only consider the Cache-Control directives when a cache policy is not
      // explicitly set on the request.
      flags |= GetCacheControlHeaderPolicy(headermap_);
    }

    int net_flags = 0;

    if (flags & UR_FLAG_SKIP_CACHE) {
      net_flags |= net::LOAD_BYPASS_CACHE;
    }
    if (flags & UR_FLAG_ONLY_FROM_CACHE) {
      net_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
    }
    if (flags & UR_FLAG_DISABLE_CACHE) {
      net_flags |= net::LOAD_DISABLE_CACHE;
    }

    if (!(flags & UR_FLAG_ALLOW_STORED_CREDENTIALS)) {
      net_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA |
                   net::LOAD_DO_NOT_SEND_COOKIES |
                   net::LOAD_DO_NOT_SAVE_COOKIES;
    }

    request->load_flags = net_flags;
  }
}

void CefRequestImpl::Set(const net::RedirectInfo& redirect_info) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  url_ = redirect_info.new_url;
  method_ = redirect_info.new_method;
  first_party_for_cookies_ = redirect_info.new_site_for_cookies;
  referrer_url_ = GURL(redirect_info.new_referrer);
  referrer_policy_ =
      static_cast<cef_referrer_policy_t>(redirect_info.new_referrer_policy);
}

void CefRequestImpl::Set(const net::HttpRequestHeaders& headers) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  ::GetHeaderMap(headers, headermap_);
}

void CefRequestImpl::Set(const net::URLRequest* request) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  Reset();

  url_ = request->url();
  method_ = request->method();
  identifier_ = request->identifier();

  // URLRequest::SetReferrer ensures that we do not send username and password
  // fields in the referrer.
  GURL referrer(request->referrer());

  // Our consumer should have made sure that this is a safe referrer. See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (referrer.is_valid()) {
    referrer_url_ = referrer;
    referrer_policy_ =
        static_cast<cef_referrer_policy_t>(request->referrer_policy());
  }

  // Transfer request headers.
  ::GetHeaderMap(request->extra_request_headers(), headermap_);

  // Transfer post data, if any.
  const net::UploadDataStream* data = request->get_upload();
  if (data) {
    postdata_ = CefPostData::Create();
    static_cast<CefPostDataImpl*>(postdata_.get())->Set(*data);
  }

  first_party_for_cookies_ = request->site_for_cookies();

  content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (info) {
    resource_type_ = static_cast<cef_resource_type_t>(info->GetResourceType());
    transition_type_ =
        static_cast<cef_transition_type_t>(info->GetPageTransition());
  }
}

void CefRequestImpl::Get(net::URLRequest* request, bool changed_only) const {
  base::AutoLock lock_scope(lock_);

  if (ShouldSet(kChangedMethod, changed_only))
    request->set_method(method_);

  if (ShouldSet(kChangedReferrer, changed_only)) {
    request->SetReferrer(GetURLRequestReferrer(referrer_url_));
    request->set_referrer_policy(
        static_cast<net::URLRequest::ReferrerPolicy>(referrer_policy_));
  }

  if (ShouldSet(kChangedHeaderMap, changed_only)) {
    net::HttpRequestHeaders headers;
    headers.AddHeadersFromString(HttpHeaderUtils::GenerateHeaders(headermap_));
    request->SetExtraRequestHeaders(headers);
  }

  if (ShouldSet(kChangedPostData, changed_only)) {
    if (postdata_.get()) {
      request->set_upload(
          static_cast<CefPostDataImpl*>(postdata_.get())->Get());
    } else if (request->get_upload()) {
      request->set_upload(std::unique_ptr<net::UploadDataStream>());
    }
  }

  if (!first_party_for_cookies_.is_empty() &&
      ShouldSet(kChangedFirstPartyForCookies, changed_only)) {
    request->set_site_for_cookies(first_party_for_cookies_);
  }
}

void CefRequestImpl::Set(
    const navigation_interception::NavigationParams& params,
    bool is_main_frame) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  Reset();

  url_ = params.url();
  method_ = params.is_post() ? "POST" : "GET";

  const content::Referrer& sanitized_referrer =
      content::Referrer::SanitizeForRequest(params.url(), params.referrer());
  referrer_url_ = sanitized_referrer.url;
  referrer_policy_ =
      BlinkReferrerPolicyToNetReferrerPolicy(sanitized_referrer.policy);

  resource_type_ = is_main_frame ? RT_MAIN_FRAME : RT_SUB_FRAME;
  transition_type_ =
      static_cast<cef_transition_type_t>(params.transition_type());
}

void CefRequestImpl::Get(blink::WebURLRequest& request,
                         int64& upload_data_size) const {
  base::AutoLock lock_scope(lock_);

  request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  request.SetURL(url_);
  request.SetHTTPMethod(blink::WebString::FromUTF8(method_));

  if (!referrer_url_.is_empty()) {
    const blink::WebString& referrer =
        blink::WebSecurityPolicy::GenerateReferrerHeader(
            NetReferrerPolicyToBlinkReferrerPolicy(referrer_policy_), url_,
            blink::WebString::FromUTF8(referrer_url_.spec()));
    if (!referrer.IsEmpty()) {
      request.SetHTTPReferrer(
          referrer, NetReferrerPolicyToBlinkReferrerPolicy(referrer_policy_));
    }
  }

  if (postdata_.get()) {
    blink::WebHTTPBody body;
    body.Initialize();
    static_cast<CefPostDataImpl*>(postdata_.get())->Get(body);
    request.SetHTTPBody(body);

    if (flags_ & UR_FLAG_REPORT_UPLOAD_PROGRESS) {
      // Attempt to determine the upload data size.
      CefPostData::ElementVector elements;
      postdata_->GetElements(elements);
      if (elements.size() == 1 && elements[0]->GetType() == PDE_TYPE_BYTES) {
        CefPostDataElementImpl* impl =
            static_cast<CefPostDataElementImpl*>(elements[0].get());
        upload_data_size = impl->GetBytesCount();
      }
    }
  }

  ::SetHeaderMap(headermap_, request);

  if (!first_party_for_cookies_.is_empty())
    request.SetSiteForCookies(first_party_for_cookies_);

  int flags = flags_;
  if (!(flags & kURCachePolicyMask)) {
    // Only consider the Cache-Control directives when a cache policy is not
    // explicitly set on the request.
    flags |= GetCacheControlHeaderPolicy(headermap_);
  }
  request.SetCacheMode(GetFetchCacheMode(flags));

  SETBOOLFLAG(request, flags_, SetAllowStoredCredentials,
              UR_FLAG_ALLOW_STORED_CREDENTIALS);
  SETBOOLFLAG(request, flags_, SetReportUploadProgress,
              UR_FLAG_REPORT_UPLOAD_PROGRESS);
}

// static
void CefRequestImpl::Get(const CefMsg_LoadRequest_Params& params,
                         blink::WebURLRequest& request) {
  request.SetURL(params.url);
  request.SetRequestorOrigin(blink::WebSecurityOrigin::Create(params.url));
  if (!params.method.empty())
    request.SetHTTPMethod(blink::WebString::FromASCII(params.method));

  if (params.referrer.is_valid()) {
    const blink::WebString& referrer =
        blink::WebSecurityPolicy::GenerateReferrerHeader(
            NetReferrerPolicyToBlinkReferrerPolicy(
                static_cast<cef_referrer_policy_t>(params.referrer_policy)),
            params.url, blink::WebString::FromUTF8(params.referrer.spec()));
    if (!referrer.IsEmpty()) {
      request.SetHTTPReferrer(
          referrer,
          NetReferrerPolicyToBlinkReferrerPolicy(
              static_cast<cef_referrer_policy_t>(params.referrer_policy)));
    }
  }

  CefRequest::HeaderMap headerMap;
  if (!params.headers.empty()) {
    for (net::HttpUtil::HeadersIterator i(params.headers.begin(),
                                          params.headers.end(), "\n\r");
         i.GetNext();) {
      request.AddHttpHeaderField(blink::WebString::FromUTF8(i.name()),
                                 blink::WebString::FromUTF8(i.values()));
      headerMap.insert(std::make_pair(i.name(), i.values()));
    }
  }

  if (params.upload_data.get()) {
    const base::string16& method = request.HttpMethod().Utf16();
    if (method == base::ASCIIToUTF16("GET") ||
        method == base::ASCIIToUTF16("HEAD")) {
      request.SetHTTPMethod(blink::WebString::FromASCII("POST"));
    }

    // The comparison performed by httpHeaderField() is case insensitive.
    if (request
            .HttpHeaderField(blink::WebString::FromASCII(
                net::HttpRequestHeaders::kContentType))
            .length() == 0) {
      request.SetHttpHeaderField(
          blink::WebString::FromASCII(net::HttpRequestHeaders::kContentType),
          blink::WebString::FromASCII(
              net_service::kContentTypeApplicationFormURLEncoded));
    }

    blink::WebHTTPBody body;
    body.Initialize();

    for (const auto& element : params.upload_data->elements()) {
      if (element->type() == net::UploadElement::TYPE_BYTES) {
        blink::WebData data;
        data.Assign(element->bytes(), element->bytes_length());
        body.AppendData(data);
      } else if (element->type() == net::UploadElement::TYPE_FILE) {
        body.AppendFile(
            FilePathStringToWebString(element->file_path().value()));
      } else {
        NOTREACHED();
      }
    }

    request.SetHTTPBody(body);
  }

  if (params.site_for_cookies.is_valid())
    request.SetSiteForCookies(params.site_for_cookies);

  int flags = params.load_flags;
  if (!(flags & kURCachePolicyMask)) {
    // Only consider the Cache-Control directives when a cache policy is not
    // explicitly set on the request.
    flags |= GetCacheControlHeaderPolicy(headerMap);
  }
  request.SetCacheMode(GetFetchCacheMode(flags));

  SETBOOLFLAG(request, params.load_flags, SetAllowStoredCredentials,
              UR_FLAG_ALLOW_STORED_CREDENTIALS);
  SETBOOLFLAG(request, params.load_flags, SetReportUploadProgress,
              UR_FLAG_REPORT_UPLOAD_PROGRESS);
}

void CefRequestImpl::Get(CefNavigateParams& params) const {
  base::AutoLock lock_scope(lock_);

  params.url = url_;
  params.method = method_;

  // Referrer policy will be applied later in the request pipeline.
  params.referrer.url = referrer_url_;
  params.referrer.policy =
      NetReferrerPolicyToBlinkReferrerPolicy(referrer_policy_);

  if (!headermap_.empty())
    params.headers = HttpHeaderUtils::GenerateHeaders(headermap_);

  if (postdata_) {
    CefPostDataImpl* impl = static_cast<CefPostDataImpl*>(postdata_.get());
    params.upload_data = new net::UploadData();
    impl->Get(*params.upload_data.get());
  }

  params.site_for_cookies = first_party_for_cookies_;
  params.load_flags = flags_;
}

void CefRequestImpl::Get(net::URLFetcher& fetcher,
                         int64& upload_data_size) const {
  base::AutoLock lock_scope(lock_);

  if (!referrer_url_.is_empty()) {
    fetcher.SetReferrer(GetURLRequestReferrer(referrer_url_));
    fetcher.SetReferrerPolicy(
        static_cast<net::URLRequest::ReferrerPolicy>(referrer_policy_));
  }

  CefRequest::HeaderMap headerMap = headermap_;

  std::string content_type;

  // Extract the Content-Type header value.
  {
    HeaderMap::iterator it = headerMap.begin();
    for (; it != headerMap.end(); ++it) {
      if (base::LowerCaseEqualsASCII(it->first.ToString(),
                                     kContentTypeLowerCase)) {
        content_type = it->second;
        headerMap.erase(it);
        break;
      }
    }
  }

  fetcher.SetExtraRequestHeaders(HttpHeaderUtils::GenerateHeaders(headerMap));

  if (postdata_.get()) {
    CefPostData::ElementVector elements;
    postdata_->GetElements(elements);
    if (elements.size() == 1) {
      // Default to URL encoding if not specified.
      if (content_type.empty())
        content_type = net_service::kContentTypeApplicationFormURLEncoded;

      CefPostDataElementImpl* impl =
          static_cast<CefPostDataElementImpl*>(elements[0].get());

      switch (elements[0]->GetType()) {
        case PDE_TYPE_BYTES: {
          const size_t size = impl->GetBytesCount();
          if (flags_ & UR_FLAG_REPORT_UPLOAD_PROGRESS) {
            // Return the upload data size.
            upload_data_size = size;
          }
          fetcher.SetUploadData(
              content_type,
              std::string(static_cast<char*>(impl->GetBytes()), size));
          break;
        }
        case PDE_TYPE_FILE:
          fetcher.SetUploadFilePath(
              content_type, base::FilePath(impl->GetFile()), 0,
              std::numeric_limits<uint64_t>::max(), GetFileTaskRunner());
          break;
        case PDE_TYPE_EMPTY:
          break;
      }
    } else if (elements.size() > 1) {
      NOTIMPLEMENTED() << " multi-part form data is not supported";
    }
  }

  if (!first_party_for_cookies_.is_empty())
    fetcher.SetInitiator(url::Origin::Create(first_party_for_cookies_));

  int flags = flags_;
  if (!(flags & kURCachePolicyMask)) {
    // Only consider the Cache-Control directives when a cache policy is not
    // explicitly set on the request.
    flags |= GetCacheControlHeaderPolicy(headerMap);
  }

  if (flags & UR_FLAG_NO_RETRY_ON_5XX)
    fetcher.SetAutomaticallyRetryOn5xx(false);
  if (flags & UR_FLAG_STOP_ON_REDIRECT)
    fetcher.SetStopOnRedirect(true);

  int net_flags = 0;

  if (flags & UR_FLAG_SKIP_CACHE) {
    net_flags |= net::LOAD_BYPASS_CACHE;
  }
  if (flags & UR_FLAG_ONLY_FROM_CACHE) {
    net_flags |= net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
  }
  if (flags & UR_FLAG_DISABLE_CACHE) {
    net_flags |= net::LOAD_DISABLE_CACHE;
  }

  if (!(flags & UR_FLAG_ALLOW_STORED_CREDENTIALS)) {
    net_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA |
                 net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  }

  fetcher.SetLoadFlags(net_flags);
}

void CefRequestImpl::SetReadOnly(bool read_only) {
  base::AutoLock lock_scope(lock_);
  if (read_only_ == read_only)
    return;

  read_only_ = read_only;

  if (postdata_.get())
    static_cast<CefPostDataImpl*>(postdata_.get())->SetReadOnly(read_only);
}

void CefRequestImpl::SetTrackChanges(bool track_changes,
                                     bool backup_on_change) {
  base::AutoLock lock_scope(lock_);
  if (track_changes_ == track_changes)
    return;

  if (!track_changes && backup_on_change_)
    backup_.reset();

  track_changes_ = track_changes;
  backup_on_change_ = track_changes ? backup_on_change : false;
  changes_ = kChangedNone;

  if (postdata_.get()) {
    static_cast<CefPostDataImpl*>(postdata_.get())
        ->SetTrackChanges(track_changes);
  }
}

void CefRequestImpl::RevertChanges() {
  base::AutoLock lock_scope(lock_);
  DCHECK(!read_only_);
  DCHECK(track_changes_);
  DCHECK(backup_on_change_);
  if (!backup_)
    return;

  // Restore the original values if a backup exists.
  if (backup_->backups_ & kChangedUrl)
    url_ = backup_->url_;
  if (backup_->backups_ & kChangedMethod)
    method_ = backup_->method_;
  if (backup_->backups_ & kChangedReferrer) {
    referrer_url_ = backup_->referrer_url_;
    referrer_policy_ = backup_->referrer_policy_;
  }
  if (backup_->backups_ & kChangedPostData)
    postdata_ = backup_->postdata_;
  if (backup_->backups_ & kChangedHeaderMap) {
    DCHECK(backup_->headermap_);
    headermap_.swap(*backup_->headermap_);
  }
  if (backup_->backups_ & kChangedFlags)
    flags_ = backup_->flags_;
  if (backup_->backups_ & kChangedFirstPartyForCookies)
    first_party_for_cookies_ = backup_->first_party_for_cookies_;

  backup_.reset();
}

void CefRequestImpl::DiscardChanges() {
  base::AutoLock lock_scope(lock_);
  DCHECK(track_changes_);
  DCHECK(backup_on_change_);
  backup_.reset();
}

uint8_t CefRequestImpl::GetChanges() const {
  base::AutoLock lock_scope(lock_);

  uint8_t changes = changes_;
  if (postdata_.get() &&
      static_cast<CefPostDataImpl*>(postdata_.get())->HasChanges()) {
    changes |= kChangedPostData;
  }
  return changes;
}

// From content/child/web_url_loader_impl.cc
// static
network::mojom::ReferrerPolicy
CefRequestImpl::NetReferrerPolicyToBlinkReferrerPolicy(
    cef_referrer_policy_t net_policy) {
  switch (net_policy) {
    case REFERRER_POLICY_CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    case REFERRER_POLICY_REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::
          kNoReferrerWhenDowngradeOriginWhenCrossOrigin;
    case REFERRER_POLICY_ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
    case REFERRER_POLICY_NEVER_CLEAR_REFERRER:
      return network::mojom::ReferrerPolicy::kAlways;
    case REFERRER_POLICY_ORIGIN:
      return network::mojom::ReferrerPolicy::kOrigin;
    case REFERRER_POLICY_CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kSameOrigin;
    case REFERRER_POLICY_ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kStrictOrigin;
    case REFERRER_POLICY_NO_REFERRER:
      return network::mojom::ReferrerPolicy::kNever;
  }
  NOTREACHED();
  return network::mojom::ReferrerPolicy::kDefault;
}

// static
cef_referrer_policy_t CefRequestImpl::BlinkReferrerPolicyToNetReferrerPolicy(
    network::mojom::ReferrerPolicy blink_policy) {
  switch (blink_policy) {
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return REFERRER_POLICY_CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case network::mojom::ReferrerPolicy::
        kNoReferrerWhenDowngradeOriginWhenCrossOrigin:
      return REFERRER_POLICY_REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return REFERRER_POLICY_ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
    case network::mojom::ReferrerPolicy::kAlways:
      return REFERRER_POLICY_NEVER_CLEAR_REFERRER;
    case network::mojom::ReferrerPolicy::kOrigin:
      return REFERRER_POLICY_ORIGIN;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return REFERRER_POLICY_CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return REFERRER_POLICY_ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;
    case network::mojom::ReferrerPolicy::kNever:
      return REFERRER_POLICY_NO_REFERRER;
    case network::mojom::ReferrerPolicy::kDefault:
      return REFERRER_POLICY_DEFAULT;
  }
  NOTREACHED();
  return REFERRER_POLICY_DEFAULT;
}

void CefRequestImpl::Changed(uint8_t changes) {
  lock_.AssertAcquired();
  if (!track_changes_)
    return;

  if (backup_on_change_) {
    if (!backup_)
      backup_.reset(new Backup());

    // Set the backup values if not already set.
    if ((changes & kChangedUrl) && !(backup_->backups_ & kChangedUrl)) {
      backup_->url_ = url_;
      backup_->backups_ |= kChangedUrl;
    }
    if ((changes & kChangedMethod) && !(backup_->backups_ & kChangedMethod)) {
      backup_->method_ = method_;
      backup_->backups_ |= kChangedMethod;
    }
    if ((changes & kChangedReferrer) &&
        !(backup_->backups_ & kChangedReferrer)) {
      backup_->referrer_url_ = referrer_url_;
      backup_->referrer_policy_ = referrer_policy_;
      backup_->backups_ |= kChangedReferrer;
    }
    if ((changes & kChangedPostData) &&
        !(backup_->backups_ & kChangedPostData)) {
      backup_->postdata_ = postdata_;
      backup_->backups_ |= kChangedPostData;
    }
    if ((changes & kChangedHeaderMap) &&
        !(backup_->backups_ & kChangedHeaderMap)) {
      if (!backup_->headermap_)
        backup_->headermap_.reset(new HeaderMap());
      backup_->headermap_->swap(headermap_);
      backup_->backups_ |= kChangedHeaderMap;
    }
    if ((changes & kChangedFlags) && !(backup_->backups_ & kChangedFlags)) {
      backup_->flags_ = flags_;
      backup_->backups_ |= kChangedFlags;
    }
    if ((changes & kChangedFirstPartyForCookies) &&
        !(backup_->backups_ & kChangedFirstPartyForCookies)) {
      backup_->first_party_for_cookies_ = first_party_for_cookies_;
      backup_->backups_ |= kChangedFirstPartyForCookies;
    }
  }

  changes_ |= changes;
}

bool CefRequestImpl::ShouldSet(uint8_t changes, bool changed_only) const {
  lock_.AssertAcquired();

  // Always change if changes are not being tracked.
  if (!track_changes_)
    return true;

  // Always change if changed-only was not requested.
  if (!changed_only)
    return true;

  // Change if the |changes| bit flag has been set.
  if ((changes_ & changes) == changes)
    return true;

  if ((changes & kChangedPostData) == kChangedPostData) {
    // Change if the post data object was modified directly.
    if (postdata_.get() &&
        static_cast<CefPostDataImpl*>(postdata_.get())->HasChanges()) {
      return true;
    }
  }

  return false;
}

void CefRequestImpl::Reset() {
  lock_.AssertAcquired();
  DCHECK(!read_only_);

  url_ = GURL();
  method_ = "GET";
  referrer_url_ = GURL();
  referrer_policy_ = REFERRER_POLICY_DEFAULT;
  postdata_ = NULL;
  headermap_.clear();
  resource_type_ = RT_SUB_RESOURCE;
  transition_type_ = TT_EXPLICIT;
  identifier_ = 0U;
  flags_ = UR_FLAG_NONE;
  first_party_for_cookies_ = GURL();

  changes_ = kChangedNone;
}

// CefPostData ----------------------------------------------------------------

// static
CefRefPtr<CefPostData> CefPostData::Create() {
  CefRefPtr<CefPostData> postdata(new CefPostDataImpl());
  return postdata;
}

// CefPostDataImpl ------------------------------------------------------------

CefPostDataImpl::CefPostDataImpl()
    : read_only_(false),
      has_excluded_elements_(false),
      track_changes_(false),
      has_changes_(false) {}

bool CefPostDataImpl::IsReadOnly() {
  base::AutoLock lock_scope(lock_);
  return read_only_;
}

bool CefPostDataImpl::HasExcludedElements() {
  base::AutoLock lock_scope(lock_);
  return has_excluded_elements_;
}

size_t CefPostDataImpl::GetElementCount() {
  base::AutoLock lock_scope(lock_);
  return elements_.size();
}

void CefPostDataImpl::GetElements(ElementVector& elements) {
  base::AutoLock lock_scope(lock_);
  elements = elements_;
}

bool CefPostDataImpl::RemoveElement(CefRefPtr<CefPostDataElement> element) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN(false);

  ElementVector::iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    if (it->get() == element.get()) {
      elements_.erase(it);
      Changed();
      return true;
    }
  }

  return false;
}

bool CefPostDataImpl::AddElement(CefRefPtr<CefPostDataElement> element) {
  bool found = false;

  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN(false);

  // check that the element isn't already in the list before adding
  ElementVector::const_iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    if (it->get() == element.get()) {
      found = true;
      break;
    }
  }

  if (!found) {
    elements_.push_back(element);
    Changed();
  }

  return !found;
}

void CefPostDataImpl::RemoveElements() {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  elements_.clear();
  Changed();
}

void CefPostDataImpl::Set(const network::ResourceRequestBody& body) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  CefRefPtr<CefPostDataElement> postelem;

  for (const auto& element : *body.elements()) {
    postelem = CefPostDataElement::Create();
    static_cast<CefPostDataElementImpl*>(postelem.get())->Set(element);
    AddElement(postelem);
  }
}

scoped_refptr<network::ResourceRequestBody> CefPostDataImpl::GetBody() const {
  base::AutoLock lock_scope(lock_);

  scoped_refptr<network::ResourceRequestBody> body =
      new network::ResourceRequestBody();
  for (const auto& element : elements_) {
    static_cast<CefPostDataElementImpl*>(element.get())->Get(*body);
  }
  return body;
}

void CefPostDataImpl::Set(const net::UploadData& data) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  CefRefPtr<CefPostDataElement> postelem;

  for (const auto& element : data.elements()) {
    postelem = CefPostDataElement::Create();
    static_cast<CefPostDataElementImpl*>(postelem.get())->Set(*element);
    AddElement(postelem);
  }
}

void CefPostDataImpl::Set(const net::UploadDataStream& data_stream) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  CefRefPtr<CefPostDataElement> postelem;

  const UploadElementReaders* elements = data_stream.GetElementReaders();
  if (elements) {
    UploadElementReaders::const_iterator it = elements->begin();
    for (; it != elements->end(); ++it) {
      postelem = CefPostDataElement::Create();
      static_cast<CefPostDataElementImpl*>(postelem.get())->Set(**it);
      if (postelem->GetType() != PDE_TYPE_EMPTY)
        AddElement(postelem);
      else if (!has_excluded_elements_)
        has_excluded_elements_ = true;
    }
  }
}

void CefPostDataImpl::Get(net::UploadData& data) const {
  base::AutoLock lock_scope(lock_);

  net::UploadData::ElementsVector data_elements;
  for (const auto& element : elements_) {
    std::unique_ptr<net::UploadElement> data_element =
        std::make_unique<net::UploadElement>();
    static_cast<CefPostDataElementImpl*>(element.get())
        ->Get(*data_element.get());
    data_elements.push_back(std::move(data_element));
  }
  data.swap_elements(&data_elements);
}

std::unique_ptr<net::UploadDataStream> CefPostDataImpl::Get() const {
  base::AutoLock lock_scope(lock_);

  UploadElementReaders element_readers;
  for (const auto& element : elements_) {
    element_readers.push_back(
        static_cast<CefPostDataElementImpl*>(element.get())->Get());
  }

  return std::make_unique<net::ElementsUploadDataStream>(
      std::move(element_readers), 0);
}

void CefPostDataImpl::Set(const blink::WebHTTPBody& data) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  CefRefPtr<CefPostDataElement> postelem;
  blink::WebHTTPBody::Element element;
  size_t size = data.ElementCount();
  for (size_t i = 0; i < size; ++i) {
    if (data.ElementAt(i, element)) {
      postelem = CefPostDataElement::Create();
      static_cast<CefPostDataElementImpl*>(postelem.get())->Set(element);
      AddElement(postelem);
    }
  }
}

void CefPostDataImpl::Get(blink::WebHTTPBody& data) const {
  base::AutoLock lock_scope(lock_);

  blink::WebHTTPBody::Element element;
  ElementVector::const_iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    static_cast<CefPostDataElementImpl*>(it->get())->Get(element);
    if (element.type == blink::WebHTTPBody::Element::kTypeData) {
      data.AppendData(element.data);
    } else if (element.type == blink::WebHTTPBody::Element::kTypeFile) {
      data.AppendFile(element.file_path);
    } else {
      NOTREACHED();
    }
  }
}

void CefPostDataImpl::SetReadOnly(bool read_only) {
  base::AutoLock lock_scope(lock_);
  if (read_only_ == read_only)
    return;

  read_only_ = read_only;

  ElementVector::const_iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    static_cast<CefPostDataElementImpl*>(it->get())->SetReadOnly(read_only);
  }
}

void CefPostDataImpl::SetTrackChanges(bool track_changes) {
  base::AutoLock lock_scope(lock_);
  if (track_changes_ == track_changes)
    return;

  track_changes_ = track_changes;
  has_changes_ = false;

  ElementVector::const_iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    static_cast<CefPostDataElementImpl*>(it->get())->SetTrackChanges(
        track_changes);
  }
}

bool CefPostDataImpl::HasChanges() const {
  base::AutoLock lock_scope(lock_);
  if (has_changes_)
    return true;

  ElementVector::const_iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    if (static_cast<CefPostDataElementImpl*>(it->get())->HasChanges())
      return true;
  }

  return false;
}

void CefPostDataImpl::Changed() {
  lock_.AssertAcquired();
  if (track_changes_ && !has_changes_)
    has_changes_ = true;
}

// CefPostDataElement ---------------------------------------------------------

// static
CefRefPtr<CefPostDataElement> CefPostDataElement::Create() {
  CefRefPtr<CefPostDataElement> element(new CefPostDataElementImpl());
  return element;
}

// CefPostDataElementImpl -----------------------------------------------------

CefPostDataElementImpl::CefPostDataElementImpl()
    : type_(PDE_TYPE_EMPTY),
      read_only_(false),
      track_changes_(false),
      has_changes_(false) {
  memset(&data_, 0, sizeof(data_));
}

CefPostDataElementImpl::~CefPostDataElementImpl() {
  Cleanup();
}

bool CefPostDataElementImpl::IsReadOnly() {
  base::AutoLock lock_scope(lock_);
  return read_only_;
}

void CefPostDataElementImpl::SetToEmpty() {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  Cleanup();
  Changed();
}

void CefPostDataElementImpl::SetToFile(const CefString& fileName) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  // Clear any data currently in the element
  Cleanup();

  // Assign the new data
  type_ = PDE_TYPE_FILE;
  cef_string_copy(fileName.c_str(), fileName.length(), &data_.filename);

  Changed();
}

void CefPostDataElementImpl::SetToBytes(size_t size, const void* bytes) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  // Clear any data currently in the element
  Cleanup();

  // Assign the new data
  void* data = malloc(size);
  DCHECK(data != NULL);
  if (data == NULL)
    return;

  memcpy(data, bytes, size);

  type_ = PDE_TYPE_BYTES;
  data_.bytes.bytes = data;
  data_.bytes.size = size;

  Changed();
}

CefPostDataElement::Type CefPostDataElementImpl::GetType() {
  base::AutoLock lock_scope(lock_);
  return type_;
}

CefString CefPostDataElementImpl::GetFile() {
  base::AutoLock lock_scope(lock_);
  DCHECK(type_ == PDE_TYPE_FILE);
  CefString filename;
  if (type_ == PDE_TYPE_FILE)
    filename.FromString(data_.filename.str, data_.filename.length, false);
  return filename;
}

size_t CefPostDataElementImpl::GetBytesCount() {
  base::AutoLock lock_scope(lock_);
  DCHECK(type_ == PDE_TYPE_BYTES);
  size_t size = 0;
  if (type_ == PDE_TYPE_BYTES)
    size = data_.bytes.size;
  return size;
}

size_t CefPostDataElementImpl::GetBytes(size_t size, void* bytes) {
  base::AutoLock lock_scope(lock_);
  DCHECK(type_ == PDE_TYPE_BYTES);
  size_t rv = 0;
  if (type_ == PDE_TYPE_BYTES) {
    rv = (size < data_.bytes.size ? size : data_.bytes.size);
    memcpy(bytes, data_.bytes.bytes, rv);
  }
  return rv;
}

void CefPostDataElementImpl::Set(const network::DataElement& element) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  if (element.type() == network::mojom::DataElementType::kBytes) {
    SetToBytes(element.length(), element.bytes());
  } else if (element.type() == network::mojom::DataElementType::kFile) {
    SetToFile(element.path().value());
  } else {
    NOTREACHED();
  }
}

void CefPostDataElementImpl::Get(network::ResourceRequestBody& body) const {
  base::AutoLock lock_scope(lock_);

  if (type_ == PDE_TYPE_BYTES) {
    body.AppendBytes(static_cast<char*>(data_.bytes.bytes), data_.bytes.size);
  } else if (type_ == PDE_TYPE_FILE) {
    base::FilePath path = base::FilePath(CefString(&data_.filename));
    body.AppendFileRange(path, 0, std::numeric_limits<uint64_t>::max(),
                         base::Time());
  } else {
    NOTREACHED();
  }
}

void CefPostDataElementImpl::Set(const net::UploadElement& element) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  if (element.type() == net::UploadElement::TYPE_BYTES) {
    SetToBytes(element.bytes_length(), element.bytes());
  } else if (element.type() == net::UploadElement::TYPE_FILE) {
    SetToFile(element.file_path().value());
  } else {
    NOTREACHED();
  }
}

void CefPostDataElementImpl::Set(
    const net::UploadElementReader& element_reader) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  const net::UploadBytesElementReader* bytes_reader =
      element_reader.AsBytesReader();
  if (bytes_reader) {
    SetToBytes(bytes_reader->length(), bytes_reader->bytes());
    return;
  }

  const net::UploadFileElementReader* file_reader =
      element_reader.AsFileReader();
  if (file_reader) {
    SetToFile(file_reader->path().value());
    return;
  }

  // Chunked uploads cannot currently be represented.
  SetToEmpty();
}

void CefPostDataElementImpl::Get(net::UploadElement& element) const {
  base::AutoLock lock_scope(lock_);

  if (type_ == PDE_TYPE_BYTES) {
    element.SetToBytes(static_cast<char*>(data_.bytes.bytes), data_.bytes.size);
  } else if (type_ == PDE_TYPE_FILE) {
    base::FilePath path = base::FilePath(CefString(&data_.filename));
    element.SetToFilePath(path);
  } else {
    NOTREACHED();
  }
}

std::unique_ptr<net::UploadElementReader> CefPostDataElementImpl::Get() const {
  base::AutoLock lock_scope(lock_);

  if (type_ == PDE_TYPE_BYTES) {
    net::UploadElement* element = new net::UploadElement();
    element->SetToBytes(static_cast<char*>(data_.bytes.bytes),
                        data_.bytes.size);
    return std::make_unique<BytesElementReader>(base::WrapUnique(element));
  } else if (type_ == PDE_TYPE_FILE) {
    net::UploadElement* element = new net::UploadElement();
    base::FilePath path = base::FilePath(CefString(&data_.filename));
    element->SetToFilePath(path);
    return std::make_unique<FileElementReader>(base::WrapUnique(element));
  } else {
    NOTREACHED();
    return nullptr;
  }
}

void CefPostDataElementImpl::Set(const blink::WebHTTPBody::Element& element) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  if (element.type == blink::WebHTTPBody::Element::kTypeData) {
    std::string file_contents;
    file_contents.reserve(element.data.size());
    element.data.ForEachSegment([&file_contents](const char* segment,
                                                 size_t segment_size,
                                                 size_t segment_offset) {
      file_contents.append(segment, segment_size);
      return true;
    });
    SetToBytes(file_contents.size(), file_contents.data());
  } else if (element.type == blink::WebHTTPBody::Element::kTypeFile) {
    SetToFile(element.file_path.Utf16());
  } else {
    NOTREACHED();
  }
}

void CefPostDataElementImpl::Get(blink::WebHTTPBody::Element& element) const {
  base::AutoLock lock_scope(lock_);

  if (type_ == PDE_TYPE_BYTES) {
    element.type = blink::WebHTTPBody::Element::kTypeData;
    element.data.Assign(static_cast<char*>(data_.bytes.bytes),
                        data_.bytes.size);
  } else if (type_ == PDE_TYPE_FILE) {
    element.type = blink::WebHTTPBody::Element::kTypeFile;
    element.file_path =
        blink::WebString::FromUTF16(CefString(&data_.filename).ToString16());
  } else {
    NOTREACHED();
  }
}

void CefPostDataElementImpl::SetReadOnly(bool read_only) {
  base::AutoLock lock_scope(lock_);
  if (read_only_ == read_only)
    return;

  read_only_ = read_only;
}

void CefPostDataElementImpl::SetTrackChanges(bool track_changes) {
  base::AutoLock lock_scope(lock_);
  if (track_changes_ == track_changes)
    return;

  track_changes_ = track_changes;
  has_changes_ = false;
}

bool CefPostDataElementImpl::HasChanges() const {
  base::AutoLock lock_scope(lock_);
  return has_changes_;
}

void CefPostDataElementImpl::Changed() {
  lock_.AssertAcquired();
  if (track_changes_ && !has_changes_)
    has_changes_ = true;
}

void CefPostDataElementImpl::Cleanup() {
  if (type_ == PDE_TYPE_EMPTY)
    return;

  if (type_ == PDE_TYPE_BYTES)
    free(data_.bytes.bytes);
  else if (type_ == PDE_TYPE_FILE)
    cef_string_clear(&data_.filename);
  type_ = PDE_TYPE_EMPTY;
  memset(&data_, 0, sizeof(data_));
}
