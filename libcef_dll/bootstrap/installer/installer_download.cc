// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_download.h"

#include <windows.h>

#include <winhttp.h>

#include <atomic>
#include <limits>
#include <optional>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "crypto/hash.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

#pragma comment(lib, "winhttp.lib")

namespace cef_installer {

namespace {

// User-Agent string for WinHTTP requests.
constexpr wchar_t kUserAgent[] = L"CEF Installer/1.0";

// Connection and response timeouts in milliseconds.
constexpr int kConnectTimeoutMs = 30000;  // 30 seconds
constexpr int kSendTimeoutMs = 30000;     // 30 seconds
constexpr int kReceiveTimeoutMs = 60000;  // 60 seconds

// Buffer size for reading response body.
constexpr size_t kReadBufferSize = 64 * 1024;  // 64 KB

// RAII wrapper for HINTERNET handles.
class ScopedWinHttpHandle {
 public:
  ScopedWinHttpHandle() = default;
  explicit ScopedWinHttpHandle(HINTERNET handle) : handle_(handle) {}
  ~ScopedWinHttpHandle() {
    if (handle_) {
      WinHttpCloseHandle(handle_);
    }
  }

  ScopedWinHttpHandle(const ScopedWinHttpHandle&) = delete;
  ScopedWinHttpHandle& operator=(const ScopedWinHttpHandle&) = delete;

  ScopedWinHttpHandle(ScopedWinHttpHandle&& other) noexcept
      : handle_(other.handle_) {
    other.handle_ = nullptr;
  }
  ScopedWinHttpHandle& operator=(ScopedWinHttpHandle&&) = delete;

  HINTERNET get() const { return handle_; }
  HINTERNET release() {
    HINTERNET handle = handle_;
    handle_ = nullptr;
    return handle;
  }
  explicit operator bool() const { return handle_ != nullptr; }

 private:
  HINTERNET handle_ = nullptr;
};

// Open a WinHTTP session with automatic proxy support.
// Positive overrides bound launch-sensitive requests.
ScopedWinHttpHandle OpenSession(int connect_timeout_ms = 0,
                                int receive_timeout_ms = 0,
                                bool async = false) {
  ScopedWinHttpHandle session(WinHttpOpen(
      kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, async ? WINHTTP_FLAG_ASYNC : 0));

  if (session) {
    int effective_receive_timeout =
        receive_timeout_ms > 0 ? receive_timeout_ms : kReceiveTimeoutMs;
    int effective_send_timeout =
        receive_timeout_ms > 0 ? receive_timeout_ms : kSendTimeoutMs;
    int effective_connect_timeout =
        connect_timeout_ms > 0 ? connect_timeout_ms : kConnectTimeoutMs;
    int resolve_timeout = connect_timeout_ms > 0 ? connect_timeout_ms : 0;
    WinHttpSetTimeouts(session.get(), resolve_timeout,
                       effective_connect_timeout, effective_send_timeout,
                       effective_receive_timeout);

    // WinHttpSetTimeouts' 4th parameter is a per-read timeout for body data.
    // WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT controls how long
    // WinHttpReceiveResponse waits for the initial response headers.
    DWORD response_timeout = static_cast<DWORD>(effective_receive_timeout);
    WinHttpSetOption(session.get(), WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT,
                     &response_timeout, sizeof(response_timeout));

    // Explicitly block HTTPS-to-HTTP redirect downgrades. Modern Windows
    // defaults to this policy, but setting it explicitly provides
    // defense-in-depth against platform version differences.
    DWORD redirect_policy =
        WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(session.get(), WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirect_policy, sizeof(redirect_policy));
  }
  return session;
}

// Connect to a host and create a request handle.
ScopedWinHttpHandle CreateRequest(HINTERNET session,
                                  const internal::UrlComponents& url,
                                  const wchar_t* verb,
                                  bool ignore_certificate_errors_for_testing) {
  ScopedWinHttpHandle connection(
      WinHttpConnect(session, url.host.c_str(), url.port, 0));
  if (!connection) {
    return ScopedWinHttpHandle();
  }

  DWORD flags = url.is_https ? WINHTTP_FLAG_SECURE : 0;
  ScopedWinHttpHandle request(WinHttpOpenRequest(
      connection.get(), verb, url.path.c_str(), nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
  if (!request) {
    return ScopedWinHttpHandle();
  }

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (url.is_https && ignore_certificate_errors_for_testing) {
    DWORD security_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                           SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                           SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                           SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_SECURITY_FLAGS,
                          &security_flags, sizeof(security_flags))) {
      return ScopedWinHttpHandle();
    }
  }
#endif

  return request;
}

// Get the HTTP status code from a response.
DWORD GetHttpStatusCode(HINTERNET request) {
  DWORD status_code = 0;
  DWORD size = sizeof(status_code);
  if (!WinHttpQueryHeaders(
          request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size,
          WINHTTP_NO_HEADER_INDEX)) {
    return 0;
  }
  return status_code;
}

// Get the Content-Length from response headers (-1 if not available).
int64_t GetContentLength(HINTERNET request) {
  wchar_t buf[32] = {};
  DWORD size = sizeof(buf);
  if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
                           WINHTTP_HEADER_NAME_BY_INDEX, buf, &size,
                           WINHTTP_NO_HEADER_INDEX)) {
    return -1;
  }
  int64_t length = 0;
  if (!base::StringToInt64(base::WideToUTF8(buf), &length)) {
    return -1;
  }
  return length;
}

bool GetFinalResponseUrl(HINTERNET request, std::string* url) {
  DWORD size = 0;
  if (WinHttpQueryOption(request, WINHTTP_OPTION_URL, nullptr, &size) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER || size < sizeof(wchar_t)) {
    return false;
  }
  std::vector<wchar_t> buffer(size / sizeof(wchar_t));
  if (!WinHttpQueryOption(request, WINHTTP_OPTION_URL, buffer.data(), &size)) {
    return false;
  }
  const size_t length = size / sizeof(wchar_t);
  url->assign(base::WideToUTF8(std::wstring_view(
      buffer.data(),
      length > 0 && buffer[length - 1] == L'\0' ? length - 1 : length)));
  return !url->empty();
}

bool GetRawResponseHeaders(HINTERNET request, std::string* headers) {
  DWORD size = 0;
  if (WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                          WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size,
                          WINHTTP_NO_HEADER_INDEX) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER || size < sizeof(wchar_t)) {
    return false;
  }
  std::vector<wchar_t> buffer(size / sizeof(wchar_t));
  if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                           WINHTTP_HEADER_NAME_BY_INDEX, buffer.data(), &size,
                           WINHTTP_NO_HEADER_INDEX)) {
    return false;
  }
  const size_t length = size / sizeof(wchar_t);
  *headers = base::WideToUTF8(std::wstring_view(
      buffer.data(),
      length > 0 && buffer[length - 1] == L'\0' ? length - 1 : length));
  return true;
}

enum class HeaderResult {
  kMissing,
  kSingle,
  kDuplicate,
  kError,
};

HeaderResult GetUniqueResponseHeader(HINTERNET request,
                                     std::string_view name,
                                     std::string* value) {
  std::string headers;
  if (!GetRawResponseHeaders(request, &headers)) {
    return HeaderResult::kError;
  }
  const std::string prefix = std::string(name) + ":";
  size_t count = 0;
  for (const auto line : base::SplitStringPiece(
           headers, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (!base::StartsWith(line, prefix, base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    ++count;
    *value = std::string(base::TrimString(line.substr(prefix.size()),
                                          base::kWhitespaceASCII,
                                          base::TrimPositions::TRIM_ALL));
  }
  if (count == 0) {
    return HeaderResult::kMissing;
  }
  return count == 1 ? HeaderResult::kSingle : HeaderResult::kDuplicate;
}

bool ParseStrictUint64(std::string_view value, uint64_t* output) {
  return !value.empty() &&
         value.find_first_not_of("0123456789") == std::string_view::npos &&
         base::StringToUint64(value, output);
}

std::string OriginDigest(std::string_view origin) {
  const std::array<uint8_t, crypto::kSHA256Length> hash =
      crypto::SHA256Hash(base::as_byte_span(origin));
  return base::ToLowerASCII(base::HexEncode(base::span(hash).first<16>()));
}

bool GetArchiveHashStem(const base::FilePath& destination,
                        std::string* hash_stem) {
  constexpr std::string_view kArchiveSuffix = ".tar.xz";
  const std::string filename = destination.BaseName().AsUTF8Unsafe();
  if (!base::EndsWith(filename, kArchiveSuffix, base::CompareCase::SENSITIVE)) {
    return false;
  }
  *hash_stem = filename.substr(0, filename.size() - kArchiveSuffix.size());
  return (hash_stem->size() == 40 || hash_stem->size() == 64) &&
         hash_stem->find_first_not_of("0123456789abcdefABCDEF") ==
             std::string::npos;
}

base::FilePath BuildPartialPath(const base::FilePath& destination,
                                std::string_view configured_origin,
                                std::string_view final_origin) {
  std::string hash_stem;
  if (!GetArchiveHashStem(destination, &hash_stem)) {
    return base::FilePath();
  }
  return destination.DirName().AppendASCII(
      hash_stem + "." + OriginDigest(configured_origin) + "." +
      OriginDigest(final_origin) + ".tar.xz.partial");
}

struct PartialState {
  base::FilePath path;
  std::string final_digest;
  uint64_t size = 0;
};

bool ParsePartialName(const base::FilePath& destination,
                      const base::FilePath& path,
                      std::string* configured_digest,
                      std::string* final_digest) {
  std::string hash_stem;
  if (!GetArchiveHashStem(destination, &hash_stem) ||
      path.DirName() != destination.DirName()) {
    return false;
  }
  const std::string filename = path.BaseName().AsUTF8Unsafe();
  const std::string prefix = hash_stem + ".";
  constexpr std::string_view kSuffix = ".tar.xz.partial";
  if (!base::StartsWith(filename, prefix, base::CompareCase::SENSITIVE) ||
      !base::EndsWith(filename, kSuffix, base::CompareCase::SENSITIVE)) {
    return false;
  }
  const std::string_view middle(
      filename.data() + prefix.size(),
      filename.size() - prefix.size() - kSuffix.size());
  if (middle.size() != 65 || middle[32] != '.') {
    return false;
  }
  const auto is_digest = [](std::string_view value) {
    return value.size() == 32 && value.find_first_not_of("0123456789abcdef") ==
                                     std::string_view::npos;
  };
  const std::string_view configured = middle.substr(0, 32);
  const std::string_view final = middle.substr(33);
  if (!is_digest(configured) || !is_digest(final)) {
    return false;
  }
  *configured_digest = std::string(configured);
  *final_digest = std::string(final);
  return true;
}

bool DeletePartial(const base::FilePath& path) {
  if (!VerifySafeFilePath(path)) {
    return false;
  }
  return !base::PathExists(path) || base::DeleteFile(path);
}

bool EnumeratePartialVariants(const base::FilePath& destination,
                              std::vector<base::FilePath>* paths) {
  std::string hash_stem;
  if (!GetArchiveHashStem(destination, &hash_stem)) {
    return false;
  }
  base::FileEnumerator enumerator(
      destination.DirName(), false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      base::FilePath::FromASCII(hash_stem + ".*.tar.xz.partial").value());
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    std::string configured_digest;
    std::string final_digest;
    if (ParsePartialName(destination, path, &configured_digest,
                         &final_digest)) {
      paths->push_back(path);
    }
  }
  return true;
}

bool DeleteAllPartialVariants(const base::FilePath& destination) {
  std::vector<base::FilePath> paths;
  if (!EnumeratePartialVariants(destination, &paths)) {
    return false;
  }
  for (const auto& path : paths) {
    if (!DeletePartial(path)) {
      return false;
    }
  }
  return true;
}

DownloadError PreparePartialForConfiguredOrigin(
    const base::FilePath& destination,
    std::string_view configured_digest,
    uint64_t max_size,
    std::optional<PartialState>* state) {
  std::vector<base::FilePath> paths;
  if (!EnumeratePartialVariants(destination, &paths)) {
    return DownloadError::kFileWriteError;
  }

  std::vector<PartialState> matching;
  for (const auto& path : paths) {
    std::string path_configured_digest;
    PartialState candidate;
    if (!ParsePartialName(destination, path, &path_configured_digest,
                          &candidate.final_digest)) {
      continue;
    }
    if (path_configured_digest != configured_digest) {
      if (!DeletePartial(path)) {
        return DownloadError::kFileWriteError;
      }
      continue;
    }
    if (!VerifySafeFilePath(path)) {
      return DownloadError::kFileWriteError;
    }
    if (!base::PathExists(path)) {
      continue;
    }
    base::File::Info info;
    if (!base::GetFileInfo(path, &info) || info.is_directory || info.size < 0) {
      return DownloadError::kFileWriteError;
    }
    candidate.path = path;
    candidate.size = static_cast<uint64_t>(info.size);
    matching.push_back(std::move(candidate));
  }

  if (matching.size() > 1) {
    for (const auto& candidate : matching) {
      if (!DeletePartial(candidate.path)) {
        return DownloadError::kFileWriteError;
      }
    }
    return DownloadError::kSuccess;
  }
  if (matching.empty()) {
    return DownloadError::kSuccess;
  }
  if (matching[0].size == 0 || (max_size > 0 && matching[0].size > max_size)) {
    return DeletePartial(matching[0].path) ? DownloadError::kSuccess
                                           : DownloadError::kFileWriteError;
  }
  *state = std::move(matching[0]);
  return DownloadError::kSuccess;
}

bool GetStrictContentLength(HINTERNET request,
                            std::optional<uint64_t>* content_length) {
  std::string value;
  const HeaderResult result =
      GetUniqueResponseHeader(request, "Content-Length", &value);
  if (result == HeaderResult::kMissing) {
    content_length->reset();
    return true;
  }
  uint64_t parsed = 0;
  if (result != HeaderResult::kSingle || !ParseStrictUint64(value, &parsed)) {
    return false;
  }
  *content_length = parsed;
  return true;
}

// Check if a URL's scheme is allowed for network requests.
// In production, only HTTPS is permitted. In non-official builds,
// HTTP can be allowed for testing with local test servers.
bool IsAllowedUrl(const std::string& url, bool allow_http_for_testing) {
  if (IsValidDownloadUrl(url)) {
    return true;
  }
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (allow_http_for_testing &&
      base::StartsWith(url, "http://", base::CompareCase::INSENSITIVE_ASCII)) {
    return true;
  }
#endif
  return false;
}

// Accumulator for download response data. Allows DownloadFile and
// DownloadToString to share the HTTP request/read loop.
class DownloadSink {
 public:
  virtual ~DownloadSink() = default;
  // Called once before the read loop with the Content-Length (0 if unknown).
  virtual void Reserve(uint64_t expected_size) {}
  virtual bool Write(const uint8_t* data, size_t size) = 0;
  virtual void OnComplete() {}
  virtual void OnError() {}
};

// Writes downloaded data to a file.
class FileSink : public DownloadSink {
 public:
  explicit FileSink(const base::FilePath& path,
                    uint64_t offset = 0,
                    bool truncate = false)
      : file_(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE) {
    if (!file_.IsValid()) {
      return;
    }
    if (truncate && !file_.SetLength(0)) {
      file_.Close();
      return;
    }
    if (offset > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ||
        (offset > 0 && file_.GetLength() != static_cast<int64_t>(offset)) ||
        file_.Seek(base::File::FROM_BEGIN, static_cast<int64_t>(offset)) !=
            static_cast<int64_t>(offset)) {
      file_.Close();
    }
  }

  bool IsValid() const { return file_.IsValid(); }

  bool Write(const uint8_t* data, size_t size) override {
    return file_.WriteAtCurrentPosAndCheck(
        base::span<const uint8_t>(data, size));
  }

  void OnComplete() override { file_.Close(); }
  void OnError() override { file_.Close(); }

 private:
  base::File file_;
};

DownloadError ReadResponseToFile(HINTERNET request,
                                 const DownloadOptions& options,
                                 FileSink* sink,
                                 uint64_t initial_size,
                                 uint64_t total_size,
                                 std::optional<uint64_t> expected_body_size) {
  sink->Reserve(total_size);
  std::vector<uint8_t> buffer(kReadBufferSize);
  uint64_t received = 0;
  while (true) {
    if (options.progress_callback &&
        !options.progress_callback.Run(initial_size + received, total_size)) {
      sink->OnError();
      return DownloadError::kCancelled;
    }

    DWORD bytes_available = 0;
    if (!WinHttpQueryDataAvailable(request, &bytes_available)) {
      sink->OnError();
      return DownloadError::kNetworkError;
    }
    if (bytes_available == 0) {
      break;
    }
    const DWORD to_read = static_cast<DWORD>(
        std::min(static_cast<size_t>(bytes_available), buffer.size()));
    DWORD bytes_read = 0;
    if (!WinHttpReadData(request, buffer.data(), to_read, &bytes_read)) {
      sink->OnError();
      return DownloadError::kNetworkError;
    }
    if (bytes_read == 0) {
      break;
    }
    if (received > std::numeric_limits<uint64_t>::max() - bytes_read ||
        initial_size >
            std::numeric_limits<uint64_t>::max() - received - bytes_read) {
      sink->OnError();
      return DownloadError::kFileTooLarge;
    }
    received += bytes_read;
    const uint64_t complete_size = initial_size + received;
    if ((expected_body_size && received > *expected_body_size) ||
        (options.max_download_size > 0 &&
         complete_size > options.max_download_size)) {
      sink->OnError();
      return expected_body_size && received > *expected_body_size
                 ? DownloadError::kProtocolError
                 : DownloadError::kFileTooLarge;
    }
    if (!sink->Write(buffer.data(), static_cast<size_t>(bytes_read))) {
      sink->OnError();
      return DownloadError::kFileWriteError;
    }
    if (options.progress_callback &&
        !options.progress_callback.Run(complete_size, total_size)) {
      sink->OnError();
      return DownloadError::kCancelled;
    }
  }
  if (expected_body_size && received != *expected_body_size) {
    sink->OnError();
    return DownloadError::kNetworkError;
  }
  sink->OnComplete();
  return DownloadError::kSuccess;
}

// Writes downloaded data to an in-memory string.
class StringSink : public DownloadSink {
 public:
  explicit StringSink(std::string* out) : out_(out) {}

  void Reserve(uint64_t expected_size) override {
    // Use Content-Length when available, otherwise pre-allocate 4KB which
    // covers most manifest/hash payloads without waste.
    constexpr size_t kDefaultReserve = 4096;
    out_->reserve(expected_size > 0 ? static_cast<size_t>(expected_size)
                                    : kDefaultReserve);
  }

  bool Write(const uint8_t* data, size_t size) override {
    out_->append(reinterpret_cast<const char*>(data), size);
    return true;
  }

 private:
  std::string* out_;
};

// Owns one deadline-bounded asynchronous GET. On timeout the initiating thread
// closes the request handle and detaches this object; the final HANDLE_CLOSING
// callback then destroys it after WinHTTP has stopped using the callback
// context and buffers.
class AsyncStringDownloadState {
 public:
  AsyncStringDownloadState(HINTERNET session,
                           HINTERNET request,
                           const DownloadOptions& options)
      : session_(session),
        request_(request),
        options_(options),
        buffer_(kReadBufferSize),
        completed_event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
        closing_event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}

  ~AsyncStringDownloadState() {
    if (request_) {
      WinHttpCloseHandle(request_);
    }
    if (session_) {
      WinHttpCloseHandle(session_);
    }
    if (completed_event_) {
      CloseHandle(completed_event_);
    }
    if (closing_event_) {
      CloseHandle(closing_event_);
    }
  }

  AsyncStringDownloadState(const AsyncStringDownloadState&) = delete;
  AsyncStringDownloadState& operator=(const AsyncStringDownloadState&) = delete;

  bool Initialize() {
    if (!completed_event_ || !closing_event_) {
      return false;
    }
    DWORD_PTR context = reinterpret_cast<DWORD_PTR>(this);
    if (!WinHttpSetOption(request_, WINHTTP_OPTION_CONTEXT_VALUE, &context,
                          sizeof(context))) {
      return false;
    }
    constexpr DWORD kCallbackFlags =
        WINHTTP_CALLBACK_FLAG_SENDREQUEST_COMPLETE |
        WINHTTP_CALLBACK_FLAG_HEADERS_AVAILABLE |
        WINHTTP_CALLBACK_FLAG_DATA_AVAILABLE |
        WINHTTP_CALLBACK_FLAG_READ_COMPLETE |
        WINHTTP_CALLBACK_FLAG_REQUEST_ERROR | WINHTTP_CALLBACK_FLAG_HANDLES;
    callback_registered_ =
        WinHttpSetStatusCallback(request_, &StatusCallback, kCallbackFlags,
                                 0) != WINHTTP_INVALID_STATUS_CALLBACK;
    return callback_registered_;
  }

  bool Start() {
    return WinHttpSendRequest(request_, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                              WINHTTP_NO_REQUEST_DATA, 0, 0,
                              reinterpret_cast<DWORD_PTR>(this)) != FALSE;
  }

  HANDLE completed_event() const { return completed_event_; }

  DownloadError result() const {
    int value = result_.load(std::memory_order_acquire);
    return value < 0 ? DownloadError::kNetworkError
                     : static_cast<DownloadError>(value);
  }

  std::string TakeContent() { return std::move(content_); }

  bool CloseAndWait() {
    HINTERNET request = request_;
    request_ = nullptr;
    if (!WinHttpCloseHandle(request)) {
      request_ = request;
      return false;
    }
    return WaitForSingleObject(closing_event_, INFINITE) == WAIT_OBJECT_0;
  }

  // This method may delete |this| from HANDLE_CLOSING before
  // WinHttpCloseHandle returns. Do not access members after that call.
  void CancelAndDeleteOnClose() {
    detached_.store(true, std::memory_order_release);
    HINTERNET request = request_;
    request_ = nullptr;
    WinHttpCloseHandle(request);
  }

 private:
  static void CALLBACK StatusCallback(HINTERNET request,
                                      DWORD_PTR context,
                                      DWORD status,
                                      void* status_info,
                                      DWORD status_info_length) {
    auto* state = reinterpret_cast<AsyncStringDownloadState*>(context);
    if (!state) {
      return;
    }
    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
      if (state->detached_.load(std::memory_order_acquire)) {
        delete state;
      } else {
        SetEvent(state->closing_event_);
      }
      return;
    }
    if (state->IsFinished()) {
      return;
    }

    switch (status) {
      case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
        if (!WinHttpReceiveResponse(request, nullptr)) {
          state->Finish(DownloadError::kNetworkError);
        }
        break;
      case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
        state->HandleHeaders(request);
        break;
      case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
        if (!status_info || status_info_length != sizeof(DWORD)) {
          state->Finish(DownloadError::kNetworkError);
          break;
        }
        state->HandleDataAvailable(request,
                                   *static_cast<const DWORD*>(status_info));
        break;
      case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
        state->HandleReadComplete(request, status_info, status_info_length);
        break;
      case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
        state->Finish(DownloadError::kNetworkError);
        break;
      default:
        break;
    }
  }

  bool IsFinished() const {
    return result_.load(std::memory_order_acquire) >= 0;
  }

  void Finish(DownloadError result) {
    int expected = -1;
    if (result_.compare_exchange_strong(expected, static_cast<int>(result),
                                        std::memory_order_acq_rel)) {
      SetEvent(completed_event_);
    }
  }

  void HandleHeaders(HINTERNET request) {
    DWORD status_code = GetHttpStatusCode(request);
    if (status_code != 200) {
      Finish(status_code >= 400 && status_code < 600
                 ? DownloadError::kHttpError
                 : DownloadError::kNetworkError);
      return;
    }

    int64_t content_length = GetContentLength(request);
    total_bytes_ =
        content_length > 0 ? static_cast<uint64_t>(content_length) : 0;
    if (options_.max_download_size > 0 &&
        total_bytes_ > options_.max_download_size) {
      Finish(DownloadError::kFileTooLarge);
      return;
    }
    if (total_bytes_ > 0) {
      content_.reserve(static_cast<size_t>(total_bytes_));
    }
    QueryData(request);
  }

  void QueryData(HINTERNET request) {
    if (!WinHttpQueryDataAvailable(request, nullptr)) {
      Finish(DownloadError::kNetworkError);
    }
  }

  void HandleDataAvailable(HINTERNET request, DWORD bytes_available) {
    if (bytes_available == 0) {
      Finish(total_bytes_ == 0 || bytes_downloaded_ == total_bytes_
                 ? DownloadError::kSuccess
                 : DownloadError::kNetworkError);
      return;
    }
    bytes_remaining_ = bytes_available;
    ReadData(request);
  }

  void ReadData(HINTERNET request) {
    DWORD to_read = static_cast<DWORD>(
        std::min(static_cast<size_t>(bytes_remaining_), buffer_.size()));
    if (!WinHttpReadData(request, buffer_.data(), to_read, nullptr)) {
      Finish(DownloadError::kNetworkError);
    }
  }

  void HandleReadComplete(HINTERNET request,
                          const void* data,
                          DWORD bytes_read) {
    if (bytes_read == 0) {
      Finish(total_bytes_ == 0 || bytes_downloaded_ == total_bytes_
                 ? DownloadError::kSuccess
                 : DownloadError::kNetworkError);
      return;
    }
    bytes_downloaded_ += bytes_read;
    if (options_.max_download_size > 0 &&
        bytes_downloaded_ > options_.max_download_size) {
      Finish(DownloadError::kFileTooLarge);
      return;
    }
    content_.append(static_cast<const char*>(data), bytes_read);
    if (bytes_read >= bytes_remaining_) {
      bytes_remaining_ = 0;
      QueryData(request);
    } else {
      bytes_remaining_ -= bytes_read;
      ReadData(request);
    }
  }

  HINTERNET session_ = nullptr;
  HINTERNET request_ = nullptr;
  DownloadOptions options_;
  std::vector<uint8_t> buffer_;
  std::string content_;
  uint64_t total_bytes_ = 0;
  uint64_t bytes_downloaded_ = 0;
  DWORD bytes_remaining_ = 0;
  HANDLE completed_event_ = nullptr;
  HANDLE closing_event_ = nullptr;
  std::atomic<int> result_{-1};
  std::atomic<bool> detached_{false};
  bool callback_registered_ = false;
};

DWORD GetRemainingWait(base::TimeTicks deadline) {
  base::TimeDelta remaining = deadline - base::TimeTicks::Now();
  if (!remaining.is_positive()) {
    return 0;
  }
  return static_cast<DWORD>(remaining.InMillisecondsRoundedUp());
}

DownloadError DownloadStringWithDeadline(const std::string& url,
                                         std::string* content,
                                         const DownloadOptions& options,
                                         base::TimeDelta overall_timeout) {
  base::TimeTicks deadline = base::TimeTicks::Now() + overall_timeout;
  if (!IsAllowedUrl(url, options.allow_http_for_testing)) {
    return DownloadError::kInvalidUrl;
  }

  internal::UrlComponents url_parts;
  if (!internal::ParseUrl(url, &url_parts)) {
    return DownloadError::kInvalidUrl;
  }

  ScopedWinHttpHandle session =
      OpenSession(options.connect_timeout_ms, options.receive_timeout_ms, true);
  if (!session) {
    return DownloadError::kNetworkError;
  }
  DWORD connect_retries = 1;
  if (!WinHttpSetOption(session.get(), WINHTTP_OPTION_CONNECT_RETRIES,
                        &connect_retries, sizeof(connect_retries))) {
    return DownloadError::kNetworkError;
  }

  ScopedWinHttpHandle request =
      CreateRequest(session.get(), url_parts, L"GET",
                    options.ignore_certificate_errors_for_testing);
  if (!request) {
    return DownloadError::kNetworkError;
  }

  auto state = std::make_unique<AsyncStringDownloadState>(
      session.release(), request.release(), options);
  if (!state->Initialize()) {
    return DownloadError::kNetworkError;
  }
  if (!state->Start()) {
    if (!state->CloseAndWait()) {
      state.release()->CancelAndDeleteOnClose();
    }
    return DownloadError::kNetworkError;
  }

  DWORD wait_result =
      WaitForSingleObject(state->completed_event(), GetRemainingWait(deadline));
  if (wait_result != WAIT_OBJECT_0) {
    state.release()->CancelAndDeleteOnClose();
    return DownloadError::kNetworkError;
  }

  if (!state->CloseAndWait()) {
    state.release()->CancelAndDeleteOnClose();
    return DownloadError::kNetworkError;
  }
  DownloadError result = state->result();
  if (result == DownloadError::kSuccess) {
    *content = state->TakeContent();
  }
  return result;
}

// Extract the filename (last path component) from a URL.
std::string FilenameFromUrl(const std::string& url) {
  size_t pos = url.rfind('/');
  if (pos == std::string::npos || pos + 1 >= url.size()) {
    return url;
  }
  return url.substr(pos + 1);
}

// Read from a local file into the sink, mimicking the network download path.
DownloadError ReadLocalFileToSink(const base::FilePath& local_download_path,
                                  const std::string& url,
                                  const DownloadOptions& options,
                                  DownloadSink* sink) {
  base::FilePath local_file =
      local_download_path.AppendASCII(FilenameFromUrl(url));
  base::File file(local_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return DownloadError::kNetworkError;
  }

  int64_t file_size = file.GetLength();
  uint64_t total_bytes = file_size > 0 ? static_cast<uint64_t>(file_size) : 0;

  if (options.max_download_size > 0 &&
      total_bytes > options.max_download_size) {
    return DownloadError::kFileTooLarge;
  }

  sink->Reserve(total_bytes);

  std::vector<uint8_t> buffer(kReadBufferSize);
  uint64_t bytes_read_total = 0;

  while (true) {
    std::optional<size_t> bytes_read =
        file.ReadAtCurrentPos(base::as_writable_byte_span(buffer));
    if (!bytes_read.has_value()) {
      sink->OnError();
      return DownloadError::kNetworkError;
    }
    if (*bytes_read == 0) {
      break;
    }

    bytes_read_total += *bytes_read;

    if (options.max_download_size > 0 &&
        bytes_read_total > options.max_download_size) {
      sink->OnError();
      return DownloadError::kFileTooLarge;
    }

    if (!sink->Write(buffer.data(), *bytes_read)) {
      sink->OnError();
      return DownloadError::kFileWriteError;
    }

    if (options.progress_callback) {
      if (!options.progress_callback.Run(bytes_read_total, total_bytes)) {
        sink->OnError();
        return DownloadError::kCancelled;
      }
    }
  }

  sink->OnComplete();
  return DownloadError::kSuccess;
}

// Shared HTTP download implementation. Streams response body into the sink.
DownloadError DownloadToSink(const std::string& url,
                             const DownloadOptions& options,
                             DownloadSink* sink) {
  if (!options.local_download_path.empty()) {
    return ReadLocalFileToSink(options.local_download_path, url, options, sink);
  }

  if (!IsAllowedUrl(url, options.allow_http_for_testing)) {
    return DownloadError::kInvalidUrl;
  }

  internal::UrlComponents url_parts;
  if (!internal::ParseUrl(url, &url_parts)) {
    return DownloadError::kInvalidUrl;
  }

  ScopedWinHttpHandle session =
      OpenSession(options.connect_timeout_ms, options.receive_timeout_ms);
  if (!session) {
    return DownloadError::kNetworkError;
  }

  ScopedWinHttpHandle request =
      CreateRequest(session.get(), url_parts, L"GET",
                    options.ignore_certificate_errors_for_testing);
  if (!request) {
    return DownloadError::kNetworkError;
  }

  if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    return DownloadError::kNetworkError;
  }

  if (!WinHttpReceiveResponse(request.get(), nullptr)) {
    return DownloadError::kNetworkError;
  }

  DWORD status_code = GetHttpStatusCode(request.get());
  if (options.response_status_code) {
    *options.response_status_code = static_cast<int>(status_code);
  }
  if (status_code != 200) {
    if (status_code >= 400 && status_code < 600) {
      return DownloadError::kHttpError;
    }
    return DownloadError::kNetworkError;
  }

  int64_t content_length = GetContentLength(request.get());
  uint64_t total_bytes =
      content_length > 0 ? static_cast<uint64_t>(content_length) : 0;

  if (options.max_download_size > 0 &&
      total_bytes > options.max_download_size) {
    return DownloadError::kFileTooLarge;
  }

  sink->Reserve(total_bytes);

  std::vector<uint8_t> buffer(kReadBufferSize);
  uint64_t bytes_downloaded = 0;

  while (true) {
    if (options.progress_callback &&
        !options.progress_callback.Run(bytes_downloaded, total_bytes)) {
      sink->OnError();
      return DownloadError::kCancelled;
    }

    DWORD bytes_available = 0;
    if (!WinHttpQueryDataAvailable(request.get(), &bytes_available)) {
      sink->OnError();
      return DownloadError::kNetworkError;
    }
    if (bytes_available == 0) {
      break;
    }

    DWORD to_read = static_cast<DWORD>(
        std::min(static_cast<size_t>(bytes_available), buffer.size()));
    DWORD bytes_read = 0;
    if (!WinHttpReadData(request.get(), buffer.data(), to_read, &bytes_read)) {
      sink->OnError();
      return DownloadError::kNetworkError;
    }
    if (bytes_read == 0) {
      break;
    }
    bytes_downloaded += bytes_read;

    if (options.max_download_size > 0 &&
        bytes_downloaded > options.max_download_size) {
      sink->OnError();
      return DownloadError::kFileTooLarge;
    }

    if (!sink->Write(buffer.data(), static_cast<size_t>(bytes_read))) {
      sink->OnError();
      return DownloadError::kFileWriteError;
    }

    if (options.progress_callback) {
      if (!options.progress_callback.Run(bytes_downloaded, total_bytes)) {
        sink->OnError();
        return DownloadError::kCancelled;
      }
    }
  }

  sink->OnComplete();
  return DownloadError::kSuccess;
}

DownloadError DownloadOneResumableResponse(const std::string& url,
                                           const base::FilePath& destination,
                                           std::string_view configured_origin,
                                           const DownloadOptions& options,
                                           std::optional<PartialState>* state) {
  internal::UrlComponents url_parts;
  if (!internal::ParseUrl(url, &url_parts)) {
    return DownloadError::kInvalidUrl;
  }
  ScopedWinHttpHandle session =
      OpenSession(options.connect_timeout_ms, options.receive_timeout_ms);
  if (!session) {
    return DownloadError::kNetworkError;
  }
  ScopedWinHttpHandle request =
      CreateRequest(session.get(), url_parts, L"GET",
                    options.ignore_certificate_errors_for_testing);
  if (!request) {
    return DownloadError::kNetworkError;
  }

  const uint64_t requested_offset = *state ? (*state)->size : 0;
  if (requested_offset > 0) {
    const std::wstring range_header =
        L"Range: bytes=" + base::NumberToWString(requested_offset) + L"-";
    if (!WinHttpAddRequestHeaders(request.get(), range_header.c_str(),
                                  static_cast<DWORD>(range_header.size()),
                                  WINHTTP_ADDREQ_FLAG_ADD)) {
      return DownloadError::kNetworkError;
    }
  }
  if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(request.get(), nullptr)) {
    return DownloadError::kNetworkError;
  }

  std::string final_url;
  std::string final_origin;
  if (!GetFinalResponseUrl(request.get(), &final_url) ||
      !internal::NormalizeUrlOrigin(final_url, &final_origin)) {
    return DownloadError::kProtocolError;
  }
  const std::string final_digest = OriginDigest(final_origin);
  const bool final_origin_changed =
      *state && (*state)->final_digest != final_digest;
  if (final_origin_changed) {
    if (!DeletePartial((*state)->path)) {
      return DownloadError::kFileWriteError;
    }
    state->reset();
  }

  const DWORD status_code = GetHttpStatusCode(request.get());
  if (options.response_status_code) {
    *options.response_status_code = static_cast<int>(status_code);
  }
  if (status_code >= 400 && status_code < 600 && status_code != 416) {
    if (*state && !DeletePartial((*state)->path)) {
      return DownloadError::kFileWriteError;
    }
    state->reset();
    return DownloadError::kHttpError;
  }
  if (status_code != 200 && status_code != 206) {
    return status_code == 416 ? DownloadError::kProtocolError
                              : DownloadError::kNetworkError;
  }

  base::FilePath partial_path;
  uint64_t initial_size = 0;
  uint64_t total_size = 0;
  std::optional<uint64_t> expected_body_size;
  bool truncate = false;
  if (status_code == 206) {
    if (requested_offset == 0 || final_origin_changed || !*state) {
      return DownloadError::kProtocolError;
    }
    std::string content_range_value;
    if (GetUniqueResponseHeader(request.get(), "Content-Range",
                                &content_range_value) !=
        HeaderResult::kSingle) {
      return DownloadError::kProtocolError;
    }
    internal::ContentRange content_range;
    if (!internal::ParseContentRange(content_range_value, requested_offset,
                                     options.max_download_size,
                                     &content_range)) {
      return DownloadError::kProtocolError;
    }
    std::optional<uint64_t> content_length;
    if (!GetStrictContentLength(request.get(), &content_length)) {
      return DownloadError::kProtocolError;
    }
    const uint64_t range_length = content_range.end - content_range.start + 1;
    if (content_length && *content_length != range_length) {
      return DownloadError::kProtocolError;
    }
    partial_path = (*state)->path;
    initial_size = requested_offset;
    total_size = content_range.total;
    expected_body_size = range_length;
  } else {
    std::optional<uint64_t> content_length;
    if (!GetStrictContentLength(request.get(), &content_length)) {
      return DownloadError::kProtocolError;
    }
    if (content_length && options.max_download_size > 0 &&
        *content_length > options.max_download_size) {
      if (*state && !DeletePartial((*state)->path)) {
        return DownloadError::kFileWriteError;
      }
      state->reset();
      return DownloadError::kFileTooLarge;
    }
    partial_path =
        BuildPartialPath(destination, configured_origin, final_origin);
    if (partial_path.empty()) {
      return DownloadError::kFileWriteError;
    }
    total_size = content_length.value_or(0);
    expected_body_size = content_length;
    truncate = true;
  }

  if (!VerifySafeFilePath(partial_path)) {
    return DownloadError::kFileWriteError;
  }
  if (status_code == 206 && !base::PathExists(partial_path)) {
    return DownloadError::kFileWriteError;
  }
  FileSink sink(partial_path, initial_size, truncate);
  if (!sink.IsValid()) {
    return DownloadError::kFileWriteError;
  }
  const DownloadError body_error =
      ReadResponseToFile(request.get(), options, &sink, initial_size,
                         total_size, expected_body_size);
  if (body_error != DownloadError::kSuccess) {
    if (body_error == DownloadError::kProtocolError ||
        body_error == DownloadError::kFileTooLarge) {
      if (!DeletePartial(partial_path)) {
        return DownloadError::kFileWriteError;
      }
    }
    return body_error;
  }

  base::File::Info info;
  if (!base::GetFileInfo(partial_path, &info) || info.is_directory ||
      info.size < 0 ||
      (total_size > 0 && static_cast<uint64_t>(info.size) != total_size)) {
    if (!DeletePartial(partial_path)) {
      return DownloadError::kFileWriteError;
    }
    return DownloadError::kProtocolError;
  }
  if (!VerifyFileHash(partial_path, options.expected_sha256,
                      options.expected_sha1)) {
    if (!DeletePartial(partial_path)) {
      return DownloadError::kFileWriteError;
    }
    return DownloadError::kHashMismatch;
  }
  if (!VerifySafeFilePath(destination) ||
      (base::PathExists(destination) && !base::DeleteFile(destination)) ||
      !base::Move(partial_path, destination)) {
    return DownloadError::kFileWriteError;
  }
  return DownloadError::kSuccess;
}

}  // namespace

namespace internal {

bool ParseContentRange(std::string_view value,
                       uint64_t expected_start,
                       uint64_t max_size,
                       ContentRange* output) {
  constexpr std::string_view kPrefix = "bytes ";
  if (!output ||
      !base::StartsWith(value, kPrefix, base::CompareCase::SENSITIVE) ||
      value.find(',') != std::string_view::npos) {
    return false;
  }
  value.remove_prefix(kPrefix.size());
  const size_t dash = value.find('-');
  const size_t slash = value.find('/');
  if (dash == std::string_view::npos || slash == std::string_view::npos ||
      dash == 0 || slash <= dash + 1 || slash + 1 >= value.size() ||
      value.find('-', dash + 1) != std::string_view::npos ||
      value.find('/', slash + 1) != std::string_view::npos) {
    return false;
  }
  ContentRange parsed;
  if (!ParseStrictUint64(value.substr(0, dash), &parsed.start) ||
      !ParseStrictUint64(value.substr(dash + 1, slash - dash - 1),
                         &parsed.end) ||
      !ParseStrictUint64(value.substr(slash + 1), &parsed.total) ||
      parsed.start != expected_start || parsed.end < parsed.start ||
      parsed.total == 0 || parsed.end >= parsed.total ||
      (max_size > 0 && parsed.total > max_size)) {
    return false;
  }
  *output = parsed;
  return true;
}

bool NormalizeUrlOrigin(const std::string& url, std::string* origin) {
  if (!origin) {
    return false;
  }
  std::string scheme;
  if (base::StartsWith(url, "https://", base::CompareCase::INSENSITIVE_ASCII)) {
    scheme = "https";
  } else if (base::StartsWith(url, "http://",
                              base::CompareCase::INSENSITIVE_ASCII)) {
    scheme = "http";
  } else {
    return false;
  }
  UrlComponents parts;
  if (!ParseUrl(url, &parts) || parts.host.empty() || parts.port == 0 ||
      parts.is_https != (scheme == "https")) {
    return false;
  }
  *origin = scheme + "://" + base::ToLowerASCII(base::WideToUTF8(parts.host)) +
            ":" + base::NumberToString(parts.port);
  return true;
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
base::FilePath GetDownloadPartialPathForTesting(
    const base::FilePath& destination,
    const std::string& configured_url,
    const std::string& final_url) {
  std::string configured_origin;
  std::string final_origin;
  if (!NormalizeUrlOrigin(configured_url, &configured_origin) ||
      !NormalizeUrlOrigin(final_url, &final_origin)) {
    return base::FilePath();
  }
  return BuildPartialPath(destination, configured_origin, final_origin);
}
#endif

bool ParseUrl(const std::string& url, UrlComponents* out) {
  std::wstring url_wide = base::UTF8ToWide(url);

  URL_COMPONENTS components = {};
  components.dwStructSize = sizeof(components);

  // Request host and path buffers.
  wchar_t host_buf[256] = {};
  wchar_t path_buf[2048] = {};
  components.lpszHostName = host_buf;
  components.dwHostNameLength = std::size(host_buf);
  components.lpszUrlPath = path_buf;
  components.dwUrlPathLength = std::size(path_buf);

  if (!WinHttpCrackUrl(url_wide.c_str(), static_cast<DWORD>(url_wide.size()), 0,
                       &components)) {
    return false;
  }

  out->host =
      std::wstring(components.lpszHostName, components.dwHostNameLength);
  out->path = std::wstring(components.lpszUrlPath, components.dwUrlPathLength);
  out->port = components.nPort;
  out->is_https = (components.nScheme == INTERNET_SCHEME_HTTPS);
  return true;
}

std::wstring FormatHttpDate(const base::Time& time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);

  static constexpr const wchar_t* kDayNames[] = {L"Sun", L"Mon", L"Tue", L"Wed",
                                                 L"Thu", L"Fri", L"Sat"};
  static constexpr const wchar_t* kMonthNames[] = {
      L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
      L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};

  wchar_t buf[64];
  _snwprintf_s(buf, std::size(buf), _TRUNCATE,
               L"%s, %02d %s %04d %02d:%02d:%02d GMT",
               kDayNames[exploded.day_of_week], exploded.day_of_month,
               kMonthNames[exploded.month - 1], exploded.year, exploded.hour,
               exploded.minute, exploded.second);
  return buf;
}

HeadResult HeadRequest(const std::string& url,
                       const base::Time& cache_time,
                       const DownloadOptions& options) {
  if (!options.local_download_path.empty()) {
    return HeadResult::kModified;
  }
  if (!IsAllowedUrl(url, options.allow_http_for_testing)) {
    return HeadResult::kNetworkError;
  }

  UrlComponents url_parts;
  if (!ParseUrl(url, &url_parts)) {
    return HeadResult::kNetworkError;
  }

  ScopedWinHttpHandle session =
      OpenSession(options.connect_timeout_ms, options.receive_timeout_ms);
  if (!session) {
    return HeadResult::kNetworkError;
  }

  ScopedWinHttpHandle request =
      CreateRequest(session.get(), url_parts, L"HEAD",
                    options.ignore_certificate_errors_for_testing);
  if (!request) {
    return HeadResult::kNetworkError;
  }

  // Add If-Modified-Since header.
  std::wstring header = L"If-Modified-Since: " + FormatHttpDate(cache_time);
  if (!WinHttpAddRequestHeaders(request.get(), header.c_str(),
                                static_cast<DWORD>(header.size()),
                                WINHTTP_ADDREQ_FLAG_ADD)) {
    return HeadResult::kNetworkError;
  }

  if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    return HeadResult::kNetworkError;
  }

  if (!WinHttpReceiveResponse(request.get(), nullptr)) {
    return HeadResult::kNetworkError;
  }

  DWORD status_code = GetHttpStatusCode(request.get());
  if (status_code == 304) {
    return HeadResult::kNotModified;
  }
  if (status_code == 200) {
    return HeadResult::kModified;
  }

  // Any other status (4xx, 5xx, etc.) treated as network error for fallback.
  return HeadResult::kNetworkError;
}

}  // namespace internal

bool IsValidDownloadUrl(const std::string& url) {
  // Must start with https://
  return base::StartsWith(url, "https://",
                          base::CompareCase::INSENSITIVE_ASCII);
}

std::string ComputeFileSha256(const base::FilePath& path) {
  // Use streaming hash computation to avoid loading entire file into memory.
  // This prevents memory exhaustion with large files.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::string();
  }

  std::unique_ptr<crypto::SecureHash> hasher =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  constexpr size_t kBufferSize = 64 * 1024;  // 64 KB chunks
  std::vector<uint8_t> buffer(kBufferSize);

  while (true) {
    std::optional<size_t> bytes_read = file.ReadAtCurrentPos(buffer);
    if (!bytes_read) {
      return std::string();  // Read error
    }
    if (*bytes_read == 0) {
      break;  // EOF
    }
    hasher->Update(base::span(buffer).first(*bytes_read));
  }

  std::array<uint8_t, crypto::kSHA256Length> hash;
  hasher->Finish(base::span<uint8_t>(hash));

  return base::ToLowerASCII(base::HexEncode(hash));
}

std::string ComputeFileSha1(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::string();
  }

  std::array<uint8_t, crypto::hash::kSha1Size> hash;
  if (!crypto::hash::HashFile(crypto::hash::kSha1, &file, hash)) {
    return std::string();
  }

  return base::ToLowerASCII(base::HexEncode(hash));
}

bool VerifyFileHash(const base::FilePath& path,
                    const std::string& expected_sha256,
                    const std::string& expected_sha1) {
  // Check SHA256 first if provided
  if (!expected_sha256.empty()) {
    std::string actual = ComputeFileSha256(path);
    if (base::EqualsCaseInsensitiveASCII(actual, expected_sha256)) {
      return true;
    }
    // SHA256 was provided but didn't match
    return false;
  }

  // Check SHA1 if provided
  if (!expected_sha1.empty()) {
    std::string actual = ComputeFileSha1(path);
    return base::EqualsCaseInsensitiveASCII(actual, expected_sha1);
  }

  // No hash provided - consider it valid
  return true;
}

DownloadError DownloadFile(const std::string& url,
                           const base::FilePath& dest_path,
                           const DownloadOptions& options) {
  if (options.response_status_code) {
    *options.response_status_code = 0;
  }
  if (options.enable_resume) {
    if (!IsAllowedUrl(url, options.allow_http_for_testing)) {
      return DownloadError::kInvalidUrl;
    }
    const base::FilePath parent = dest_path.DirName();
    if (!VerifySafeDirectoryPath(parent) || !base::CreateDirectory(parent)) {
      return DownloadError::kFileWriteError;
    }
    if (!options.local_download_path.empty()) {
      if (!DeleteAllPartialVariants(dest_path)) {
        return DownloadError::kFileWriteError;
      }
    } else {
      std::string configured_origin;
      if (!internal::NormalizeUrlOrigin(url, &configured_origin)) {
        return DownloadError::kInvalidUrl;
      }
      const std::string configured_digest = OriginDigest(configured_origin);
      std::optional<PartialState> state;
      DownloadError prepare_error = PreparePartialForConfiguredOrigin(
          dest_path, configured_digest, options.max_download_size, &state);
      if (prepare_error != DownloadError::kSuccess) {
        return prepare_error;
      }
      CleanDownloadRetryBudget local_retry_budget;
      CleanDownloadRetryBudget* retry_budget = options.clean_retry_budget
                                                   ? options.clean_retry_budget
                                                   : &local_retry_budget;
      while (true) {
        DownloadError result = DownloadOneResumableResponse(
            url, dest_path, configured_origin, options, &state);
        if (result != DownloadError::kProtocolError &&
            result != DownloadError::kHashMismatch) {
          return result;
        }
        if (!DeleteAllPartialVariants(dest_path)) {
          return DownloadError::kFileWriteError;
        }
        state.reset();
        if (!retry_budget->TryConsume()) {
          return result;
        }
      }
    }
  }

  // Create parent directory if needed
  base::FilePath parent = dest_path.DirName();
  if (!base::CreateDirectory(parent)) {
    return DownloadError::kFileWriteError;
  }

  // Download to a temp file first, then rename on success.
  base::FilePath temp_path;
  if (!base::CreateTemporaryFileInDir(parent, &temp_path)) {
    return DownloadError::kFileWriteError;
  }
  ScopedFileDeleter temp_deleter(temp_path);

  FileSink sink(temp_path);
  if (!sink.IsValid()) {
    return DownloadError::kFileWriteError;
  }

  DownloadError error = DownloadToSink(url, options, &sink);
  if (error != DownloadError::kSuccess) {
    return error;
  }

  // Verify hash if provided
  if (!VerifyFileHash(temp_path, options.expected_sha256,
                      options.expected_sha1)) {
    return DownloadError::kHashMismatch;
  }

  // Move temp file to destination
  if (!base::Move(temp_path, dest_path)) {
    return DownloadError::kFileWriteError;
  }

  // File no longer exists at the temp location, so nothing to delete.
  temp_deleter.Release();
  return DownloadError::kSuccess;
}

DownloadError DiscardDownloadPartials(const base::FilePath& dest_path) {
  return DeleteAllPartialVariants(dest_path) ? DownloadError::kSuccess
                                             : DownloadError::kFileWriteError;
}

DownloadError DownloadToString(const std::string& url,
                               std::string* content,
                               const DownloadOptions& options) {
  if (!content) {
    return DownloadError::kInvalidUrl;
  }

  // Apply a tighter size limit for in-memory downloads.
  DownloadOptions effective = options;
  if (effective.max_download_size == 0 ||
      effective.max_download_size > kDefaultMaxStringDownloadSize) {
    effective.max_download_size = kDefaultMaxStringDownloadSize;
  }

  StringSink sink(content);
  return DownloadToSink(url, effective, &sink);
}

DownloadError DownloadToStringWithDeadline(const std::string& url,
                                           std::string* content,
                                           base::TimeDelta overall_timeout,
                                           const DownloadOptions& options) {
  if (!content || !overall_timeout.is_positive()) {
    return DownloadError::kInvalidUrl;
  }

  DownloadOptions effective = options;
  if (effective.max_download_size == 0 ||
      effective.max_download_size > kDefaultMaxStringDownloadSize) {
    effective.max_download_size = kDefaultMaxStringDownloadSize;
  }
  if (!effective.local_download_path.empty()) {
    return DownloadToString(url, content, effective);
  }
  if (effective.progress_callback) {
    return DownloadError::kCancelled;
  }
  return DownloadStringWithDeadline(url, content, effective, overall_timeout);
}

base::FilePath GetCacheDirectory(const base::FilePath& install_dir) {
  return install_dir.Append(L".cache");
}

base::FilePath GetCacheFilePath(const base::FilePath& cache_dir,
                                const std::string& url) {
  // Hash the URL to create a safe filename
  std::array<uint8_t, crypto::kSHA256Length> hash =
      crypto::SHA256Hash(base::as_byte_span(url));
  std::string hash_hex = base::ToLowerASCII(base::HexEncode(hash));

  return cache_dir.AppendASCII(hash_hex + ".cache");
}

bool IsCacheValidAtTime(const base::FilePath& cache_path,
                        base::Time* out_last_modified,
                        int max_age_seconds,
                        base::Time now) {
  if (!base::PathExists(cache_path)) {
    return false;
  }

  // Reject symlinks/junctions to prevent an attacker from redirecting
  // cache reads to attacker-controlled content.
  if (IsReparsePoint(cache_path)) {
    return false;
  }

  base::File::Info info;
  if (!base::GetFileInfo(cache_path, &info)) {
    return false;
  }

  base::TimeDelta age = now - info.last_modified;
  if (age.is_negative() || age.InSeconds() >= max_age_seconds) {
    return false;
  }

  if (out_last_modified) {
    *out_last_modified = info.last_modified;
  }
  return true;
}

bool IsCacheValid(const base::FilePath& cache_path,
                  base::Time* out_last_modified,
                  int max_age_seconds) {
  return IsCacheValidAtTime(cache_path, out_last_modified, max_age_seconds,
                            base::Time::Now());
}

DownloadError DownloadWithCache(const std::string& url,
                                const base::FilePath& cache_dir,
                                std::string* content,
                                const DownloadOptions& options,
                                bool force_check,
                                StaleCacheFallback stale_cache_fallback,
                                std::string_view cache_key,
                                CacheWriteBehavior cache_write_behavior,
                                DownloadContentSource* content_source) {
  if (!content) {
    return DownloadError::kInvalidUrl;
  }
  if (content_source) {
    *content_source = DownloadContentSource::kNetwork;
  }

  // Set up cache directory. If we can't, proceed without caching.
  bool use_cache = base::CreateDirectory(cache_dir);

  // If cache directory is a symlink/junction, remove the reparse point itself
  // (not its target) and recreate as a real directory. This neutralizes
  // junction attacks that redirect cache reads/writes.
  if (use_cache && IsReparsePoint(cache_dir)) {
    base::DeleteFile(cache_dir);
    use_cache = base::CreateDirectory(cache_dir);
  }

  base::FilePath cache_path;
  if (use_cache) {
    cache_path = GetCacheFilePath(
        cache_dir, cache_key.empty() ? url : std::string(cache_key));

    // If cache is valid and not forcing a check, try HEAD validation.
    base::Time cache_last_modified;
    if (!force_check && IsCacheValid(cache_path, &cache_last_modified)) {
      internal::HeadResult head =
          internal::HeadRequest(url, cache_last_modified, options);

      switch (head) {
        case internal::HeadResult::kNotModified:
        case internal::HeadResult::kNetworkError: {
          // Cache is current (304) or CDN is unreachable — try serving
          // from cache.
          IntegrityResult ir = ReadFileWithIntegrity(cache_path, content);
          if (ir == IntegrityResult::kSuccess ||
              ir == IntegrityResult::kSuccessNoFooter) {
            if (content_source) {
              *content_source = DownloadContentSource::kCache;
            }
            return DownloadError::kSuccess;
          }
          // Cache read failed. If CDN is also unreachable, no point
          // attempting a full download.
          if (head == internal::HeadResult::kNetworkError) {
            return DownloadError::kNetworkError;
          }
          break;
        }

        case internal::HeadResult::kModified:
          // CDN has newer content — download below.
          break;
      }
    }
  }

  // Single download path for all cases: cache missing/expired, force_check,
  // HEAD returned kModified, cache integrity failed, or caching disabled.
  DownloadError error = DownloadToString(url, content, options);
  if (error != DownloadError::kSuccess) {
    // Network failure — try to serve from cache as last resort.
    if (stale_cache_fallback == StaleCacheFallback::kAllow && use_cache &&
        !force_check && base::PathExists(cache_path) &&
        !IsReparsePoint(cache_path)) {
      IntegrityResult ir = ReadFileWithIntegrity(cache_path, content);
      if (ir == IntegrityResult::kSuccess ||
          ir == IntegrityResult::kSuccessNoFooter) {
        if (content_source) {
          *content_source = DownloadContentSource::kCache;
        }
        return DownloadError::kSuccess;
      }
    }
    return error;
  }

  // Save to cache with integrity footer (best effort).
  // Skip cache writes when using a local download path — local manifests
  // contain filenames that don't exist on CDN, so caching them would cause
  // download failures on the next run without /cef-download-path.
  if (cache_write_behavior == CacheWriteBehavior::kAutomatic && use_cache &&
      !IsReparsePoint(cache_path) && options.local_download_path.empty()) {
    WriteFileWithIntegrity(cache_path, *content);
  }

  return DownloadError::kSuccess;
}

DownloadError ReadDownloadCache(const base::FilePath& cache_dir,
                                std::string_view cache_key,
                                std::string* content) {
  if (!content || cache_key.empty() || !base::DirectoryExists(cache_dir) ||
      IsReparsePoint(cache_dir)) {
    return DownloadError::kFileWriteError;
  }
  const base::FilePath cache_path =
      GetCacheFilePath(cache_dir, std::string(cache_key));
  if (!base::PathExists(cache_path) || IsReparsePoint(cache_path)) {
    return DownloadError::kNetworkError;
  }
  const IntegrityResult result = ReadFileWithIntegrity(cache_path, content);
  return result == IntegrityResult::kSuccess ||
                 result == IntegrityResult::kSuccessNoFooter
             ? DownloadError::kSuccess
             : DownloadError::kNetworkError;
}

DownloadError WriteDownloadCache(const base::FilePath& cache_dir,
                                 std::string_view cache_key,
                                 std::string_view content) {
  if (cache_key.empty() || !base::CreateDirectory(cache_dir)) {
    return DownloadError::kFileWriteError;
  }
  if (IsReparsePoint(cache_dir)) {
    base::DeleteFile(cache_dir);
    if (!base::CreateDirectory(cache_dir)) {
      return DownloadError::kFileWriteError;
    }
  }
  const base::FilePath cache_path =
      GetCacheFilePath(cache_dir, std::string(cache_key));
  if (IsReparsePoint(cache_path) ||
      !WriteFileWithIntegrity(cache_path, std::string(content))) {
    return DownloadError::kFileWriteError;
  }
  return DownloadError::kSuccess;
}

bool DiscardDownloadCache(const base::FilePath& cache_dir,
                          std::string_view cache_key) {
  if (cache_key.empty() || !base::DirectoryExists(cache_dir) ||
      IsReparsePoint(cache_dir)) {
    return false;
  }
  const base::FilePath cache_path =
      GetCacheFilePath(cache_dir, std::string(cache_key));
  return !base::PathExists(cache_path) || base::DeleteFile(cache_path);
}

void PruneCacheDirectoryAtTime(const base::FilePath& cache_dir,
                               base::Time now) {
  if (!base::DirectoryExists(cache_dir)) {
    return;
  }

  // If cache directory is a junction/symlink, remove the reparse point and
  // recreate. Don't enumerate through it — that would touch the target.
  if (IsReparsePoint(cache_dir)) {
    base::DeleteFile(cache_dir);
    base::CreateDirectory(cache_dir);
    return;
  }

  // Prune expired manifest caches (*.cache).
  base::FileEnumerator cache_enum(cache_dir, false, base::FileEnumerator::FILES,
                                  L"*.cache");
  for (base::FilePath path = cache_enum.Next(); !path.empty();
       path = cache_enum.Next()) {
    if (!IsCacheValidAtTime(path, nullptr, kManifestCacheValiditySeconds,
                            now)) {
      base::DeleteFile(path);
    }
  }

  // Prune orphaned archive caches (*.tar.xz) with a longer expiry.
  // Archives are normally deleted after successful installation, but failed
  // installs that are never retried could leave them behind.
  base::FileEnumerator archive_enum(cache_dir, false,
                                    base::FileEnumerator::FILES, L"*.tar.xz");
  for (base::FilePath path = archive_enum.Next(); !path.empty();
       path = archive_enum.Next()) {
    if (!IsCacheValidAtTime(path, nullptr, kArchiveCacheValiditySeconds, now)) {
      base::DeleteFile(path);
    }
  }

  // Prune only deterministic origin-bound archive partials. Exact structural
  // matching avoids touching unrelated *.partial files in the cache.
  const auto is_archive_partial_name = [](const base::FilePath& path) {
    constexpr std::string_view kSuffix = ".tar.xz.partial";
    const std::string name = path.BaseName().AsUTF8Unsafe();
    if (!base::EndsWith(name, kSuffix, base::CompareCase::SENSITIVE)) {
      return false;
    }
    const size_t stem_size = name.size() - kSuffix.size();
    const size_t hash_size = stem_size == 40 + 1 + 32 + 1 + 32   ? 40
                             : stem_size == 64 + 1 + 32 + 1 + 32 ? 64
                                                                 : 0;
    if (hash_size == 0 || name[hash_size] != '.' ||
        name[hash_size + 33] != '.') {
      return false;
    }
    const auto is_lower_hex = [](std::string_view value) {
      return value.find_first_not_of("0123456789abcdef") ==
             std::string_view::npos;
    };
    return is_lower_hex(std::string_view(name).substr(0, hash_size)) &&
           is_lower_hex(std::string_view(name).substr(hash_size + 1, 32)) &&
           is_lower_hex(std::string_view(name).substr(hash_size + 34, 32));
  };
  base::FileEnumerator partial_enum(
      cache_dir, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      L"*.partial");
  for (base::FilePath path = partial_enum.Next(); !path.empty();
       path = partial_enum.Next()) {
    if (!is_archive_partial_name(path)) {
      continue;
    }
    if (IsReparsePoint(path)) {
      base::DeleteFile(path);
      continue;
    }
    base::File::Info info;
    if (!base::GetFileInfo(path, &info) || info.is_directory) {
      continue;
    }
    if (!IsCacheValidAtTime(path, nullptr, kArchiveCacheValiditySeconds, now)) {
      base::DeleteFile(path);
    }
  }
}

void PruneCacheDirectory(const base::FilePath& cache_dir) {
  PruneCacheDirectoryAtTime(cache_dir, base::Time::Now());
}

namespace internal {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
void PruneCacheDirectoryAtTimeForTesting(const base::FilePath& cache_dir,
                                         base::Time now) {
  PruneCacheDirectoryAtTime(cache_dir, now);
}
#endif

}  // namespace internal

const char* DownloadErrorToString(DownloadError error) {
  switch (error) {
    case DownloadError::kSuccess:
      return "Success";
    case DownloadError::kInvalidUrl:
      return "Invalid URL (must be HTTPS)";
    case DownloadError::kNetworkError:
      return "Network error";
    case DownloadError::kHttpError:
      return "HTTP error";
    case DownloadError::kFileWriteError:
      return "File write error";
    case DownloadError::kHashMismatch:
      return "Hash mismatch";
    case DownloadError::kCancelled:
      return "Cancelled";
    case DownloadError::kFileTooLarge:
      return "File exceeds maximum allowed size";
    case DownloadError::kProtocolError:
      return "Invalid HTTP range response";
  }
  return "Unknown error";
}

}  // namespace cef_installer
