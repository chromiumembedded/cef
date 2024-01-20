// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "libcef/browser/net_service/stream_reader_url_loader.h"

#include "libcef/browser/thread_util.h"
#include "libcef/common/net_service/net_service_util.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace net_service {

namespace {

using OnInputStreamOpenedCallback =
    base::OnceCallback<void(std::unique_ptr<StreamReaderURLLoader::Delegate>,
                            std::unique_ptr<InputStream>)>;

// Helper for executing the OnInputStreamOpenedCallback.
class OpenInputStreamWrapper
    : public base::RefCountedThreadSafe<OpenInputStreamWrapper> {
 public:
  OpenInputStreamWrapper(const OpenInputStreamWrapper&) = delete;
  OpenInputStreamWrapper& operator=(const OpenInputStreamWrapper&) = delete;

  [[nodiscard]] static base::OnceClosure Open(
      std::unique_ptr<StreamReaderURLLoader::Delegate> delegate,
      scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner,
      int32_t request_id,
      const network::ResourceRequest& request,
      OnInputStreamOpenedCallback callback) {
    scoped_refptr<OpenInputStreamWrapper> wrapper = new OpenInputStreamWrapper(
        std::move(delegate), work_thread_task_runner,
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(callback));
    wrapper->Start(request_id, request);

    return wrapper->GetCancelCallback();
  }

 private:
  friend class base::RefCountedThreadSafe<OpenInputStreamWrapper>;

  OpenInputStreamWrapper(
      std::unique_ptr<StreamReaderURLLoader::Delegate> delegate,
      scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> job_thread_task_runner,
      OnInputStreamOpenedCallback callback)
      : delegate_(std::move(delegate)),
        work_thread_task_runner_(work_thread_task_runner),
        job_thread_task_runner_(job_thread_task_runner),
        callback_(std::move(callback)) {}
  virtual ~OpenInputStreamWrapper() = default;

  void Start(int32_t request_id, const network::ResourceRequest& request) {
    work_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&OpenInputStreamWrapper::OpenOnWorkThread,
                       base::WrapRefCounted(this), request_id, request));
  }

  base::OnceClosure GetCancelCallback() {
    return base::BindOnce(&OpenInputStreamWrapper::CancelOnJobThread,
                          base::WrapRefCounted(this));
  }

  void CancelOnJobThread() {
    DCHECK(job_thread_task_runner_->RunsTasksInCurrentSequence());
    if (callback_.is_null()) {
      return;
    }

    callback_.Reset();

    work_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&OpenInputStreamWrapper::CancelOnWorkThread,
                                  base::WrapRefCounted(this)));
  }

  void CancelOnWorkThread() {
    DCHECK(work_thread_task_runner_->RunsTasksInCurrentSequence());
    if (is_canceled_) {
      return;
    }
    is_canceled_ = true;
    OnCallback(nullptr);
  }

  void OpenOnWorkThread(int32_t request_id,
                        const network::ResourceRequest& request) {
    DCHECK(work_thread_task_runner_->RunsTasksInCurrentSequence());
    if (is_canceled_) {
      return;
    }

    // |delegate_| will remain valid until OnCallback() is executed on
    // |job_thread_task_runner_|.
    if (!delegate_->OpenInputStream(
            request_id, request,
            base::BindOnce(&OpenInputStreamWrapper::OnCallback,
                           base::WrapRefCounted(this)))) {
      is_canceled_ = true;
      OnCallback(nullptr);
    }
  }

  void OnCallback(std::unique_ptr<InputStream> input_stream) {
    if (!job_thread_task_runner_->RunsTasksInCurrentSequence()) {
      job_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&OpenInputStreamWrapper::OnCallback,
                         base::WrapRefCounted(this), std::move(input_stream)));
      return;
    }

    // May be null if CancelOnJobThread() was called on
    // |job_thread_task_runner_| while OpenOnWorkThread() was pending on
    // |work_thread_task_runner_|.
    if (callback_.is_null()) {
      delegate_.reset();
      return;
    }

    std::move(callback_).Run(std::move(delegate_), std::move(input_stream));
  }

  std::unique_ptr<StreamReaderURLLoader::Delegate> delegate_;

  scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> job_thread_task_runner_;

  // Only accessed on |job_thread_task_runner_|.
  OnInputStreamOpenedCallback callback_;

  // Only accessed on |work_thread_task_runner_|.
  bool is_canceled_ = false;
};

}  // namespace

//==============================
// InputStreamReader
//=============================

// Class responsible for reading from the InputStream.
class InputStreamReader : public base::RefCountedThreadSafe<InputStreamReader> {
 public:
  // The constructor is called on the IO thread, not on the worker thread.
  // Callbacks will be executed on the IO thread.
  InputStreamReader(
      std::unique_ptr<InputStream> stream,
      scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner);

  InputStreamReader(const InputStreamReader&) = delete;
  InputStreamReader& operator=(const InputStreamReader&) = delete;

  // Skip |skip_bytes| number of bytes from |stream_|. |callback| will be
  // executed asynchronously on the IO thread. A negative value passed to
  // |callback| will indicate an error code, a positive value will indicate the
  // number of bytes skipped.
  void Skip(int64_t skip_bytes, InputStream::SkipCallback callback);

  // Read up to |dest_size| bytes from |stream_| into |dest|. |callback| will be
  // executed asynchronously on the IO thread. A negative value passed to
  // |callback| will indicate an error code, a positive value will indicate the
  // number of bytes read.
  void Read(scoped_refptr<net::IOBuffer> dest,
            int dest_size,
            InputStream::ReadCallback callback);

 private:
  friend class base::RefCountedThreadSafe<InputStreamReader>;
  virtual ~InputStreamReader();

  void SkipOnWorkThread(int64_t skip_bytes, InputStream::SkipCallback callback);
  void ReadOnWorkThread(scoped_refptr<net::IOBuffer> buffer,
                        int buffer_size,
                        InputStream::ReadCallback callback);

  void SkipToRequestedRange();

  static void ContinueSkipCallback(
      scoped_refptr<InputStreamReader> stream,
      scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner,
      int callback_id,
      int64_t bytes_skipped);
  static void ContinueReadCallback(
      scoped_refptr<InputStreamReader> stream,
      scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner,
      int callback_id,
      int bytes_read);

  void ContinueSkipCallbackOnWorkThread(int callback_id, int64_t bytes_skipped);
  void ContinueReadCallbackOnWorkThread(int callback_id, int bytes_read);

  void RunSkipCallback(int64_t bytes_skipped);
  void RunReadCallback(int bytes_read);

  static void RunSkipCallbackOnJobThread(
      int64_t bytes_skipped,
      InputStream::SkipCallback skip_callback);
  static void RunReadCallbackOnJobThread(
      int bytes_read,
      InputStream::ReadCallback read_callback);

  std::unique_ptr<InputStream> stream_;

  // All InputStream methods are called this task runner.
  scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner_;

  // All callbacks are executed on this task runner.
  scoped_refptr<base::SingleThreadTaskRunner> job_thread_task_runner_;

  // The below members are only accessed on the work thread.
  int64_t bytes_skipped_;
  int64_t bytes_to_skip_;
  InputStream::SkipCallback pending_skip_callback_;

  scoped_refptr<net::IOBuffer> buffer_;
  InputStream::ReadCallback pending_read_callback_;

  int pending_callback_id_ = -1;

  int next_callback_id_ = 0;
};

InputStreamReader::InputStreamReader(
    std::unique_ptr<InputStream> stream,
    scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner)
    : stream_(std::move(stream)),
      work_thread_task_runner_(work_thread_task_runner),
      job_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {
  CEF_REQUIRE_IOT();
  DCHECK(stream_);
  DCHECK(work_thread_task_runner_);
}

InputStreamReader::~InputStreamReader() = default;

void InputStreamReader::Skip(int64_t skip_bytes,
                             InputStream::SkipCallback callback) {
  work_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InputStreamReader::SkipOnWorkThread,
                                base::WrapRefCounted(this), skip_bytes,
                                std::move(callback)));
}

void InputStreamReader::Read(scoped_refptr<net::IOBuffer> dest,
                             int dest_size,
                             InputStream::ReadCallback callback) {
  work_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InputStreamReader::ReadOnWorkThread,
                                base::WrapRefCounted(this), dest, dest_size,
                                std::move(callback)));
}

void InputStreamReader::SkipOnWorkThread(int64_t skip_bytes,
                                         InputStream::SkipCallback callback) {
  DCHECK(work_thread_task_runner_->RunsTasksInCurrentSequence());

  // No callback should currently be pending.
  DCHECK_EQ(pending_callback_id_, -1);
  DCHECK(pending_skip_callback_.is_null());

  pending_skip_callback_ = std::move(callback);

  if (skip_bytes <= 0) {
    RunSkipCallback(0);
    return;
  }

  bytes_skipped_ = bytes_to_skip_ = skip_bytes;
  SkipToRequestedRange();
}

void InputStreamReader::ReadOnWorkThread(scoped_refptr<net::IOBuffer> dest,
                                         int dest_size,
                                         InputStream::ReadCallback callback) {
  DCHECK(work_thread_task_runner_->RunsTasksInCurrentSequence());

  // No callback should currently be pending.
  DCHECK_EQ(pending_callback_id_, -1);
  DCHECK(pending_read_callback_.is_null());

  pending_read_callback_ = std::move(callback);

  if (!dest_size) {
    RunReadCallback(0);
    return;
  }

  DCHECK_GT(dest_size, 0);

  buffer_ = dest;
  pending_callback_id_ = ++next_callback_id_;

  int bytes_read = 0;
  bool result = stream_->Read(
      buffer_.get(), dest_size, &bytes_read,
      base::BindOnce(&InputStreamReader::ContinueReadCallback,
                     base::WrapRefCounted(this), work_thread_task_runner_,
                     pending_callback_id_));

  // Check if the callback will execute asynchronously.
  if (result && bytes_read == 0) {
    return;
  }

  RunReadCallback(result || bytes_read <= 0 ? bytes_read : net::ERR_FAILED);
}

void InputStreamReader::SkipToRequestedRange() {
  DCHECK(work_thread_task_runner_->RunsTasksInCurrentSequence());

  // Skip to the start of the requested data. This has to be done in a loop
  // because the underlying InputStream is not guaranteed to skip the requested
  // number of bytes.
  do {
    pending_callback_id_ = ++next_callback_id_;

    int64_t skipped = 0;
    bool result = stream_->Skip(
        bytes_to_skip_, &skipped,
        base::BindOnce(&InputStreamReader::ContinueSkipCallback,
                       base::WrapRefCounted(this), work_thread_task_runner_,
                       pending_callback_id_));

    // Check if the callback will execute asynchronously.
    if (result && skipped == 0) {
      return;
    }

    if (!result || skipped <= 0) {
      RunSkipCallback(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      return;
    }
    DCHECK_LE(skipped, bytes_to_skip_);

    bytes_to_skip_ -= skipped;
  } while (bytes_to_skip_ > 0);

  // All done, the requested number of bytes were skipped.
  RunSkipCallback(bytes_skipped_);
}

// static
void InputStreamReader::ContinueSkipCallback(
    scoped_refptr<InputStreamReader> stream,
    scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner,
    int callback_id,
    int64_t bytes_skipped) {
  // Always execute asynchronously.
  work_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&InputStreamReader::ContinueSkipCallbackOnWorkThread,
                     stream, callback_id, bytes_skipped));
}

// static
void InputStreamReader::ContinueReadCallback(
    scoped_refptr<InputStreamReader> stream,
    scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner,
    int callback_id,
    int bytes_read) {
  // Always execute asynchronously.
  work_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&InputStreamReader::ContinueReadCallbackOnWorkThread,
                     stream, callback_id, bytes_read));
}

void InputStreamReader::ContinueSkipCallbackOnWorkThread(
    int callback_id,
    int64_t bytes_skipped) {
  DCHECK(work_thread_task_runner_->RunsTasksInCurrentSequence());

  // Check for out of order callbacks.
  if (pending_callback_id_ != callback_id) {
    return;
  }

  DCHECK_LE(bytes_skipped, bytes_to_skip_);

  if (bytes_to_skip_ > 0 && bytes_skipped > 0) {
    bytes_to_skip_ -= bytes_skipped;
  }

  if (bytes_skipped <= 0) {
    RunSkipCallback(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
  } else if (bytes_to_skip_ > 0) {
    // Continue execution asynchronously.
    work_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InputStreamReader::SkipToRequestedRange, this));
  } else {
    // All done, the requested number of bytes were skipped.
    RunSkipCallback(bytes_skipped_);
  }
}

void InputStreamReader::ContinueReadCallbackOnWorkThread(int callback_id,
                                                         int bytes_read) {
  DCHECK(work_thread_task_runner_->RunsTasksInCurrentSequence());

  // Check for out of order callbacks.
  if (pending_callback_id_ != callback_id) {
    return;
  }

  RunReadCallback(bytes_read);
}

void InputStreamReader::RunSkipCallback(int64_t bytes_skipped) {
  DCHECK(work_thread_task_runner_->RunsTasksInCurrentSequence());

  DCHECK(!pending_skip_callback_.is_null());
  job_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(InputStreamReader::RunSkipCallbackOnJobThread,
                     bytes_skipped, std::move(pending_skip_callback_)));

  // Reset callback state.
  pending_callback_id_ = -1;
  bytes_skipped_ = bytes_to_skip_ = -1;
}

void InputStreamReader::RunReadCallback(int bytes_read) {
  DCHECK(work_thread_task_runner_->RunsTasksInCurrentSequence());

  DCHECK(!pending_read_callback_.is_null());
  job_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(InputStreamReader::RunReadCallbackOnJobThread,
                                bytes_read, std::move(pending_read_callback_)));

  // Reset callback state.
  pending_callback_id_ = -1;
  buffer_ = nullptr;
}

// static
void InputStreamReader::RunSkipCallbackOnJobThread(
    int64_t bytes_skipped,
    InputStream::SkipCallback skip_callback) {
  std::move(skip_callback).Run(bytes_skipped);
}

// static
void InputStreamReader::RunReadCallbackOnJobThread(
    int bytes_read,
    InputStream::ReadCallback read_callback) {
  std::move(read_callback).Run(bytes_read);
}

//==============================
// StreamReaderURLLoader
//=============================

StreamReaderURLLoader::StreamReaderURLLoader(
    int32_t request_id,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    absl::optional<mojo_base::BigBuffer> cached_metadata,
    std::unique_ptr<Delegate> response_delegate)
    : request_id_(request_id),
      request_(request),
      client_(std::move(client)),
      header_client_(std::move(header_client)),
      traffic_annotation_(traffic_annotation),
      cached_metadata_(std::move(cached_metadata)),
      response_delegate_(std::move(response_delegate)),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunner::GetCurrentDefault()),
      weak_factory_(this) {
  DCHECK(response_delegate_);
  // If there is a client error, clean up the request.
  client_.set_disconnect_handler(
      base::BindOnce(&StreamReaderURLLoader::RequestComplete,
                     weak_factory_.GetWeakPtr(), net::ERR_ABORTED));

  // All InputStream work will be performed on this task runner.
  stream_work_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
}

StreamReaderURLLoader::~StreamReaderURLLoader() {
  if (open_cancel_callback_) {
    // Release the Delegate held by OpenInputStreamWrapper.
    std::move(open_cancel_callback_).Run();
  }
}

void StreamReaderURLLoader::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!ParseRange(request_.headers)) {
    RequestComplete(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  if (header_client_.is_bound()) {
    header_client_->OnBeforeSendHeaders(
        request_.headers,
        base::BindOnce(&StreamReaderURLLoader::ContinueWithRequestHeaders,
                       weak_factory_.GetWeakPtr()));
  } else {
    ContinueWithRequestHeaders(net::OK, absl::nullopt);
  }
}

void StreamReaderURLLoader::ContinueWithRequestHeaders(
    int32_t result,
    const absl::optional<net::HttpRequestHeaders>& headers) {
  if (result != net::OK) {
    RequestComplete(result);
    return;
  }

  if (headers) {
    DCHECK(header_client_.is_bound());
    request_.headers = *headers;
  }

  open_cancel_callback_ = OpenInputStreamWrapper::Open(
      // This is intentional - the loader could be deleted while
      // the callback is executing on the background thread. The
      // delegate will be "returned" to the loader once the
      // InputStream open attempt is completed.
      std::move(response_delegate_), stream_work_task_runner_, request_id_,
      request_,
      base::BindOnce(&StreamReaderURLLoader::OnInputStreamOpened,
                     weak_factory_.GetWeakPtr()));
}

void StreamReaderURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  DCHECK(false);
}

void StreamReaderURLLoader::SetPriority(net::RequestPriority priority,
                                        int intra_priority_value) {}

void StreamReaderURLLoader::PauseReadingBodyFromNet() {}

void StreamReaderURLLoader::ResumeReadingBodyFromNet() {}

void StreamReaderURLLoader::OnInputStreamOpened(
    std::unique_ptr<StreamReaderURLLoader::Delegate> returned_delegate,
    std::unique_ptr<InputStream> input_stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(returned_delegate);
  response_delegate_ = std::move(returned_delegate);
  open_cancel_callback_.Reset();

  if (!input_stream) {
    bool restarted = false;
    response_delegate_->OnInputStreamOpenFailed(request_id_, &restarted);
    if (restarted) {
      // The request has been restarted with a new loader.
      // |this| will be deleted.
      CleanUp();
    } else {
      HeadersComplete(net::HTTP_NOT_FOUND, -1);
    }
    return;
  }

  input_stream_reader_ = base::MakeRefCounted<InputStreamReader>(
      std::move(input_stream), stream_work_task_runner_);

  if (!byte_range_valid()) {
    OnReaderSkipCompleted(0);
  } else {
    input_stream_reader_->Skip(
        byte_range_.first_byte_position(),
        base::BindOnce(&StreamReaderURLLoader::OnReaderSkipCompleted,
                       weak_factory_.GetWeakPtr()));
  }
}

void StreamReaderURLLoader::OnReaderSkipCompleted(int64_t bytes_skipped) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!byte_range_valid()) {
    // Expected content length is unspecified.
    HeadersComplete(net::HTTP_OK, -1);
  } else if (bytes_skipped == byte_range_.first_byte_position()) {
    // We skipped the expected number of bytes.
    int64_t expected_content_length = -1;
    if (byte_range_.HasLastBytePosition()) {
      expected_content_length = byte_range_.last_byte_position() -
                                byte_range_.first_byte_position() + 1;
      DCHECK_GE(expected_content_length, 0);
    }
    HeadersComplete(net::HTTP_OK, expected_content_length);
  } else {
    RequestComplete(bytes_skipped < 0 ? bytes_skipped : net::ERR_FAILED);
  }
}

void StreamReaderURLLoader::HeadersComplete(int orig_status_code,
                                            int64_t expected_content_length) {
  DCHECK(thread_checker_.CalledOnValidThread());

  int status_code = orig_status_code;
  std::string status_text =
      net::GetHttpReasonPhrase(static_cast<net::HttpStatusCode>(status_code));
  std::string mime_type, charset;
  int64_t content_length = expected_content_length;
  ResourceResponse::HeaderMap extra_headers;
  response_delegate_->GetResponseHeaders(request_id_, &status_code,
                                         &status_text, &mime_type, &charset,
                                         &content_length, &extra_headers);

  if (status_code < 0) {
    // Early exit if the handler reported an error.
    RequestComplete(status_code);
    return;
  }

  auto pending_response = network::mojom::URLResponseHead::New();
  pending_response->request_start = base::TimeTicks::Now();
  pending_response->response_start = base::TimeTicks::Now();

  auto headers = MakeResponseHeaders(
      status_code, status_text, mime_type, charset, content_length,
      extra_headers, false /* allow_existing_header_override */);
  pending_response->headers = headers;

  if (content_length >= 0) {
    pending_response->content_length = content_length;
  }

  if (!mime_type.empty()) {
    pending_response->mime_type = mime_type;
    if (!charset.empty()) {
      pending_response->charset = charset;
    }
  }

  if (header_client_.is_bound()) {
    header_client_->OnHeadersReceived(
        headers->raw_headers(), net::IPEndPoint(),
        base::BindOnce(&StreamReaderURLLoader::ContinueWithResponseHeaders,
                       weak_factory_.GetWeakPtr(),
                       std::move(pending_response)));
  } else {
    ContinueWithResponseHeaders(std::move(pending_response), net::OK,
                                absl::nullopt, absl::nullopt);
  }
}

void StreamReaderURLLoader::ContinueWithResponseHeaders(
    network::mojom::URLResponseHeadPtr pending_response,
    int32_t result,
    const absl::optional<std::string>& headers,
    const absl::optional<GURL>& redirect_url) {
  if (result != net::OK) {
    RequestComplete(result);
    return;
  }

  if (headers) {
    DCHECK(header_client_.is_bound());
    pending_response->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(*headers);
  }

  auto pending_headers = pending_response->headers;

  // What the length would be if we sent headers over the network. Used to
  // calculate data length.
  header_length_ = pending_headers->raw_headers().length();

  DCHECK(client_.is_bound());

  std::string location;
  const auto has_redirect_url = redirect_url && !redirect_url->is_empty();
  if (has_redirect_url || pending_headers->IsRedirect(&location)) {
    pending_response->encoded_data_length = header_length_;
    pending_response->content_length = 0;
    pending_response->encoded_body_length = nullptr;
    const GURL new_location =
        has_redirect_url ? *redirect_url : request_.url.Resolve(location);
    client_->OnReceiveRedirect(
        MakeRedirectInfo(request_, pending_headers.get(), new_location,
                         pending_headers->response_code()),
        std::move(pending_response));
    // The client will restart the request with a new loader.
    // |this| will be deleted.
    CleanUp();
  } else {
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    if (CreateDataPipe(nullptr /*options*/, producer_handle_,
                       consumer_handle) != MOJO_RESULT_OK) {
      RequestComplete(net::ERR_FAILED);
      return;
    }
    writable_handle_watcher_.Watch(
        producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        base::BindRepeating(&StreamReaderURLLoader::OnDataPipeWritable,
                            base::Unretained(this)));

    client_->OnReceiveResponse(std::move(pending_response),
                               std::move(consumer_handle),
                               std::move(cached_metadata_));
    ReadMore();
  }
}

void StreamReaderURLLoader::ReadMore() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!pending_buffer_.get());

  MojoResult mojo_result = network::NetToMojoPendingBuffer::BeginWrite(
      &producer_handle_, &pending_buffer_);
  if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full. We need to wait for it to have more space.
    writable_handle_watcher_.ArmOrNotify();
    return;
  } else if (mojo_result == MOJO_RESULT_FAILED_PRECONDITION) {
    // The data pipe consumer handle has been closed.
    RequestComplete(net::ERR_ABORTED);
    return;
  } else if (mojo_result != MOJO_RESULT_OK) {
    // The body stream is in a bad state. Bail out.
    RequestComplete(net::ERR_UNEXPECTED);
    return;
  }
  scoped_refptr<net::IOBuffer> buffer(
      new network::NetToMojoIOBuffer(pending_buffer_.get()));

  if (!input_stream_reader_.get()) {
    // This will happen if opening the InputStream fails in which case the
    // error is communicated by setting the HTTP response status header rather
    // than failing the request during the header fetch phase.
    OnReaderReadCompleted(0);
    return;
  }

  input_stream_reader_->Read(
      buffer, base::checked_cast<int>(pending_buffer_->size()),
      base::BindOnce(&StreamReaderURLLoader::OnReaderReadCompleted,
                     weak_factory_.GetWeakPtr()));
}

void StreamReaderURLLoader::OnDataPipeWritable(MojoResult result) {
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    RequestComplete(net::ERR_ABORTED);
    return;
  }
  DCHECK_EQ(result, MOJO_RESULT_OK) << result;

  ReadMore();
}

void StreamReaderURLLoader::OnReaderReadCompleted(int bytes_read) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(pending_buffer_);
  if (bytes_read < 0) {
    // Error case.
    RequestComplete(bytes_read);
    return;
  }
  if (bytes_read == 0) {
    // Eof, read completed.
    pending_buffer_->Complete(0);
    RequestComplete(net::OK);
    return;
  }
  producer_handle_ = pending_buffer_->Complete(bytes_read);
  pending_buffer_ = nullptr;

  client_->OnTransferSizeUpdated(bytes_read);
  total_bytes_read_ += bytes_read;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&StreamReaderURLLoader::ReadMore,
                                weak_factory_.GetWeakPtr()));
}

void StreamReaderURLLoader::RequestComplete(int status_code) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto status = network::URLLoaderCompletionStatus(status_code);
  status.completion_time = base::TimeTicks::Now();
  status.encoded_data_length = total_bytes_read_ + header_length_;
  status.encoded_body_length = total_bytes_read_;
  // We don't support decoders, so use the same value.
  status.decoded_body_length = total_bytes_read_;

  client_->OnComplete(status);
  CleanUp();
}

void StreamReaderURLLoader::CleanUp() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Resets the watchers and pipes, so that we will never be called back.
  writable_handle_watcher_.Cancel();
  pending_buffer_ = nullptr;
  producer_handle_.reset();

  // Manages its own lifetime.
  delete this;
}

bool StreamReaderURLLoader::ParseRange(const net::HttpRequestHeaders& headers) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // This loader only cares about the Range header so that we know how many
    // bytes in the stream to skip and how many to read after that.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      // In case of multi-range request only use the first range.
      // We don't support multirange requests.
      if (ranges.size() == 1) {
        byte_range_ = ranges[0];
      }
    } else {
      // This happens if the range header could not be parsed or is invalid.
      return false;
    }
  }
  return true;
}

bool StreamReaderURLLoader::byte_range_valid() const {
  return byte_range_.IsValid() && byte_range_.first_byte_position() >= 0;
}

}  // namespace net_service
