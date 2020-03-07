// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_STREAM_READER_URL_LOADER_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_STREAM_READER_URL_LOADER_H_

#include <map>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/http/http_byte_range.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace net_service {

class InputStreamReader;

// Abstract class representing an input stream. All methods are called in
// sequence on a worker thread, but not necessarily on the same thread.
class InputStream {
 public:
  virtual ~InputStream() {}

  // Callback for asynchronous continuation of Skip(). If |bytes_skipped| > 0
  // then either Skip() will be called again until the requested number of
  // bytes have been skipped or the request will proceed. If |bytes_skipped|
  // <= 0 the request will fail with net::ERR_REQUEST_RANGE_NOT_SATISFIABLE.
  using SkipCallback = base::OnceCallback<void(int64_t /* bytes_skipped */)>;

  // Skip over and discard |n| bytes of data from this input stream. If data
  // is available immediately set |bytes_skipped| to the number of of bytes
  // skipped and return true. To read the data at a later time set
  // |bytes_skipped| to 0, return true and execute |callback| when the data is
  // available. To indicate failure set |bytes_skipped| to < 0 (e.g.
  // net::ERR_FAILED) and return false.
  virtual bool Skip(int64_t n,
                    int64_t* bytes_skipped,
                    SkipCallback callback) = 0;

  // Callback for asynchronous continuation of Read(). If |bytes_read| == 0
  // the response will be considered complete. If |bytes_read| > 0 then Read()
  // will be called again until the request is complete (based on either the
  // result or the expected content length). If |bytes_read| < 0 then the
  // request will fail and the |bytes_read| value will be treated as the error
  // code.
  using ReadCallback = base::OnceCallback<void(int /* bytes_read */)>;

  // Read response data. If data is available immediately copy up to |length|
  // bytes into |dest|, set |bytes_read| to the number of bytes copied, and
  // return true. To read the data at a later time set |bytes_read| to 0, return
  // true and execute |callback| when the data is available. To indicate
  // response completion set |bytes_read| to 0 and return false. To indicate
  // failure set |bytes_read| to < 0 (e.g. net::ERR_FAILED) and return false.
  virtual bool Read(net::IOBuffer* dest,
                    int length,
                    int* bytes_read,
                    ReadCallback callback) = 0;
};

// Unique identifier for RequestHandler callbacks.
// Based on components/viz/common/surfaces/frame_sink_id.h
class RequestId {
 public:
  constexpr RequestId() : request_id_(0), routing_id_(0) {}

  constexpr RequestId(const RequestId& other)
      : request_id_(other.request_id_), routing_id_(other.routing_id_) {}

  constexpr RequestId(uint32_t request_id, uint32_t routing_id)
      : request_id_(request_id), routing_id_(routing_id) {}

  constexpr bool is_valid() const {
    return request_id_ != 0 || routing_id_ != 0;
  }

  constexpr uint32_t request_id() const { return request_id_; }

  constexpr uint32_t routing_id() const { return routing_id_; }

  bool operator==(const RequestId& other) const {
    return request_id_ == other.request_id_ && routing_id_ == other.routing_id_;
  }

  bool operator!=(const RequestId& other) const { return !(*this == other); }

  bool operator<(const RequestId& other) const {
    return std::tie(request_id_, routing_id_) <
           std::tie(other.request_id_, other.routing_id_);
  }

  size_t hash() const { return base::HashInts(request_id_, routing_id_); }

  std::string ToString() const;

  std::string ToString(base::StringPiece debug_label) const;

 private:
  uint32_t request_id_;
  uint32_t routing_id_;
};

std::ostream& operator<<(std::ostream& out, const RequestId& request_id);

struct RequestIdHash {
  size_t operator()(const RequestId& key) const { return key.hash(); }
};

// Abstract class for handling intercepted resource responses. All methods are
// called on the IO thread unless otherwise indicated.
class ResourceResponse {
 public:
  virtual ~ResourceResponse() {}

  // Callback for asynchronous continuation of Open(). If the InputStream is
  // null the request will be canceled.
  using OpenCallback = base::OnceCallback<void(std::unique_ptr<InputStream>)>;

  // This method is called on a worker thread. Return true and execute
  // |callback| to continue the request. Return false to cancel the request.
  // |request| may be different from the request used to create the
  // StreamReaderURLLoader if a redirect was followed.
  virtual bool OpenInputStream(const RequestId& id,
                               const network::ResourceRequest& request,
                               OpenCallback callback) = 0;

  // This method is called to populate the response headers.
  using HeaderMap = std::multimap<std::string, std::string>;
  virtual void GetResponseHeaders(const RequestId& id,
                                  int* status_code,
                                  std::string* reason_phrase,
                                  std::string* mime_type,
                                  std::string* charset,
                                  int64_t* content_length,
                                  HeaderMap* extra_headers) = 0;
};

// Custom URLLoader implementation for loading network responses from stream.
// Methods are called on the IO thread unless otherwise indicated.
// Based on android_webview/browser/network_service/
// android_stream_reader_url_loader.h
class StreamReaderURLLoader : public network::mojom::URLLoader {
 public:
  // Delegate abstraction for obtaining input streams. All methods are called
  // on the IO thread unless otherwise indicated.
  class Delegate : public ResourceResponse {
   public:
    // This method is called if the result of calling OpenInputStream was null.
    // The |restarted| parameter is set to true if the request was restarted
    // with a new loader.
    virtual void OnInputStreamOpenFailed(const RequestId& id,
                                         bool* restarted) = 0;
  };

  StreamReaderURLLoader(
      const RequestId& request_id,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderClientPtr client,
      network::mojom::TrustedHeaderClientPtr header_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      std::unique_ptr<Delegate> response_delegate);
  ~StreamReaderURLLoader() override;

  void Start();

  void ContinueResponse(bool was_redirected);

  // network::mojom::URLLoader methods:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
  void ContinueWithRequestHeaders(
      int32_t result,
      const base::Optional<net::HttpRequestHeaders>& headers);
  void OnInputStreamOpened(std::unique_ptr<Delegate> returned_delegate,
                           std::unique_ptr<InputStream> input_stream);

  void OnReaderSkipCompleted(int64_t bytes_skipped);
  void HeadersComplete(int status_code, int64_t expected_content_length);
  void ContinueWithResponseHeaders(
      network::mojom::URLResponseHeadPtr pending_response,
      int32_t result,
      const base::Optional<std::string>& headers,
      const base::Optional<GURL>& redirect_url);

  void SendBody();
  void ReadMore();
  void OnDataPipeWritable(MojoResult result);
  void OnReaderReadCompleted(int bytes_read);
  void RequestComplete(int status_code);

  void CleanUp();

  bool ParseRange(const net::HttpRequestHeaders& headers);
  bool byte_range_valid() const;

  const RequestId request_id_;

  size_t header_length_ = 0;
  int64_t total_bytes_read_ = 0;

  net::HttpByteRange byte_range_;
  network::ResourceRequest request_;
  network::mojom::URLLoaderClientPtr client_;
  network::mojom::TrustedHeaderClientPtr header_client_;
  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  std::unique_ptr<Delegate> response_delegate_;
  scoped_refptr<InputStreamReader> input_stream_reader_;

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_buffer_;
  mojo::SimpleWatcher writable_handle_watcher_;
  base::ThreadChecker thread_checker_;

  scoped_refptr<base::SequencedTaskRunner> stream_work_task_runner_;

  base::OnceClosure open_cancel_callback_;

  base::WeakPtrFactory<StreamReaderURLLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StreamReaderURLLoader);
};

}  // namespace net_service

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_STREAM_READER_URL_LOADER_H_
