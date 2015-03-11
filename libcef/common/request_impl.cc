// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <string>
#include <vector>

#include "libcef/common/http_header_utils.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/task_runner_impl.h"
#include "libcef/common/upload_data.h"

#include "base/logging.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_element_reader.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_file_element_reader.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

namespace {

// A subclass of net::UploadBytesElementReader that keeps the associated
// UploadElement alive until the request completes.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  explicit BytesElementReader(scoped_ptr<net::UploadElement> element)
      : net::UploadBytesElementReader(element->bytes(),
                                      element->bytes_length()),
        element_(element.Pass()) {
    DCHECK_EQ(net::UploadElement::TYPE_BYTES, element_->type());
  }

 private:
  scoped_ptr<net::UploadElement> element_;

  DISALLOW_COPY_AND_ASSIGN(BytesElementReader);
};

base::TaskRunner* GetFileTaskRunner() {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      CefTaskRunnerImpl::GetTaskRunner(TID_FILE);
  DCHECK(task_runner.get());
  return task_runner.get();
}

// A subclass of net::UploadFileElementReader that keeps the associated
// UploadElement alive until the request completes.
class FileElementReader : public net::UploadFileElementReader {
 public:
  explicit FileElementReader(scoped_ptr<net::UploadElement> element)
      : net::UploadFileElementReader(
            GetFileTaskRunner(),
            element->file_path(),
            element->file_range_offset(),
            element->file_range_length(),
            element->expected_file_modification_time()),
        element_(element.Pass()) {
    DCHECK_EQ(net::UploadElement::TYPE_FILE, element_->type());
  }

 private:
  scoped_ptr<net::UploadElement> element_;

  DISALLOW_COPY_AND_ASSIGN(FileElementReader);
};

}  // namespace


#define CHECK_READONLY_RETURN(val) \
  if (read_only_) { \
    NOTREACHED() << "object is read only"; \
    return val; \
  }

#define CHECK_READONLY_RETURN_VOID() \
  if (read_only_) { \
    NOTREACHED() << "object is read only"; \
    return; \
  }


// CefRequest -----------------------------------------------------------------

// static
CefRefPtr<CefRequest> CefRequest::Create() {
  CefRefPtr<CefRequest> request(new CefRequestImpl());
  return request;
}


// CefRequestImpl -------------------------------------------------------------

CefRequestImpl::CefRequestImpl()
    : method_("GET"),
      resource_type_(RT_SUB_RESOURCE),
      transition_type_(TT_EXPLICIT),
      identifier_(0U),
      flags_(UR_FLAG_NONE),
      read_only_(false) {
}

bool CefRequestImpl::IsReadOnly() {
  base::AutoLock lock_scope(lock_);
  return read_only_;
}

CefString CefRequestImpl::GetURL() {
  base::AutoLock lock_scope(lock_);
  return url_;
}

void CefRequestImpl::SetURL(const CefString& url) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  url_ = url;
}

CefString CefRequestImpl::GetMethod() {
  base::AutoLock lock_scope(lock_);
  return method_;
}

void CefRequestImpl::SetMethod(const CefString& method) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  method_ = method;
}

CefRefPtr<CefPostData> CefRequestImpl::GetPostData() {
  base::AutoLock lock_scope(lock_);
  return postdata_;
}

void CefRequestImpl::SetPostData(CefRefPtr<CefPostData> postData) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  postdata_ = postData;
}

void CefRequestImpl::GetHeaderMap(HeaderMap& headerMap) {
  base::AutoLock lock_scope(lock_);
  headerMap = headermap_;
}

void CefRequestImpl::SetHeaderMap(const HeaderMap& headerMap) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  headermap_ = headerMap;
}

void CefRequestImpl::Set(const CefString& url,
                         const CefString& method,
                         CefRefPtr<CefPostData> postData,
                         const HeaderMap& headerMap) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  url_ = url;
  method_ = method;
  postdata_ = postData;
  headermap_ = headerMap;
}

int CefRequestImpl::GetFlags() {
  base::AutoLock lock_scope(lock_);
  return flags_;
}
void CefRequestImpl::SetFlags(int flags) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  flags_ = flags;
}

CefString CefRequestImpl::GetFirstPartyForCookies() {
  base::AutoLock lock_scope(lock_);
  return first_party_for_cookies_;
}
void CefRequestImpl::SetFirstPartyForCookies(const CefString& url) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  first_party_for_cookies_ = url;
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

void CefRequestImpl::Set(net::URLRequest* request) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  url_ = request->url().spec();
  method_ = request->method();
  first_party_for_cookies_ = request->first_party_for_cookies().spec();
  identifier_ = request->identifier();

  net::HttpRequestHeaders headers = request->extra_request_headers();

  // URLRequest::SetReferrer ensures that we do not send username and password
  // fields in the referrer.
  GURL referrer(request->referrer());

  // Strip Referer from request_info_.extra_headers to prevent, e.g., plugins
  // from overriding headers that are controlled using other means. Otherwise a
  // plugin could set a referrer although sending the referrer is inhibited.
  headers.RemoveHeader(net::HttpRequestHeaders::kReferer);

  // Our consumer should have made sure that this is a safe referrer.  See for
  // instance WebCore::FrameLoader::HideReferrer.
  if (referrer.is_valid())
    headers.SetHeader(net::HttpRequestHeaders::kReferer, referrer.spec());

  // Transfer request headers
  GetHeaderMap(headers, headermap_);

  // Transfer post data, if any
  const net::UploadDataStream* data = request->get_upload();
  if (data) {
    postdata_ = CefPostData::Create();
    static_cast<CefPostDataImpl*>(postdata_.get())->Set(*data);
  } else if (postdata_.get()) {
    postdata_ = NULL;
  }

  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (info) {
    resource_type_ =
        static_cast<cef_resource_type_t>(info->GetResourceType());
    transition_type_ =
        static_cast<cef_transition_type_t>(info->GetPageTransition());
  } else {
    resource_type_ = RT_SUB_RESOURCE;
    transition_type_ = TT_EXPLICIT;
  }
}

void CefRequestImpl::Get(net::URLRequest* request) {
  base::AutoLock lock_scope(lock_);

  request->set_method(method_);
  if (!first_party_for_cookies_.empty()) {
    request->set_first_party_for_cookies(
        GURL(std::string(first_party_for_cookies_)));
  }

  CefString referrerStr;
  referrerStr.FromASCII(net::HttpRequestHeaders::kReferer);
  HeaderMap headerMap = headermap_;
  HeaderMap::iterator it = headerMap.find(referrerStr);
  if (it == headerMap.end()) {
    request->SetReferrer("");
  } else {
    request->SetReferrer(it->second);
    headerMap.erase(it);
  }
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(HttpHeaderUtils::GenerateHeaders(headerMap));
  request->SetExtraRequestHeaders(headers);

  if (postdata_.get()) {
    request->set_upload(
        make_scoped_ptr(static_cast<CefPostDataImpl*>(postdata_.get())->Get()));
  } else if (request->get_upload()) {
    request->set_upload(scoped_ptr<net::UploadDataStream>());
  }
}

void CefRequestImpl::Set(const blink::WebURLRequest& request) {
  DCHECK(!request.isNull());

  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  url_ = request.url().spec().utf16();
  method_ = request.httpMethod();

  const blink::WebHTTPBody& body = request.httpBody();
  if (!body.isNull()) {
    postdata_ = new CefPostDataImpl();
    static_cast<CefPostDataImpl*>(postdata_.get())->Set(body);
  } else if (postdata_.get()) {
    postdata_ = NULL;
  }

  headermap_.clear();
  GetHeaderMap(request, headermap_);

  flags_ = UR_FLAG_NONE;
  if (request.cachePolicy() == blink::WebURLRequest::ReloadIgnoringCacheData)
    flags_ |= UR_FLAG_SKIP_CACHE;
  if (request.allowStoredCredentials())
    flags_ |= UR_FLAG_ALLOW_CACHED_CREDENTIALS;
  if (request.reportUploadProgress())
    flags_ |= UR_FLAG_REPORT_UPLOAD_PROGRESS;
  if (request.reportRawHeaders())
    flags_ |= UR_FLAG_REPORT_RAW_HEADERS;

  first_party_for_cookies_ = request.firstPartyForCookies().spec().utf16();
}

void CefRequestImpl::Get(blink::WebURLRequest& request) {
  request.initialize();
  base::AutoLock lock_scope(lock_);

  GURL gurl = GURL(url_.ToString());
  request.setURL(blink::WebURL(gurl));

  std::string method(method_);
  request.setHTTPMethod(blink::WebString::fromUTF8(method.c_str()));

  blink::WebHTTPBody body;
  if (postdata_.get()) {
    body.initialize();
    static_cast<CefPostDataImpl*>(postdata_.get())->Get(body);
    request.setHTTPBody(body);
  }

  SetHeaderMap(headermap_, request);

  request.setCachePolicy((flags_ & UR_FLAG_SKIP_CACHE) ?
      blink::WebURLRequest::ReloadIgnoringCacheData :
      blink::WebURLRequest::UseProtocolCachePolicy);

  #define SETBOOLFLAG(obj, flags, method, FLAG) \
      obj.method((flags & (FLAG)) == (FLAG))

  SETBOOLFLAG(request, flags_, setAllowStoredCredentials,
              UR_FLAG_ALLOW_CACHED_CREDENTIALS);
  SETBOOLFLAG(request, flags_, setReportUploadProgress,
              UR_FLAG_REPORT_UPLOAD_PROGRESS);
  SETBOOLFLAG(request, flags_, setReportRawHeaders,
              UR_FLAG_REPORT_RAW_HEADERS);

  if (!first_party_for_cookies_.empty()) {
    GURL gurl = GURL(first_party_for_cookies_.ToString());
    request.setFirstPartyForCookies(blink::WebURL(gurl));
  }
}

void CefRequestImpl::SetReadOnly(bool read_only) {
  base::AutoLock lock_scope(lock_);
  if (read_only_ == read_only)
    return;

  read_only_ = read_only;

  if (postdata_.get())
    static_cast<CefPostDataImpl*>(postdata_.get())->SetReadOnly(read_only);
}

// static
void CefRequestImpl::GetHeaderMap(const net::HttpRequestHeaders& headers,
                                  HeaderMap& map) {
  if (headers.IsEmpty())
    return;

  net::HttpRequestHeaders::Iterator it(headers);
  do {
    map.insert(std::make_pair(it.name(), it.value()));
  } while (it.GetNext());
}


// static
void CefRequestImpl::GetHeaderMap(const blink::WebURLRequest& request,
                                  HeaderMap& map) {
  class HeaderVisitor : public blink::WebHTTPHeaderVisitor {
   public:
    explicit HeaderVisitor(HeaderMap* map) : map_(map) {}

    void visitHeader(const blink::WebString& name,
                     const blink::WebString& value) override {
      map_->insert(std::make_pair(base::string16(name),
                                  base::string16(value)));
    }

   private:
    HeaderMap* map_;
  };

  HeaderVisitor visitor(&map);
  request.visitHTTPHeaderFields(&visitor);
}

// static
void CefRequestImpl::SetHeaderMap(const HeaderMap& map,
                                  blink::WebURLRequest& request) {
  HeaderMap::const_iterator it = map.begin();
  for (; it != map.end(); ++it)
    request.setHTTPHeaderField(base::string16(it->first),
                               base::string16(it->second));
}

// CefPostData ----------------------------------------------------------------

// static
CefRefPtr<CefPostData> CefPostData::Create() {
  CefRefPtr<CefPostData> postdata(new CefPostDataImpl());
  return postdata;
}


// CefPostDataImpl ------------------------------------------------------------

CefPostDataImpl::CefPostDataImpl()
  : read_only_(false) {
}

bool CefPostDataImpl::IsReadOnly() {
  base::AutoLock lock_scope(lock_);
  return read_only_;
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

  if (!found)
    elements_.push_back(element);

  return !found;
}

void CefPostDataImpl::RemoveElements() {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();
  elements_.clear();
}

void CefPostDataImpl::Set(const net::UploadData& data) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  CefRefPtr<CefPostDataElement> postelem;

  const ScopedVector<net::UploadElement>& elements = data.elements();
  ScopedVector<net::UploadElement>::const_iterator it = elements.begin();
  for (; it != elements.end(); ++it) {
    postelem = CefPostDataElement::Create();
    static_cast<CefPostDataElementImpl*>(postelem.get())->Set(**it);
    AddElement(postelem);
  }
}

void CefPostDataImpl::Set(const net::UploadDataStream& data_stream) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  CefRefPtr<CefPostDataElement> postelem;

  const ScopedVector<net::UploadElementReader>* elements =
      data_stream.GetElementReaders();
  if (elements) {
    ScopedVector<net::UploadElementReader>::const_iterator it =
        elements->begin();
    for (; it != elements->end(); ++it) {
      postelem = CefPostDataElement::Create();
      static_cast<CefPostDataElementImpl*>(postelem.get())->Set(**it);
      AddElement(postelem);
    }
  }
}

void CefPostDataImpl::Get(net::UploadData& data) {
  base::AutoLock lock_scope(lock_);

  ScopedVector<net::UploadElement> data_elements;
  ElementVector::const_iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    net::UploadElement* element = new net::UploadElement();
    static_cast<CefPostDataElementImpl*>(it->get())->Get(*element);
    data_elements.push_back(element);
  }
  data.swap_elements(&data_elements);
}

net::UploadDataStream* CefPostDataImpl::Get() {
  base::AutoLock lock_scope(lock_);

  ScopedVector<net::UploadElementReader> element_readers;
  ElementVector::const_iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    element_readers.push_back(
        static_cast<CefPostDataElementImpl*>(it->get())->Get());
  }

  return new net::ElementsUploadDataStream(element_readers.Pass(), 0);
}

void CefPostDataImpl::Set(const blink::WebHTTPBody& data) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  CefRefPtr<CefPostDataElement> postelem;
  blink::WebHTTPBody::Element element;
  size_t size = data.elementCount();
  for (size_t i = 0; i < size; ++i) {
    if (data.elementAt(i, element)) {
      postelem = CefPostDataElement::Create();
      static_cast<CefPostDataElementImpl*>(postelem.get())->Set(element);
      AddElement(postelem);
    }
  }
}

void CefPostDataImpl::Get(blink::WebHTTPBody& data) {
  base::AutoLock lock_scope(lock_);

  blink::WebHTTPBody::Element element;
  ElementVector::iterator it = elements_.begin();
  for (; it != elements_.end(); ++it) {
    static_cast<CefPostDataElementImpl*>(it->get())->Get(element);
    if (element.type == blink::WebHTTPBody::Element::TypeData) {
      data.appendData(element.data);
    } else if (element.type == blink::WebHTTPBody::Element::TypeFile) {
      data.appendFile(element.filePath);
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

// CefPostDataElement ---------------------------------------------------------

// static
CefRefPtr<CefPostDataElement> CefPostDataElement::Create() {
  CefRefPtr<CefPostDataElement> element(new CefPostDataElementImpl());
  return element;
}


// CefPostDataElementImpl -----------------------------------------------------

CefPostDataElementImpl::CefPostDataElementImpl()
  : type_(PDE_TYPE_EMPTY),
    read_only_(false) {
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
}

void CefPostDataElementImpl::SetToFile(const CefString& fileName) {
  base::AutoLock lock_scope(lock_);
  CHECK_READONLY_RETURN_VOID();

  // Clear any data currently in the element
  Cleanup();

  // Assign the new data
  type_ = PDE_TYPE_FILE;
  cef_string_copy(fileName.c_str(), fileName.length(), &data_.filename);
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

  NOTREACHED();
}

void CefPostDataElementImpl::Get(net::UploadElement& element) {
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

net::UploadElementReader* CefPostDataElementImpl::Get() {
  base::AutoLock lock_scope(lock_);

  if (type_ == PDE_TYPE_BYTES) {
    net::UploadElement* element = new net::UploadElement();
    element->SetToBytes(static_cast<char*>(data_.bytes.bytes),
                        data_.bytes.size);
    return new BytesElementReader(make_scoped_ptr(element));
  } else if (type_ == PDE_TYPE_FILE) {
    net::UploadElement* element = new net::UploadElement();
    base::FilePath path = base::FilePath(CefString(&data_.filename));
    element->SetToFilePath(path);
    return new FileElementReader(make_scoped_ptr(element));
  } else {
    NOTREACHED();
    return NULL;
  }
}

void CefPostDataElementImpl::Set(const blink::WebHTTPBody::Element& element) {
  {
    base::AutoLock lock_scope(lock_);
    CHECK_READONLY_RETURN_VOID();
  }

  if (element.type == blink::WebHTTPBody::Element::TypeData) {
    SetToBytes(element.data.size(),
        static_cast<const void*>(element.data.data()));
  } else if (element.type == blink::WebHTTPBody::Element::TypeFile) {
    SetToFile(base::string16(element.filePath));
  } else {
    NOTREACHED();
  }
}

void CefPostDataElementImpl::Get(blink::WebHTTPBody::Element& element) {
  base::AutoLock lock_scope(lock_);

  if (type_ == PDE_TYPE_BYTES) {
    element.type = blink::WebHTTPBody::Element::TypeData;
    element.data.assign(
        static_cast<char*>(data_.bytes.bytes), data_.bytes.size);
  } else if (type_ == PDE_TYPE_FILE) {
    element.type = blink::WebHTTPBody::Element::TypeFile;
    element.filePath.assign(base::string16(CefString(&data_.filename)));
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
