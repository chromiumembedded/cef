// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_download.h"

#include <thread>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_integrity.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

// Test content
constexpr char kTestContent[] = "Hello, CEF Installer!";

class InstallerDownloadTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath CreateTestFile(const std::string& content) {
    base::FilePath path = temp_dir_.GetPath().AppendASCII("test_file.txt");
    EXPECT_TRUE(base::WriteFile(path, content));
    return path;
  }

  base::ScopedTempDir temp_dir_;
};

// ============================================================================
// Hash Computation Tests
// ============================================================================

TEST_F(InstallerDownloadTest, ComputeFileSha256) {
  base::FilePath path = CreateTestFile(kTestContent);

  std::string hash = ComputeFileSha256(path);

  EXPECT_FALSE(hash.empty());
  EXPECT_EQ(64u, hash.size());  // SHA256 = 32 bytes = 64 hex chars
}

TEST_F(InstallerDownloadTest, ComputeFileSha256Consistent) {
  base::FilePath path = CreateTestFile(kTestContent);

  std::string hash1 = ComputeFileSha256(path);
  std::string hash2 = ComputeFileSha256(path);

  EXPECT_EQ(hash1, hash2);
}

TEST_F(InstallerDownloadTest, ComputeFileSha256EmptyFile) {
  base::FilePath path = CreateTestFile("");

  std::string hash = ComputeFileSha256(path);

  EXPECT_FALSE(hash.empty());
  EXPECT_EQ(64u, hash.size());
  // SHA256 of empty string is known
  EXPECT_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            hash);
}

TEST_F(InstallerDownloadTest, ComputeFileSha256NonExistent) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("nonexistent.txt");

  std::string hash = ComputeFileSha256(path);

  EXPECT_TRUE(hash.empty());
}

TEST_F(InstallerDownloadTest, ComputeFileSha1) {
  base::FilePath path = CreateTestFile(kTestContent);

  std::string hash = ComputeFileSha1(path);

  EXPECT_FALSE(hash.empty());
  EXPECT_EQ(40u, hash.size());  // SHA1 = 20 bytes = 40 hex chars
}

TEST_F(InstallerDownloadTest, ComputeFileSha1Consistent) {
  base::FilePath path = CreateTestFile(kTestContent);

  std::string hash1 = ComputeFileSha1(path);
  std::string hash2 = ComputeFileSha1(path);

  EXPECT_EQ(hash1, hash2);
}

TEST_F(InstallerDownloadTest, ComputeFileSha1NonExistent) {
  base::FilePath path = temp_dir_.GetPath().AppendASCII("nonexistent.txt");

  std::string hash = ComputeFileSha1(path);

  EXPECT_TRUE(hash.empty());
}

TEST_F(InstallerDownloadTest, VerifyFileHashCorrect) {
  base::FilePath path = CreateTestFile(kTestContent);
  std::string actual_sha256 = ComputeFileSha256(path);

  EXPECT_TRUE(VerifyFileHash(path, actual_sha256, ""));
}

TEST_F(InstallerDownloadTest, VerifyFileHashWrong) {
  base::FilePath path = CreateTestFile(kTestContent);
  std::string wrong_hash =
      "0000000000000000000000000000000000000000000000000000000000000000";

  EXPECT_FALSE(VerifyFileHash(path, wrong_hash, ""));
}

TEST_F(InstallerDownloadTest, VerifyFileHashCaseInsensitive) {
  base::FilePath path = CreateTestFile(kTestContent);
  std::string actual = ComputeFileSha256(path);
  std::string upper = actual;
  for (char& c : upper) {
    c = base::ToUpperASCII(c);
  }

  EXPECT_TRUE(VerifyFileHash(path, upper, ""));
}

TEST_F(InstallerDownloadTest, VerifyFileHashNoHashProvided) {
  base::FilePath path = CreateTestFile(kTestContent);

  // No hash provided means valid
  EXPECT_TRUE(VerifyFileHash(path, "", ""));
}

TEST_F(InstallerDownloadTest, VerifyFileHashSha1Fallback) {
  base::FilePath path = CreateTestFile(kTestContent);
  std::string actual_sha1 = ComputeFileSha1(path);

  // Empty SHA256 but valid SHA1
  EXPECT_TRUE(VerifyFileHash(path, "", actual_sha1));
}

// ============================================================================
// URL Validation Tests
// ============================================================================

TEST_F(InstallerDownloadTest, IsValidDownloadUrlHttps) {
  EXPECT_TRUE(IsValidDownloadUrl("https://example.com/file.txt"));
  EXPECT_TRUE(IsValidDownloadUrl("HTTPS://example.com/file.txt"));
  EXPECT_TRUE(IsValidDownloadUrl("https://example.com:443/file.txt"));
}

TEST_F(InstallerDownloadTest, IsValidDownloadUrlRejectHttp) {
  EXPECT_FALSE(IsValidDownloadUrl("http://example.com/file.txt"));
  EXPECT_FALSE(IsValidDownloadUrl("HTTP://example.com/file.txt"));
}

TEST_F(InstallerDownloadTest, IsValidDownloadUrlRejectOther) {
  EXPECT_FALSE(IsValidDownloadUrl("ftp://example.com/file.txt"));
  EXPECT_FALSE(IsValidDownloadUrl("file:///C:/test.txt"));
  EXPECT_FALSE(IsValidDownloadUrl(""));
  EXPECT_FALSE(IsValidDownloadUrl("not a url"));
}

TEST_F(InstallerDownloadTest, DownloadFileRejectHttp) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("download.txt");

  DownloadError result = DownloadFile("http://example.com/file.txt", dest);

  EXPECT_EQ(DownloadError::kInvalidUrl, result);
}

TEST_F(InstallerDownloadTest, ParseContentRangeStrictly) {
  internal::ContentRange range;
  EXPECT_TRUE(internal::ParseContentRange("bytes 5-9/10", 5, 10, &range));
  EXPECT_EQ(5u, range.start);
  EXPECT_EQ(9u, range.end);
  EXPECT_EQ(10u, range.total);

  EXPECT_FALSE(internal::ParseContentRange("bytes 4-9/10", 5, 10, &range));
  EXPECT_FALSE(internal::ParseContentRange("bytes 5-4/10", 5, 10, &range));
  EXPECT_FALSE(internal::ParseContentRange("bytes 5-10/10", 5, 10, &range));
  EXPECT_FALSE(internal::ParseContentRange("bytes 5-9/*", 5, 10, &range));
  EXPECT_FALSE(internal::ParseContentRange("bytes 5-9/11", 5, 10, &range));
  EXPECT_FALSE(
      internal::ParseContentRange("bytes 5-9/10, 20-29/30", 5, 30, &range));
  EXPECT_FALSE(internal::ParseContentRange("bytes  5-9/10", 5, 10, &range));
  EXPECT_FALSE(internal::ParseContentRange("bytes 5-9/10 ", 5, 10, &range));
  EXPECT_TRUE(internal::ParseContentRange("bytes 0-0/1", 0, 1, &range));
  EXPECT_FALSE(internal::ParseContentRange(
      "bytes 18446744073709551616-18446744073709551617/"
      "18446744073709551618",
      0, 0, &range));
  EXPECT_FALSE(internal::ParseContentRange("bytes +5-9/10", 5, 10, &range));
  EXPECT_FALSE(internal::ParseContentRange("bytes -5-9/10", 5, 10, &range));
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(InstallerDownloadTest, OriginNormalizationAndPartialPathAreBounded) {
  std::string origin;
  EXPECT_TRUE(internal::NormalizeUrlOrigin(
      "HTTPS://Example.COM/path?secret=value", &origin));
  EXPECT_EQ("https://example.com:443", origin);

  const base::FilePath destination = temp_dir_.GetPath().AppendASCII(
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
      ".tar.xz");
  const base::FilePath first = internal::GetDownloadPartialPathForTesting(
      destination, "https://example.com/private/a",
      "https://user:password@redirect.example/archive?token=secret");
  const base::FilePath repeated = internal::GetDownloadPartialPathForTesting(
      destination, "https://example.com/other",
      "https://redirect.example/different");
  const base::FilePath changed = internal::GetDownloadPartialPathForTesting(
      destination, "https://other.example/a",
      "https://redirect.example/archive");
  EXPECT_FALSE(first.empty());
  EXPECT_EQ(first, repeated);
  EXPECT_NE(first, changed);
  EXPECT_EQ(destination.DirName(), first.DirName());
  EXPECT_EQ(std::string::npos,
            first.BaseName().AsUTF8Unsafe().find("example.com"));
  EXPECT_EQ(std::string::npos, first.BaseName().AsUTF8Unsafe().find("secret"));
}

// ============================================================================
// Cache Path Tests
// ============================================================================

#endif

TEST_F(InstallerDownloadTest, GetCacheFilePath) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  std::string url = "https://example.com/manifest.json";

  base::FilePath path1 = GetCacheFilePath(cache_dir, url);
  base::FilePath path2 = GetCacheFilePath(cache_dir, url);

  // Should be consistent
  EXPECT_EQ(path1, path2);

  // Should end with .cache
  EXPECT_EQ(L".cache", path1.Extension());

  // Should be in cache directory
  EXPECT_EQ(cache_dir, path1.DirName());
}

TEST_F(InstallerDownloadTest, GetCacheFilePathDifferentUrls) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");

  base::FilePath path1 = GetCacheFilePath(cache_dir, "https://a.com/1");
  base::FilePath path2 = GetCacheFilePath(cache_dir, "https://b.com/2");

  EXPECT_NE(path1, path2);
}

TEST_F(InstallerDownloadTest, GetCacheDirectory) {
  base::FilePath install_dir = temp_dir_.GetPath().Append(kCefSubdirectory);

  base::FilePath cache_dir = GetCacheDirectory(install_dir);

  EXPECT_EQ(install_dir.Append(L".cache"), cache_dir);
}

TEST_F(InstallerDownloadTest, IsCacheValidFresh) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  base::FilePath cache_file = cache_dir.AppendASCII("test.cache");

  // Create a fresh cache file
  ASSERT_TRUE(base::WriteFile(cache_file, "cached content"));

  EXPECT_TRUE(IsCacheValid(cache_file));
}

TEST_F(InstallerDownloadTest, IsCacheValidNonExistent) {
  base::FilePath cache_file =
      temp_dir_.GetPath().AppendASCII("nonexistent.cache");

  EXPECT_FALSE(IsCacheValid(cache_file));
}

TEST_F(InstallerDownloadTest, PruneCacheDirectory) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  // Create a fresh cache file
  base::FilePath fresh = cache_dir.AppendASCII("fresh.cache");
  ASSERT_TRUE(base::WriteFile(fresh, "fresh"));

  // Prune should not delete fresh files
  PruneCacheDirectory(cache_dir);

  EXPECT_TRUE(base::PathExists(fresh));
}

// ============================================================================
// Download Tests (using EmbeddedTestServer)
// ============================================================================

class InstallerDownloadServerTest : public testing::Test {
 protected:
  enum class ResumeMode {
    kRange,
    kIgnoreRange,
    kMalformedRange,
    kAlwaysRangeNotSatisfiable,
    kWrongFullBodyOnce,
    kAlwaysWrongFullBody,
    kMalformedThenWrongFullBody,
    kDuplicateContentRange,
    kMismatchedContentLength,
    kShortRangeBody,
    kOverlongRangeBody,
  };

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    slow_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            server_.get(), "/slow.txt");
    header_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            server_.get(), "/headers.txt");
    server_->RegisterRequestHandler(base::BindRepeating(
        &InstallerDownloadServerTest::HandleRequest, base::Unretained(this)));
    final_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    final_server_->RegisterRequestHandler(base::BindRepeating(
        &InstallerDownloadServerTest::HandleRequest, base::Unretained(this)));
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &InstallerDownloadServerTest::HandleRequest, base::Unretained(this)));
    https_final_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_final_server_->RegisterRequestHandler(base::BindRepeating(
        &InstallerDownloadServerTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(final_server_->Start());
    ASSERT_TRUE(https_final_server_->Start());
    ASSERT_TRUE(https_server_->Start());
    ASSERT_TRUE(server_->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.relative_url == "/test.txt") {
      if (request.method == net::test_server::METHOD_HEAD) {
        // HEAD request for CDN validation.
        auto it = request.headers.find("If-Modified-Since");
        if (it != request.headers.end() && !force_modified_) {
          // Content hasn't changed — return 304.
          response->set_code(net::HTTP_NOT_MODIFIED);
        } else {
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/plain");
        }
        return response;
      }
      response->set_code(net::HTTP_OK);
      response->set_content(kTestContent);
      response->set_content_type("text/plain");
    } else if (request.relative_url == "/changing.txt") {
      if (request.method == net::test_server::METHOD_HEAD) {
        // Always report content as modified (200 for HEAD).
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/plain");
        return response;
      }
      response->set_code(net::HTTP_OK);
      response->set_content(server_content_);
      response->set_content_type("text/plain");
    } else if (request.relative_url == "/redirect") {
      response->set_code(net::HTTP_MOVED_PERMANENTLY);
      response->AddCustomHeader("Location", "/test.txt");
    } else if (request.relative_url == "/no-content") {
      response->set_code(net::HTTP_NO_CONTENT);
    } else if (request.relative_url == "/notfound") {
      response->set_code(net::HTTP_NOT_FOUND);
    } else if (request.relative_url == "/large.txt") {
      // Return a larger file for progress testing
      std::string large_content(10000, 'X');
      response->set_code(net::HTTP_OK);
      response->set_content(large_content);
      response->set_content_type("text/plain");
    } else if (request.relative_url == "/oversized.bin") {
      // Return a file that exceeds size limits for security testing
      std::string oversized_content(2000, 'Z');  // 2 KB file
      response->set_code(net::HTTP_OK);
      response->set_content(oversized_content);
      response->set_content_type("application/octet-stream");
    } else if (request.relative_url == "/archive.tar.xz") {
      ++resume_request_count_;
      auto range = request.headers.find("Range");
      if (range == request.headers.end() ||
          resume_mode_ == ResumeMode::kIgnoreRange) {
        seen_ranges_.push_back("");
        response->set_code(net::HTTP_OK);
        response->set_content(
            (resume_mode_ == ResumeMode::kWrongFullBodyOnce &&
             resume_request_count_ == 1) ||
                    resume_mode_ == ResumeMode::kAlwaysWrongFullBody ||
                    (resume_mode_ == ResumeMode::kMalformedThenWrongFullBody &&
                     resume_request_count_ == 2)
                ? std::string(resume_content_.size(), 'x')
                : resume_content_);
        return response;
      }
      seen_ranges_.push_back(range->second);
      if (resume_mode_ == ResumeMode::kAlwaysRangeNotSatisfiable) {
        response->set_code(net::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE);
        return response;
      }
      uint64_t offset = 0;
      const std::string prefix = "bytes=";
      const std::string value = range->second;
      if (value.size() <= prefix.size() + 1 ||
          value.compare(0, prefix.size(), prefix) != 0 || value.back() != '-' ||
          !base::StringToUint64(
              value.substr(prefix.size(), value.size() - prefix.size() - 1),
              &offset) ||
          offset >= resume_content_.size()) {
        response->set_code(net::HTTP_BAD_REQUEST);
        return response;
      }
      response->set_code(net::HTTP_PARTIAL_CONTENT);
      std::string response_content = resume_content_.substr(offset);
      const uint64_t start =
          resume_mode_ == ResumeMode::kMalformedRange ||
                  resume_mode_ == ResumeMode::kMalformedThenWrongFullBody
              ? offset + 1
              : offset;
      const std::string content_range =
          "bytes " + base::NumberToString(start) + "-" +
          base::NumberToString(resume_content_.size() - 1) + "/" +
          base::NumberToString(resume_content_.size());
      if (resume_mode_ == ResumeMode::kMismatchedContentLength) {
        return std::make_unique<net::test_server::RawHttpResponse>(
            "HTTP/1.1 206 Partial Content\r\nContent-Range: " + content_range +
                "\r\nContent-Length: 1\r\nConnection: close",
            response_content);
      }
      if (resume_mode_ == ResumeMode::kShortRangeBody) {
        response_content.pop_back();
        return std::make_unique<net::test_server::RawHttpResponse>(
            "HTTP/1.1 206 Partial Content\r\nContent-Range: " + content_range +
                "\r\nConnection: close",
            response_content);
      }
      if (resume_mode_ == ResumeMode::kOverlongRangeBody) {
        response_content.push_back('x');
        return std::make_unique<net::test_server::RawHttpResponse>(
            "HTTP/1.1 206 Partial Content\r\nContent-Range: " + content_range +
                "\r\nConnection: close",
            response_content);
      }
      response->set_content(response_content);
      response->AddCustomHeader("Content-Range", content_range);
      if (resume_mode_ == ResumeMode::kDuplicateContentRange) {
        response->AddCustomHeader("Content-Range", content_range);
      }
    } else if (request.relative_url == "/redirect-archive") {
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader("Location", "/archive.tar.xz");
    } else if (request.relative_url == "/cross-origin-archive") {
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader(
          "Location", final_server_->GetURL("/archive.tar.xz").spec());
    } else if (request.relative_url == "/https-cross-origin-archive") {
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader(
          "Location", https_final_server_->GetURL("/archive.tar.xz").spec());
    } else if (request.relative_url == "/https-downgrade-archive") {
      response->set_code(net::HTTP_TEMPORARY_REDIRECT);
      response->AddCustomHeader(
          "Location", final_server_->GetURL("/archive.tar.xz").spec());
    } else {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    return response;
  }

  std::string GetTestUrl(const std::string& path) {
    return server_->GetURL(path).spec();
  }

  // Returns DownloadOptions with allow_http_for_testing enabled
  DownloadOptions TestOptions() {
    DownloadOptions options;
    options.allow_http_for_testing = true;
    options.ignore_certificate_errors_for_testing = true;
    return options;
  }

  DownloadOptions ResumeOptions() {
    DownloadOptions options = TestOptions();
    options.enable_resume = true;
    const base::FilePath source =
        temp_dir_.GetPath().AppendASCII("archive-source.bin");
    EXPECT_TRUE(base::WriteFile(source, resume_content_));
    options.expected_sha256 = ComputeFileSha256(source);
    return options;
  }

  base::FilePath ArchiveDestination(const DownloadOptions& options) {
    return temp_dir_.GetPath().AppendASCII(options.expected_sha256 + ".tar.xz");
  }

  // When true, HEAD requests to /test.txt return 200 instead of 304.
  bool force_modified_ = false;

  // Configurable content for /changing.txt endpoint.
  std::string server_content_ = "initial content";
  const std::string resume_content_ = "0123456789abcdefghijklmnopqrstuv";
  ResumeMode resume_mode_ = ResumeMode::kRange;
  int resume_request_count_ = 0;
  std::vector<std::string> seen_ranges_;

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::EmbeddedTestServer> server_;
  std::unique_ptr<net::EmbeddedTestServer> final_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_final_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> slow_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> header_response_;
};

TEST_F(InstallerDownloadServerTest, DownloadFileSuccess) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("downloaded.txt");

  DownloadError result =
      DownloadFile(GetTestUrl("/test.txt"), dest, TestOptions());

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_TRUE(base::PathExists(dest));

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(dest, &content));
  EXPECT_EQ(kTestContent, content);
}

TEST_F(InstallerDownloadServerTest, DownloadFileHashMismatch) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("downloaded.txt");
  DownloadOptions options = TestOptions();
  options.expected_sha256 =
      "0000000000000000000000000000000000000000000000000000000000000000";

  DownloadError result = DownloadFile(GetTestUrl("/test.txt"), dest, options);

  EXPECT_EQ(DownloadError::kHashMismatch, result);
  EXPECT_FALSE(base::PathExists(dest));  // Should not keep mismatched file
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(InstallerDownloadServerTest, ResumesOriginBoundPartialWithValidRange) {
  DownloadOptions options = ResumeOptions();
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  const size_t offset = 9;
  ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, offset)));
  std::vector<std::pair<uint64_t, uint64_t>> progress;
  options.progress_callback = base::BindRepeating(
      [](std::vector<std::pair<uint64_t, uint64_t>>* values, uint64_t done,
         uint64_t total) {
        values->emplace_back(done, total);
        return true;
      },
      &progress);

  EXPECT_EQ(DownloadError::kSuccess, DownloadFile(url, destination, options));
  EXPECT_EQ(1, resume_request_count_);
  ASSERT_EQ(1u, seen_ranges_.size());
  EXPECT_EQ("bytes=9-", seen_ranges_[0]);
  ASSERT_FALSE(progress.empty());
  EXPECT_EQ(offset, progress.front().first);
  for (size_t i = 1; i < progress.size(); ++i) {
    EXPECT_LE(progress[i - 1].first, progress[i].first);
  }
  EXPECT_EQ(resume_content_.size(), progress.back().first);
  EXPECT_EQ(resume_content_.size(), progress.back().second);
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(destination, &content));
  EXPECT_EQ(resume_content_, content);
  EXPECT_FALSE(base::PathExists(partial));
}

TEST_F(InstallerDownloadServerTest, RangeIgnoredConsumesSameResponseFromZero) {
  resume_mode_ = ResumeMode::kIgnoreRange;
  DownloadOptions options = ResumeOptions();
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, 7)));

  EXPECT_EQ(DownloadError::kSuccess, DownloadFile(url, destination, options));
  EXPECT_EQ(1, resume_request_count_);
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(destination, &content));
  EXPECT_EQ(resume_content_, content);
}

TEST_F(InstallerDownloadServerTest, InvalidRangeUsesOneCleanRetry) {
  resume_mode_ = ResumeMode::kMalformedRange;
  DownloadOptions options = ResumeOptions();
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, 5)));

  EXPECT_EQ(DownloadError::kSuccess, DownloadFile(url, destination, options));
  EXPECT_EQ(2, resume_request_count_);
  ASSERT_EQ(2u, seen_ranges_.size());
  EXPECT_EQ("bytes=5-", seen_ranges_[0]);
  EXPECT_TRUE(seen_ranges_[1].empty());
}

TEST_F(InstallerDownloadServerTest,
       RangeValidationRejectsHeadersAndBodyBeforePromotion) {
  const ResumeMode modes[] = {
      ResumeMode::kDuplicateContentRange,
      ResumeMode::kMismatchedContentLength,
      ResumeMode::kOverlongRangeBody,
  };
  DownloadOptions options = ResumeOptions();
  CleanDownloadRetryBudget budget;
  budget.consumed = true;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  for (ResumeMode mode : modes) {
    SCOPED_TRACE(static_cast<int>(mode));
    resume_mode_ = mode;
    resume_request_count_ = 0;
    seen_ranges_.clear();
    ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, 5)));
    EXPECT_EQ(DownloadError::kProtocolError,
              DownloadFile(url, destination, options));
    EXPECT_EQ(1, resume_request_count_);
    EXPECT_FALSE(base::PathExists(partial));
    EXPECT_FALSE(base::PathExists(destination));
  }
}

TEST_F(InstallerDownloadServerTest, TruncatedRangeBodyPreservesPartial) {
  resume_mode_ = ResumeMode::kShortRangeBody;
  DownloadOptions options = ResumeOptions();
  CleanDownloadRetryBudget budget;
  budget.consumed = true;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, 5)));

  EXPECT_EQ(DownloadError::kNetworkError,
            DownloadFile(url, destination, options));
  EXPECT_TRUE(base::PathExists(partial));
  EXPECT_FALSE(base::PathExists(destination));
}

TEST_F(InstallerDownloadServerTest, ResumeHeaderTimeoutPreservesBoundPartial) {
  DownloadOptions options = ResumeOptions();
  options.receive_timeout_ms = 100;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/headers.txt");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  const std::string prefix = resume_content_.substr(0, 4);
  ASSERT_TRUE(base::WriteFile(partial, prefix));
  DownloadError result = DownloadError::kSuccess;

  std::thread downloader(
      [&] { result = DownloadFile(url, destination, options); });
  header_response_->WaitForRequest();
  downloader.join();
  header_response_->Done();

  EXPECT_EQ(DownloadError::kNetworkError, result);
  std::string retained;
  ASSERT_TRUE(base::ReadFileToString(partial, &retained));
  EXPECT_EQ(prefix, retained);
  EXPECT_FALSE(base::PathExists(destination));
}

TEST_F(InstallerDownloadServerTest,
       ResumeBodyTimeoutPreservesBytesReceivedBeforeInterruption) {
  DownloadOptions options = ResumeOptions();
  options.receive_timeout_ms = 100;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/slow.txt");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  const std::string prefix = resume_content_.substr(0, 4);
  ASSERT_TRUE(base::WriteFile(partial, prefix));
  DownloadError result = DownloadError::kSuccess;

  std::thread downloader(
      [&] { result = DownloadFile(url, destination, options); });
  slow_response_->WaitForRequest();
  slow_response_->Send(
      "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 4-31/32\r\n"
      "Content-Length: 28\r\nContent-Type: application/octet-stream\r\n\r\n");
  slow_response_->Send("45");
  downloader.join();
  slow_response_->Done();

  EXPECT_EQ(DownloadError::kNetworkError, result);
  std::string retained;
  ASSERT_TRUE(base::ReadFileToString(partial, &retained));
  EXPECT_EQ("012345", retained);
  EXPECT_FALSE(base::PathExists(destination));
}

TEST_F(InstallerDownloadServerTest, RangeValidationRejectsOversizeTotal) {
  DownloadOptions options = ResumeOptions();
  options.max_download_size = resume_content_.size() - 1;
  CleanDownloadRetryBudget budget;
  budget.consumed = true;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, 5)));

  EXPECT_EQ(DownloadError::kProtocolError,
            DownloadFile(url, destination, options));
  EXPECT_FALSE(base::PathExists(partial));
}

TEST_F(InstallerDownloadServerTest, ExhaustedCleanRetryRejectsRangeProtocol) {
  resume_mode_ = ResumeMode::kAlwaysRangeNotSatisfiable;
  DownloadOptions options = ResumeOptions();
  CleanDownloadRetryBudget budget;
  budget.consumed = true;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, 5)));

  EXPECT_EQ(DownloadError::kProtocolError,
            DownloadFile(url, destination, options));
  EXPECT_EQ(1, resume_request_count_);
  EXPECT_FALSE(base::PathExists(partial));
  EXPECT_FALSE(base::PathExists(destination));
}

TEST_F(InstallerDownloadServerTest, CancellationPreservesBoundPartial) {
  DownloadOptions options = ResumeOptions();
  options.progress_callback =
      base::BindRepeating([](uint64_t, uint64_t) { return false; });
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  const std::string prefix = resume_content_.substr(0, 8);
  ASSERT_TRUE(base::WriteFile(partial, prefix));

  EXPECT_EQ(DownloadError::kCancelled, DownloadFile(url, destination, options));
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(partial, &content));
  EXPECT_EQ(prefix, content);
  EXPECT_FALSE(base::PathExists(destination));
}

#endif

TEST_F(InstallerDownloadServerTest, HashMismatchUsesSharedCleanRetry) {
  resume_mode_ = ResumeMode::kWrongFullBodyOnce;
  DownloadOptions options = ResumeOptions();
  CleanDownloadRetryBudget budget;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);

  EXPECT_EQ(DownloadError::kSuccess,
            DownloadFile(GetTestUrl("/archive.tar.xz"), destination, options));
  EXPECT_TRUE(budget.consumed);
  EXPECT_EQ(2, resume_request_count_);
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(InstallerDownloadServerTest,
       ProtocolThenHashMismatchHasOnlyOneCleanRequest) {
  resume_mode_ = ResumeMode::kMalformedThenWrongFullBody;
  DownloadOptions options = ResumeOptions();
  CleanDownloadRetryBudget budget;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, 5)));

  EXPECT_EQ(DownloadError::kHashMismatch,
            DownloadFile(url, destination, options));
  EXPECT_TRUE(budget.consumed);
  EXPECT_EQ(2, resume_request_count_);
  EXPECT_FALSE(base::PathExists(destination));
}

#endif

TEST_F(InstallerDownloadServerTest, TwoHashMismatchesStopAfterCleanRetry) {
  resume_mode_ = ResumeMode::kAlwaysWrongFullBody;
  DownloadOptions options = ResumeOptions();
  const base::FilePath destination = ArchiveDestination(options);

  EXPECT_EQ(DownloadError::kHashMismatch,
            DownloadFile(GetTestUrl("/archive.tar.xz"), destination, options));
  EXPECT_EQ(2, resume_request_count_);
  EXPECT_FALSE(base::PathExists(destination));
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(InstallerDownloadServerTest, MultipleFinalOriginVariantsRestartClean) {
  DownloadOptions options = ResumeOptions();
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath first =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  const base::FilePath second = internal::GetDownloadPartialPathForTesting(
      destination, url, final_server_->GetURL("/archive.tar.xz").spec());
  ASSERT_NE(first, second);
  ASSERT_TRUE(base::WriteFile(first, "one"));
  ASSERT_TRUE(base::WriteFile(second, "two"));

  EXPECT_EQ(DownloadError::kSuccess, DownloadFile(url, destination, options));
  ASSERT_EQ(1u, seen_ranges_.size());
  EXPECT_TRUE(seen_ranges_[0].empty());
  EXPECT_FALSE(base::PathExists(first));
  EXPECT_FALSE(base::PathExists(second));
}

TEST_F(InstallerDownloadServerTest, ZeroLengthPartialStartsWithoutRange) {
  DownloadOptions options = ResumeOptions();
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  ASSERT_TRUE(base::WriteFile(partial, ""));

  EXPECT_EQ(DownloadError::kSuccess, DownloadFile(url, destination, options));
  ASSERT_EQ(1u, seen_ranges_.size());
  EXPECT_TRUE(seen_ranges_[0].empty());
}

TEST_F(InstallerDownloadServerTest, OversizePartialRestartsWithoutRange) {
  DownloadOptions options = ResumeOptions();
  options.max_download_size = 64;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  ASSERT_TRUE(base::WriteFile(partial, std::string(65, 'x')));

  EXPECT_EQ(DownloadError::kSuccess, DownloadFile(url, destination, options));
  ASSERT_EQ(1u, seen_ranges_.size());
  EXPECT_TRUE(seen_ranges_[0].empty());
}

TEST_F(InstallerDownloadServerTest, CrossOriginRedirectRestartsOnce) {
  DownloadOptions options = ResumeOptions();
  CleanDownloadRetryBudget budget;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string configured_url = GetTestUrl("/cross-origin-archive");
  const base::FilePath old_partial = internal::GetDownloadPartialPathForTesting(
      destination, configured_url, configured_url);
  ASSERT_TRUE(base::WriteFile(old_partial, resume_content_.substr(0, 6)));

  EXPECT_EQ(DownloadError::kSuccess,
            DownloadFile(configured_url, destination, options));
  EXPECT_TRUE(budget.consumed);
  EXPECT_EQ(2, resume_request_count_);
  EXPECT_FALSE(base::PathExists(old_partial));
}

TEST_F(InstallerDownloadServerTest,
       CrossOriginRedirectWithFullResponseUsesSameRequest) {
  resume_mode_ = ResumeMode::kIgnoreRange;
  DownloadOptions options = ResumeOptions();
  CleanDownloadRetryBudget budget;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string configured_url = GetTestUrl("/cross-origin-archive");
  const base::FilePath old_partial = internal::GetDownloadPartialPathForTesting(
      destination, configured_url, configured_url);
  ASSERT_TRUE(base::WriteFile(old_partial, resume_content_.substr(0, 6)));

  EXPECT_EQ(DownloadError::kSuccess,
            DownloadFile(configured_url, destination, options));
  EXPECT_FALSE(budget.consumed);
  EXPECT_EQ(1, resume_request_count_);
}

TEST_F(InstallerDownloadServerTest,
       CrossOriginRangeWithExhaustedBudgetFailsBeforeAppend) {
  DownloadOptions options = ResumeOptions();
  CleanDownloadRetryBudget budget;
  budget.consumed = true;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string configured_url = GetTestUrl("/cross-origin-archive");
  const base::FilePath old_partial = internal::GetDownloadPartialPathForTesting(
      destination, configured_url, configured_url);
  ASSERT_TRUE(base::WriteFile(old_partial, resume_content_.substr(0, 6)));

  EXPECT_EQ(DownloadError::kProtocolError,
            DownloadFile(configured_url, destination, options));
  EXPECT_EQ(1, resume_request_count_);
  EXPECT_FALSE(base::PathExists(old_partial));
  EXPECT_FALSE(base::PathExists(destination));
}

TEST_F(InstallerDownloadServerTest, SameOriginRedirectResumesWithoutRetry) {
  DownloadOptions options = ResumeOptions();
  CleanDownloadRetryBudget budget;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string configured_url = GetTestUrl("/redirect-archive");
  const std::string final_url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial = internal::GetDownloadPartialPathForTesting(
      destination, configured_url, final_url);
  ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, 4)));

  EXPECT_EQ(DownloadError::kSuccess,
            DownloadFile(configured_url, destination, options));
  EXPECT_FALSE(budget.consumed);
  EXPECT_EQ(1, resume_request_count_);
  ASSERT_EQ(1u, seen_ranges_.size());
  EXPECT_EQ("bytes=4-", seen_ranges_[0]);
}

TEST_F(InstallerDownloadServerTest,
       HttpsCrossOriginRedirectResumesWhenBothOriginsMatch) {
  DownloadOptions options = ResumeOptions();
  options.allow_http_for_testing = false;
  CleanDownloadRetryBudget budget;
  options.clean_retry_budget = &budget;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string configured_url =
      https_server_->GetURL("/https-cross-origin-archive").spec();
  const std::string final_url =
      https_final_server_->GetURL("/archive.tar.xz").spec();
  const base::FilePath partial = internal::GetDownloadPartialPathForTesting(
      destination, configured_url, final_url);
  ASSERT_TRUE(base::WriteFile(partial, resume_content_.substr(0, 4)));

  EXPECT_EQ(DownloadError::kSuccess,
            DownloadFile(configured_url, destination, options));
  EXPECT_FALSE(budget.consumed);
  EXPECT_EQ(1, resume_request_count_);
  ASSERT_EQ(1u, seen_ranges_.size());
  EXPECT_EQ("bytes=4-", seen_ranges_[0]);
}

TEST_F(InstallerDownloadServerTest,
       HttpsToHttpRedirectIsRejectedBeforeTargetRequest) {
  DownloadOptions options = ResumeOptions();
  options.allow_http_for_testing = false;
  const base::FilePath destination = ArchiveDestination(options);
  const std::string configured_url =
      https_server_->GetURL("/https-downgrade-archive").spec();
  const base::FilePath partial = internal::GetDownloadPartialPathForTesting(
      destination, configured_url, configured_url);
  const std::string prefix = resume_content_.substr(0, 4);
  ASSERT_TRUE(base::WriteFile(partial, prefix));

  EXPECT_EQ(DownloadError::kNetworkError,
            DownloadFile(configured_url, destination, options));
  EXPECT_EQ(0, resume_request_count_);
  std::string retained;
  ASSERT_TRUE(base::ReadFileToString(partial, &retained));
  EXPECT_EQ(prefix, retained);
  EXPECT_FALSE(base::PathExists(destination));
}

TEST_F(InstallerDownloadServerTest, ConfiguredOriginChangeDiscardsPartial) {
  DownloadOptions options = ResumeOptions();
  const base::FilePath destination = ArchiveDestination(options);
  const std::string old_url = GetTestUrl("/archive.tar.xz");
  const std::string new_url = final_server_->GetURL("/archive.tar.xz").spec();
  const base::FilePath old_partial =
      internal::GetDownloadPartialPathForTesting(destination, old_url, old_url);
  ASSERT_TRUE(base::WriteFile(old_partial, resume_content_.substr(0, 4)));

  EXPECT_EQ(DownloadError::kSuccess,
            DownloadFile(new_url, destination, options));
  EXPECT_FALSE(base::PathExists(old_partial));
  ASSERT_EQ(1u, seen_ranges_.size());
  EXPECT_TRUE(seen_ranges_[0].empty());
}

TEST_F(InstallerDownloadServerTest, UnsafePartialEntryIsTerminalBeforeNetwork) {
  DownloadOptions options = ResumeOptions();
  const base::FilePath destination = ArchiveDestination(options);
  const std::string url = GetTestUrl("/archive.tar.xz");
  const base::FilePath partial =
      internal::GetDownloadPartialPathForTesting(destination, url, url);
  ASSERT_TRUE(base::CreateDirectory(partial));
  ASSERT_TRUE(base::WriteFile(partial.AppendASCII("marker"), "unsafe"));

  EXPECT_EQ(DownloadError::kFileWriteError,
            DownloadFile(url, destination, options));
  EXPECT_EQ(0, resume_request_count_);
  EXPECT_TRUE(base::DirectoryExists(partial));
}

#endif

TEST_F(InstallerDownloadServerTest, PromotionFailureIsTerminalWithoutRetry) {
  DownloadOptions options = ResumeOptions();
  const base::FilePath destination = ArchiveDestination(options);
  ASSERT_TRUE(base::CreateDirectory(destination));
  ASSERT_TRUE(
      base::WriteFile(destination.AppendASCII("marker"), "not replaceable"));

  EXPECT_EQ(DownloadError::kFileWriteError,
            DownloadFile(GetTestUrl("/archive.tar.xz"), destination, options));
  EXPECT_EQ(1, resume_request_count_);
  EXPECT_TRUE(base::DirectoryExists(destination));
}

TEST_F(InstallerDownloadServerTest, DownloadToString) {
  std::string content;

  DownloadError result =
      DownloadToString(GetTestUrl("/test.txt"), &content, TestOptions());

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(kTestContent, content);
}

TEST_F(InstallerDownloadServerTest, AbsoluteDeadlineDownloadSucceeds) {
  DownloadOptions options = TestOptions();
  std::string content;

  DownloadError result = DownloadToStringWithDeadline(
      GetTestUrl("/test.txt"), &content, base::Seconds(1), options);

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(kTestContent, content);
}

TEST_F(InstallerDownloadServerTest, AbsoluteDeadlineCancelsHeaderWait) {
  DownloadOptions options = TestOptions();
  options.connect_timeout_ms = 1000;
  options.receive_timeout_ms = 1000;
  DownloadError result = DownloadError::kSuccess;
  base::TimeDelta elapsed;

  std::thread downloader([&] {
    base::TimeTicks start = base::TimeTicks::Now();
    std::string content;
    result = DownloadToStringWithDeadline(GetTestUrl("/headers.txt"), &content,
                                          base::Milliseconds(100), options);
    elapsed = base::TimeTicks::Now() - start;
  });

  header_response_->WaitForRequest();
  downloader.join();
  header_response_->Done();

  EXPECT_EQ(DownloadError::kNetworkError, result);
  EXPECT_GE(elapsed, base::Milliseconds(70));
  EXPECT_LT(elapsed, base::Milliseconds(400));
}

TEST_F(InstallerDownloadServerTest, AbsoluteDeadlineCancelsSlowResponseBody) {
  DownloadOptions options = TestOptions();
  options.connect_timeout_ms = 1000;
  options.receive_timeout_ms = 1000;
  DownloadError result = DownloadError::kSuccess;
  base::TimeDelta elapsed;

  std::thread downloader([&] {
    base::TimeTicks start = base::TimeTicks::Now();
    std::string content;
    result = DownloadToStringWithDeadline(GetTestUrl("/slow.txt"), &content,
                                          base::Milliseconds(120), options);
    elapsed = base::TimeTicks::Now() - start;
  });

  slow_response_->WaitForRequest();
  slow_response_->Send(
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n"
      "Content-Type: text/plain\r\n\r\n");
  slow_response_->Send("1\r\nx\r\n");
  downloader.join();
  slow_response_->Done();

  EXPECT_EQ(DownloadError::kNetworkError, result);
  EXPECT_GE(elapsed, base::Milliseconds(90));
  EXPECT_LT(elapsed, base::Milliseconds(400));
}

TEST_F(InstallerDownloadServerTest, ProgressCallback) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("large.txt");
  int callback_count = 0;

  DownloadOptions options = TestOptions();
  options.progress_callback = base::BindRepeating(
      [](int* count, uint64_t downloaded, uint64_t total) -> bool {
        (*count)++;
        return true;  // Continue download
      },
      &callback_count);

  DownloadError result = DownloadFile(GetTestUrl("/large.txt"), dest, options);

  EXPECT_EQ(DownloadError::kSuccess, result);
  // WinHTTP read loop should invoke progress callback at least once.
  EXPECT_GT(callback_count, 0);
}

TEST_F(InstallerDownloadServerTest, Cancellation) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("cancelled.txt");

  DownloadOptions options = TestOptions();
  options.progress_callback =
      base::BindRepeating([](uint64_t downloaded, uint64_t total) -> bool {
        return false;  // Cancel immediately
      });

  DownloadError result = DownloadFile(GetTestUrl("/large.txt"), dest, options);

  // Should be cancelled (may succeed if entire body arrives in first read).
  EXPECT_TRUE(result == DownloadError::kCancelled ||
              result == DownloadError::kSuccess);
}

// ============================================================================
// CDN-First Cache Tests
// ============================================================================

TEST_F(InstallerDownloadServerTest, DownloadWithCacheMiss) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  std::string url = GetTestUrl("/test.txt");

  std::string content;
  DownloadError result =
      DownloadWithCache(url, cache_dir, &content, TestOptions());

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(kTestContent, content);

  // Should be cached now
  base::FilePath cache_path = GetCacheFilePath(cache_dir, url);
  EXPECT_TRUE(base::PathExists(cache_path));
}

TEST_F(InstallerDownloadServerTest, DownloadWithCacheCdnValidates304) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  std::string url = GetTestUrl("/test.txt");
  base::FilePath cache_path = GetCacheFilePath(cache_dir, url);

  // Pre-populate cache with integrity footer.
  std::string cached_content = "cached version";
  ASSERT_TRUE(WriteFileWithIntegrity(cache_path, cached_content));

  // Server will return 304 for HEAD with If-Modified-Since.
  std::string content;
  DownloadError result =
      DownloadWithCache(url, cache_dir, &content, TestOptions());

  EXPECT_EQ(DownloadError::kSuccess, result);
  // Should return cached content since CDN says 304 Not Modified.
  EXPECT_EQ(cached_content, content);
}

TEST_F(InstallerDownloadServerTest, DownloadWithCacheCdnReturns200) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  std::string url = GetTestUrl("/changing.txt");
  base::FilePath cache_path = GetCacheFilePath(cache_dir, url);

  // Pre-populate cache with old content.
  std::string old_content = "old version";
  ASSERT_TRUE(WriteFileWithIntegrity(cache_path, old_content));

  // /changing.txt HEAD always returns 200 (modified).
  server_content_ = "new version from CDN";

  std::string content;
  DownloadError result =
      DownloadWithCache(url, cache_dir, &content, TestOptions());

  EXPECT_EQ(DownloadError::kSuccess, result);
  // Should return fresh content from CDN.
  EXPECT_EQ("new version from CDN", content);
}

TEST_F(InstallerDownloadServerTest, DownloadWithCacheDefersPromotion) {
  const base::FilePath cache_dir =
      temp_dir_.GetPath().AppendASCII(".cache-deferred-promotion");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const std::string url = GetTestUrl("/changing.txt");
  ASSERT_EQ(DownloadError::kSuccess,
            WriteDownloadCache(cache_dir, url, "validated old content"));
  server_content_ = "unvalidated new content";

  std::string content;
  DownloadContentSource source = DownloadContentSource::kCache;
  ASSERT_EQ(DownloadError::kSuccess,
            DownloadWithCache(url, cache_dir, &content, TestOptions(), false,
                              StaleCacheFallback::kSkip, url,
                              CacheWriteBehavior::kDefer, &source));
  EXPECT_EQ(DownloadContentSource::kNetwork, source);
  EXPECT_EQ("unvalidated new content", content);

  content.clear();
  ASSERT_EQ(DownloadError::kSuccess,
            ReadDownloadCache(cache_dir, url, &content));
  EXPECT_EQ("validated old content", content);

  ASSERT_EQ(DownloadError::kSuccess,
            WriteDownloadCache(cache_dir, url, "validated new content"));
  ASSERT_TRUE(DiscardDownloadCache(cache_dir, url));
  EXPECT_EQ(DownloadError::kNetworkError,
            ReadDownloadCache(cache_dir, url, &content));
}

TEST_F(InstallerDownloadServerTest, DownloadWithCacheForceCheck) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  std::string url = GetTestUrl("/test.txt");

  // Pre-populate cache with stale content
  base::FilePath cache_path = GetCacheFilePath(cache_dir, url);
  std::string stale_content = "stale cached content";
  ASSERT_TRUE(base::WriteFile(cache_path, stale_content));

  // With force_check, should bypass cache and fetch fresh content.
  std::string content;
  DownloadError result =
      DownloadWithCache(url, cache_dir, &content, TestOptions(),
                        /*force_check=*/true);
  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(kTestContent, content);  // Returns fresh content from server
}

TEST_F(InstallerDownloadServerTest, DownloadWithCacheLegacyFileServedOn304) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  std::string url = GetTestUrl("/test.txt");
  base::FilePath cache_path = GetCacheFilePath(cache_dir, url);

  // Pre-populate cache with legacy format (no integrity footer).
  std::string legacy_content = "legacy cached data";
  ASSERT_TRUE(base::WriteFile(cache_path, legacy_content));

  // CDN returns 304 — should still serve legacy content.
  std::string content;
  DownloadError result =
      DownloadWithCache(url, cache_dir, &content, TestOptions());
  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(legacy_content, content);
}

// ============================================================================
// Cache Integrity Tests
// ============================================================================

TEST_F(InstallerDownloadServerTest, DownloadWithCacheWritesIntegrityFooter) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  std::string url = GetTestUrl("/test.txt");

  // Download (cache miss) — should write cache with integrity footer.
  std::string content;
  DownloadError result =
      DownloadWithCache(url, cache_dir, &content, TestOptions());
  EXPECT_EQ(DownloadError::kSuccess, result);

  // Read back with integrity check — should return kSuccess (not NoFooter).
  base::FilePath cache_path = GetCacheFilePath(cache_dir, url);
  std::string cached;
  IntegrityResult ir = ReadFileWithIntegrity(cache_path, &cached);
  EXPECT_EQ(IntegrityResult::kSuccess, ir);
  EXPECT_EQ(kTestContent, cached);
}

TEST_F(InstallerDownloadServerTest,
       DownloadWithCacheCorruptedCacheFallsThrough) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  std::string url = GetTestUrl("/test.txt");

  // Write a cache file with integrity footer, then corrupt it.
  base::FilePath cache_path = GetCacheFilePath(cache_dir, url);
  ASSERT_TRUE(WriteFileWithIntegrity(cache_path, "original cached"));
  std::string raw;
  ASSERT_TRUE(base::ReadFileToString(cache_path, &raw));
  raw[0] = ~raw[0];  // Corrupt first byte
  ASSERT_TRUE(base::WriteFile(cache_path, raw));

  // CDN will return 304 for HEAD, but integrity check will fail,
  // causing a fall-through to fresh download.
  std::string content;
  DownloadError result =
      DownloadWithCache(url, cache_dir, &content, TestOptions());
  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(kTestContent, content);  // Fresh content from server
}

// ============================================================================
// Security: File Size Limit Tests (H1)
// ============================================================================

TEST_F(InstallerDownloadServerTest, DownloadFileSizeLimitExceeded) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("oversized.bin");

  DownloadOptions options = TestOptions();
  // Set a very small limit (1 KB) to trigger rejection
  options.max_download_size = 1024;

  // Download a 2 KB file - should be rejected
  DownloadError result =
      DownloadFile(GetTestUrl("/oversized.bin"), dest, options);

  EXPECT_EQ(DownloadError::kFileTooLarge, result);
  EXPECT_FALSE(base::PathExists(dest));  // File should be deleted
}

TEST_F(InstallerDownloadServerTest, DownloadFileSizeLimitDisabled) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("oversized.bin");

  DownloadOptions options = TestOptions();
  // Disable size limit
  options.max_download_size = 0;

  // Download should succeed even for large files
  DownloadError result =
      DownloadFile(GetTestUrl("/oversized.bin"), dest, options);

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_TRUE(base::PathExists(dest));
}

TEST_F(InstallerDownloadServerTest, DownloadFileSizeLimitJustUnder) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("test.txt");

  DownloadOptions options = TestOptions();
  // Set limit just above file size
  options.max_download_size = 1024 * 1024;  // 1 MB (file is ~21 bytes)

  DownloadError result = DownloadFile(GetTestUrl("/test.txt"), dest, options);

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_TRUE(base::PathExists(dest));
}

// ============================================================================
// Security: Streaming Hash Tests (H2)
// ============================================================================

TEST_F(InstallerDownloadTest, ComputeFileSha256LargeFile) {
  // Create a large file (1 MB) to verify streaming hash works without
  // loading entire file into memory
  std::string large_content(1024 * 1024, 'X');  // 1 MB of 'X'
  base::FilePath path = CreateTestFile(large_content);

  std::string hash = ComputeFileSha256(path);

  EXPECT_FALSE(hash.empty());
  EXPECT_EQ(64u, hash.size());
  // Hash should be consistent
  std::string hash2 = ComputeFileSha256(path);
  EXPECT_EQ(hash, hash2);
}

TEST_F(InstallerDownloadTest, ComputeFileSha1LargeFile) {
  // Create a large file (1 MB) to verify streaming hash works
  std::string large_content(1024 * 1024, 'Y');  // 1 MB of 'Y'
  base::FilePath path = CreateTestFile(large_content);

  std::string hash = ComputeFileSha1(path);

  EXPECT_FALSE(hash.empty());
  EXPECT_EQ(40u, hash.size());
  // Hash should be consistent
  std::string hash2 = ComputeFileSha1(path);
  EXPECT_EQ(hash, hash2);
}

// ============================================================================
// Security: Download Size Limit Tests (H1)
// ============================================================================

TEST_F(InstallerDownloadTest, DownloadOptionsDefaultMaxSize) {
  DownloadOptions options;

  // Default should be 500 MB
  EXPECT_EQ(kDefaultMaxDownloadSize, options.max_download_size);
  EXPECT_EQ(500u * 1024 * 1024, options.max_download_size);
}

TEST_F(InstallerDownloadTest, DownloadOptionsZeroDisablesLimit) {
  DownloadOptions options;
  options.max_download_size = 0;

  // Zero means no limit
  EXPECT_EQ(0u, options.max_download_size);
}

// ============================================================================
// Error String Tests
// ============================================================================

TEST_F(InstallerDownloadTest, DownloadErrorToString) {
  EXPECT_STREQ("Success", DownloadErrorToString(DownloadError::kSuccess));
  EXPECT_STREQ("Invalid URL (must be HTTPS)",
               DownloadErrorToString(DownloadError::kInvalidUrl));
  EXPECT_STREQ("Network error",
               DownloadErrorToString(DownloadError::kNetworkError));
  EXPECT_STREQ("HTTP error", DownloadErrorToString(DownloadError::kHttpError));
  EXPECT_STREQ("File write error",
               DownloadErrorToString(DownloadError::kFileWriteError));
  EXPECT_STREQ("Hash mismatch",
               DownloadErrorToString(DownloadError::kHashMismatch));
  EXPECT_STREQ("Cancelled", DownloadErrorToString(DownloadError::kCancelled));
  EXPECT_STREQ("File exceeds maximum allowed size",
               DownloadErrorToString(DownloadError::kFileTooLarge));
  EXPECT_STREQ("Invalid HTTP range response",
               DownloadErrorToString(DownloadError::kProtocolError));
}

// ============================================================================
// Internal Helper Tests (ParseUrl, FormatHttpDate, HeadRequest)
// ============================================================================

using internal::FormatHttpDate;
using internal::HeadRequest;
using internal::HeadResult;
using internal::ParseUrl;
using internal::UrlComponents;

TEST_F(InstallerDownloadTest, ParseUrlHttps) {
  UrlComponents parts;
  ASSERT_TRUE(ParseUrl("https://cdn.example.com/path/file.json", &parts));

  EXPECT_EQ(L"cdn.example.com", parts.host);
  EXPECT_EQ(L"/path/file.json", parts.path);
  EXPECT_EQ(443, parts.port);
  EXPECT_TRUE(parts.is_https);
}

TEST_F(InstallerDownloadTest, ParseUrlHttp) {
  UrlComponents parts;
  ASSERT_TRUE(ParseUrl("http://localhost:8080/test.txt", &parts));

  EXPECT_EQ(L"localhost", parts.host);
  EXPECT_EQ(L"/test.txt", parts.path);
  EXPECT_EQ(8080, parts.port);
  EXPECT_FALSE(parts.is_https);
}

TEST_F(InstallerDownloadTest, ParseUrlCustomPort) {
  UrlComponents parts;
  ASSERT_TRUE(ParseUrl("https://example.com:8443/api/v1", &parts));

  EXPECT_EQ(L"example.com", parts.host);
  EXPECT_EQ(L"/api/v1", parts.path);
  EXPECT_EQ(8443, parts.port);
  EXPECT_TRUE(parts.is_https);
}

TEST_F(InstallerDownloadTest, ParseUrlInvalid) {
  UrlComponents parts;
  EXPECT_FALSE(ParseUrl("not-a-url", &parts));
  EXPECT_FALSE(ParseUrl("", &parts));
}

TEST_F(InstallerDownloadTest, ParseUrlRootPath) {
  UrlComponents parts;
  ASSERT_TRUE(ParseUrl("https://example.com/", &parts));

  EXPECT_EQ(L"example.com", parts.host);
  EXPECT_EQ(L"/", parts.path);
}

TEST_F(InstallerDownloadTest, FormatHttpDateKnownDate) {
  // 2026-03-08 12:30:45 UTC is a Sunday.
  base::Time::Exploded exploded = {};
  exploded.year = 2026;
  exploded.month = 3;
  exploded.day_of_week = 0;  // Sunday
  exploded.day_of_month = 8;
  exploded.hour = 12;
  exploded.minute = 30;
  exploded.second = 45;

  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCExploded(exploded, &time));

  std::wstring result = FormatHttpDate(time);
  EXPECT_EQ(L"Sun, 08 Mar 2026 12:30:45 GMT", result);
}

TEST_F(InstallerDownloadTest, FormatHttpDateEpoch) {
  // Unix epoch: 1970-01-01 00:00:00 UTC is a Thursday.
  base::Time epoch = base::Time::UnixEpoch();

  std::wstring result = FormatHttpDate(epoch);
  EXPECT_EQ(L"Thu, 01 Jan 1970 00:00:00 GMT", result);
}

TEST_F(InstallerDownloadTest, FormatHttpDateDecember) {
  // Test month boundary — December 31st.
  base::Time::Exploded exploded = {};
  exploded.year = 2025;
  exploded.month = 12;
  exploded.day_of_week = 3;  // Wednesday
  exploded.day_of_month = 31;
  exploded.hour = 23;
  exploded.minute = 59;
  exploded.second = 59;

  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCExploded(exploded, &time));

  std::wstring result = FormatHttpDate(time);
  EXPECT_EQ(L"Wed, 31 Dec 2025 23:59:59 GMT", result);
}

// HeadRequest tests use the embedded test server.

TEST_F(InstallerDownloadServerTest, HeadRequestReturns304) {
  std::string url = GetTestUrl("/test.txt");

  // Server returns 304 for HEAD with If-Modified-Since.
  DownloadOptions head_opts;
  head_opts.allow_http_for_testing = true;
  HeadResult result = HeadRequest(url, base::Time::Now(), head_opts);
  EXPECT_EQ(HeadResult::kNotModified, result);
}

TEST_F(InstallerDownloadServerTest, HeadRequestReturns200) {
  std::string url = GetTestUrl("/changing.txt");

  // /changing.txt HEAD always returns 200 (modified).
  DownloadOptions head_opts;
  head_opts.allow_http_for_testing = true;
  HeadResult result = HeadRequest(url, base::Time::Now(), head_opts);
  EXPECT_EQ(HeadResult::kModified, result);
}

TEST_F(InstallerDownloadServerTest, HeadRequestReturnsErrorOn404) {
  std::string url = GetTestUrl("/notfound");

  // 404 is treated as network error.
  DownloadOptions head_opts;
  head_opts.allow_http_for_testing = true;
  HeadResult result = HeadRequest(url, base::Time::Now(), head_opts);
  EXPECT_EQ(HeadResult::kNetworkError, result);
}

TEST_F(InstallerDownloadServerTest, HeadRequestForceModified) {
  force_modified_ = true;
  std::string url = GetTestUrl("/test.txt");

  // With force_modified_, HEAD to /test.txt returns 200 even with IMS header.
  DownloadOptions head_opts;
  head_opts.allow_http_for_testing = true;
  HeadResult result = HeadRequest(url, base::Time::Now(), head_opts);
  EXPECT_EQ(HeadResult::kModified, result);
}

// ============================================================================
// Additional Hash Verification Tests
// ============================================================================

TEST_F(InstallerDownloadTest, VerifyFileHashWrongSha1) {
  base::FilePath path = CreateTestFile(kTestContent);
  std::string wrong_sha1 = "0000000000000000000000000000000000000000";

  // Empty SHA256 + wrong SHA1 → false
  EXPECT_FALSE(VerifyFileHash(path, "", wrong_sha1));
}

// ============================================================================
// Additional URL / Parse Tests
// ============================================================================

TEST_F(InstallerDownloadTest, ParseUrlNoHost) {
  UrlComponents parts;
  // "https://" passes StartsWith check but has no host for WinHttpCrackUrl.
  EXPECT_FALSE(ParseUrl("https://", &parts));
}

TEST_F(InstallerDownloadTest, DownloadFileUrlParseFailsAfterSchemeCheck) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("download.txt");

  // "https://" passes IsValidDownloadUrl but fails ParseUrl.
  DownloadError result = DownloadFile("https://", dest);

  EXPECT_EQ(DownloadError::kInvalidUrl, result);
}

TEST_F(InstallerDownloadTest, HeadRequestInvalidUrl) {
  DownloadOptions head_opts;
  HeadResult result = HeadRequest("not-a-url", base::Time::Now(), head_opts);

  EXPECT_EQ(HeadResult::kNetworkError, result);
}

// ============================================================================
// Null Pointer Guard Tests
// ============================================================================

TEST_F(InstallerDownloadTest, DownloadToStringNullContent) {
  DownloadOptions options;
  options.allow_http_for_testing = true;

  DownloadError result =
      DownloadToString("http://example.com/test.txt", nullptr, options);

  EXPECT_EQ(DownloadError::kInvalidUrl, result);
}

TEST_F(InstallerDownloadTest, DownloadWithCacheNullContent) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");

  DownloadError result = DownloadWithCache(
      "http://example.com/test.txt", cache_dir, nullptr, DownloadOptions());

  EXPECT_EQ(DownloadError::kInvalidUrl, result);
}

// ============================================================================
// HTTP Error Tests
// ============================================================================

TEST_F(InstallerDownloadServerTest, DownloadFileHttpError) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("download.txt");

  DownloadError result =
      DownloadFile(GetTestUrl("/notfound"), dest, TestOptions());

  EXPECT_EQ(DownloadError::kHttpError, result);
  EXPECT_FALSE(base::PathExists(dest));
}

TEST_F(InstallerDownloadServerTest, DownloadFileFollowsRedirect) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("download.txt");

  // WinHTTP follows 301 redirects automatically.
  DownloadError result =
      DownloadFile(GetTestUrl("/redirect"), dest, TestOptions());

  EXPECT_EQ(DownloadError::kSuccess, result);
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(dest, &content));
  EXPECT_EQ(kTestContent, content);
}

TEST_F(InstallerDownloadServerTest,
       DownloadFileNon200SuccessReturnsNetworkError) {
  base::FilePath dest = temp_dir_.GetPath().AppendASCII("download.txt");

  // 204 No Content is not 200, not 4xx/5xx → kNetworkError.
  DownloadError result =
      DownloadFile(GetTestUrl("/no-content"), dest, TestOptions());

  EXPECT_EQ(DownloadError::kNetworkError, result);
  EXPECT_FALSE(base::PathExists(dest));
}

// ============================================================================
// Cache Fallback Tests
// ============================================================================

TEST_F(InstallerDownloadTest,
       DownloadWithCacheNetworkFailFallsBackToStaleCache) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  // Use a URL pointing to a non-listening port so the download fails.
  std::string dead_url = "http://127.0.0.1:1/unreachable";
  base::FilePath cache_path = GetCacheFilePath(cache_dir, dead_url);

  // Pre-populate cache with content (legacy format, no footer).
  std::string stale_content = "stale but usable content";
  ASSERT_TRUE(base::WriteFile(cache_path, stale_content));

  // Backdate the file so IsCacheValid returns false (expired).
  base::Time old_time = base::Time::Now() - base::Seconds(7200);
  ASSERT_TRUE(base::TouchFile(cache_path, old_time, old_time));

  DownloadOptions options;
  options.allow_http_for_testing = true;

  std::string content;
  DownloadError result =
      DownloadWithCache(dead_url, cache_dir, &content, options);

  // Should fall back to stale cache on network failure.
  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(stale_content, content);
}

TEST_F(InstallerDownloadTest, PolicyFailoverDefersStaleCacheUntilOriginsFail) {
  const base::FilePath cache_dir =
      temp_dir_.GetPath().AppendASCII(".cache-deferred");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const std::string dead_url = "http://127.0.0.1:1/unreachable";
  const std::string cache_key = "shared-manifest.json";
  const base::FilePath cache_path = GetCacheFilePath(cache_dir, cache_key);
  ASSERT_TRUE(WriteFileWithIntegrity(cache_path, "stale shared content"));
  const base::Time old_time = base::Time::Now() - base::Hours(2);
  ASSERT_TRUE(base::TouchFile(cache_path, old_time, old_time));

  DownloadOptions options;
  options.allow_http_for_testing = true;
  std::string content;
  EXPECT_EQ(DownloadError::kNetworkError,
            DownloadWithCache(dead_url, cache_dir, &content, options,
                              /*force_check=*/false, StaleCacheFallback::kSkip,
                              cache_key));
  EXPECT_TRUE(content.empty());

  EXPECT_EQ(DownloadError::kSuccess,
            ReadDownloadCache(cache_dir, cache_key, &content));
  EXPECT_EQ("stale shared content", content);

  ASSERT_EQ(DownloadError::kSuccess,
            WriteDownloadCache(cache_dir, cache_key, "validated content"));
  content.clear();
  EXPECT_EQ(DownloadError::kSuccess,
            ReadDownloadCache(cache_dir, cache_key, &content));
  EXPECT_EQ("validated content", content);
}

TEST_F(InstallerDownloadTest, DownloadWithCacheCdnUnreachableFallsBackToCache) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  // Use a URL pointing to a non-listening port.
  std::string dead_url = "http://127.0.0.1:1/unreachable";
  base::FilePath cache_path = GetCacheFilePath(cache_dir, dead_url);

  // Pre-populate cache with fresh content (recently modified).
  std::string cached_content = "fresh cached content";
  ASSERT_TRUE(WriteFileWithIntegrity(cache_path, cached_content));

  DownloadOptions options;
  options.allow_http_for_testing = true;

  std::string content;
  DownloadError result =
      DownloadWithCache(dead_url, cache_dir, &content, options);

  // Cache is fresh, HEAD fails (CDN unreachable) → serve from cache.
  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(cached_content, content);
}

// ============================================================================
// Cache Pruning Tests
// ============================================================================

TEST_F(InstallerDownloadTest, PruneCacheDirectoryDeletesExpired) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  // Create two cache files: one fresh, one expired.
  base::FilePath fresh = cache_dir.AppendASCII("fresh.cache");
  ASSERT_TRUE(base::WriteFile(fresh, "fresh"));

  base::FilePath expired = cache_dir.AppendASCII("expired.cache");
  ASSERT_TRUE(base::WriteFile(expired, "expired"));

  // Backdate the expired file beyond the validity window.
  base::Time old_time = base::Time::Now() - base::Seconds(7200);
  ASSERT_TRUE(base::TouchFile(expired, old_time, old_time));

  PruneCacheDirectory(cache_dir);

  EXPECT_TRUE(base::PathExists(fresh));
  EXPECT_FALSE(base::PathExists(expired));
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(InstallerDownloadTest, PruneArchiveAndOriginBoundPartialTtl) {
  const base::FilePath cache_dir =
      temp_dir_.GetPath().AppendASCII(".cache-archives");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const std::string fresh_hash(64, 'a');
  const std::string expired_hash(64, 'b');
  const base::FilePath fresh_archive =
      cache_dir.AppendASCII(fresh_hash + ".tar.xz");
  const base::FilePath expired_archive =
      cache_dir.AppendASCII(expired_hash + ".tar.xz");
  const base::FilePath fresh_partial =
      internal::GetDownloadPartialPathForTesting(
          fresh_archive, "https://one.example/archive",
          "https://redirect.example/archive");
  const base::FilePath expired_partial =
      internal::GetDownloadPartialPathForTesting(
          expired_archive, "https://two.example/archive",
          "https://redirect.example/archive");
  const base::FilePath unrelated =
      cache_dir.AppendASCII("notes.tar.xz.partial");
  ASSERT_TRUE(base::WriteFile(fresh_archive, "fresh"));
  ASSERT_TRUE(base::WriteFile(expired_archive, "expired"));
  ASSERT_TRUE(base::WriteFile(fresh_partial, "fresh partial"));
  ASSERT_TRUE(base::WriteFile(expired_partial, "expired partial"));
  ASSERT_TRUE(base::WriteFile(unrelated, "unrelated"));
  const base::Time expired_time =
      base::Time::Now() - base::Seconds(kArchiveCacheValiditySeconds + 60);
  ASSERT_TRUE(base::TouchFile(expired_archive, expired_time, expired_time));
  ASSERT_TRUE(base::TouchFile(expired_partial, expired_time, expired_time));

  PruneCacheDirectory(cache_dir);

  EXPECT_TRUE(base::PathExists(fresh_archive));
  EXPECT_TRUE(base::PathExists(fresh_partial));
  EXPECT_FALSE(base::PathExists(expired_archive));
  EXPECT_FALSE(base::PathExists(expired_partial));
  EXPECT_TRUE(base::PathExists(unrelated));
}

TEST_F(InstallerDownloadTest, PruneArchiveTtlExactClockBoundaries) {
  const base::FilePath cache_dir =
      temp_dir_.GetPath().AppendASCII(".cache-exact-ttl");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const base::Time now = base::Time::UnixEpoch() + base::Days(30000);
  const base::TimeDelta ttl = base::Seconds(kArchiveCacheValiditySeconds);
  const base::FilePath just_before =
      cache_dir.AppendASCII(std::string(64, '1') + ".tar.xz");
  const base::FilePath exactly_at =
      cache_dir.AppendASCII(std::string(64, '2') + ".tar.xz");
  const base::FilePath just_after_archive =
      cache_dir.AppendASCII(std::string(64, '3') + ".tar.xz");
  const base::FilePath partial_destination =
      cache_dir.AppendASCII(std::string(64, '4') + ".tar.xz");
  const base::FilePath just_after_partial =
      internal::GetDownloadPartialPathForTesting(
          partial_destination, "https://one.example/archive",
          "https://redirect.example/archive");
  const base::FilePath future_partial =
      internal::GetDownloadPartialPathForTesting(
          partial_destination, "https://two.example/archive",
          "https://redirect.example/archive");
  const base::FilePath future_archive =
      cache_dir.AppendASCII(std::string(64, '5') + ".tar.xz");
  for (const auto& path :
       {just_before, exactly_at, just_after_archive, just_after_partial,
        future_partial, future_archive}) {
    ASSERT_TRUE(base::WriteFile(path, "cache"));
  }
  ASSERT_TRUE(base::TouchFile(just_before, now - ttl + base::Seconds(1),
                              now - ttl + base::Seconds(1)));
  ASSERT_TRUE(base::TouchFile(exactly_at, now - ttl, now - ttl));
  ASSERT_TRUE(base::TouchFile(just_after_archive, now - ttl - base::Seconds(1),
                              now - ttl - base::Seconds(1)));
  ASSERT_TRUE(base::TouchFile(just_after_partial, now - ttl - base::Seconds(1),
                              now - ttl - base::Seconds(1)));
  ASSERT_TRUE(base::TouchFile(future_partial, now + base::Seconds(1),
                              now + base::Seconds(1)));
  ASSERT_TRUE(base::TouchFile(future_archive, now + base::Seconds(1),
                              now + base::Seconds(1)));

  internal::PruneCacheDirectoryAtTimeForTesting(cache_dir, now);

  EXPECT_TRUE(base::PathExists(just_before));
  EXPECT_FALSE(base::PathExists(exactly_at));
  EXPECT_FALSE(base::PathExists(just_after_archive));
  EXPECT_FALSE(base::PathExists(just_after_partial));
  EXPECT_FALSE(base::PathExists(future_partial));
  EXPECT_FALSE(base::PathExists(future_archive));
}

TEST_F(InstallerDownloadTest,
       PruneDeletionFailureDoesNotBlockOtherSafeEntries) {
  const base::FilePath cache_dir =
      temp_dir_.GetPath().AppendASCII(".cache-locked-ttl");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));
  const base::Time now = base::Time::UnixEpoch() + base::Days(30000);
  const base::Time expired =
      now - base::Seconds(kArchiveCacheValiditySeconds + 1);
  const base::FilePath locked =
      cache_dir.AppendASCII(std::string(64, '5') + ".tar.xz");
  const base::FilePath removable =
      cache_dir.AppendASCII(std::string(64, '6') + ".tar.xz");
  ASSERT_TRUE(base::WriteFile(locked, "locked"));
  ASSERT_TRUE(base::WriteFile(removable, "removable"));
  ASSERT_TRUE(base::TouchFile(locked, expired, expired));
  ASSERT_TRUE(base::TouchFile(removable, expired, expired));
  HANDLE lock =
      ::CreateFileW(locked.value().c_str(), GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE(INVALID_HANDLE_VALUE, lock);

  internal::PruneCacheDirectoryAtTimeForTesting(cache_dir, now);

  EXPECT_TRUE(base::PathExists(locked));
  EXPECT_FALSE(base::PathExists(removable));
  ::CloseHandle(lock);
  internal::PruneCacheDirectoryAtTimeForTesting(cache_dir, now);
  EXPECT_FALSE(base::PathExists(locked));
}

TEST_F(InstallerDownloadTest, PruneCacheDirectoryReparseDoesNotTraverse) {
  const base::FilePath target = temp_dir_.GetPath().AppendASCII("cache-target");
  const base::FilePath junction =
      temp_dir_.GetPath().AppendASCII("cache-junction");
  ASSERT_TRUE(base::CreateDirectory(target));
  const base::FilePath marker = target.AppendASCII("marker.cache");
  ASSERT_TRUE(base::WriteFile(marker, "preserve"));
  const wchar_t quote = static_cast<wchar_t>(34);
  std::wstring command = L"cmd /c mklink /J ";
  command +=
      quote + junction.value() + quote + L" " + quote + target.value() + quote;
  ::_wsystem(command.c_str());
  if (!base::DirectoryExists(junction)) {
    GTEST_SKIP() << "Could not create junction point";
  }
  ASSERT_TRUE(IsReparsePoint(junction));

  internal::PruneCacheDirectoryAtTimeForTesting(junction, base::Time::Now());

  EXPECT_TRUE(base::PathExists(marker));
  EXPECT_TRUE(base::DirectoryExists(junction));
  EXPECT_FALSE(IsReparsePoint(junction));
}

TEST_F(InstallerDownloadTest,
       PartialReparseCleanupRemovesLinkWithoutTraversal) {
  const base::FilePath target =
      temp_dir_.GetPath().AppendASCII("partial-target");
  ASSERT_TRUE(base::CreateDirectory(target));
  const base::FilePath marker = target.AppendASCII("marker");
  ASSERT_TRUE(base::WriteFile(marker, "preserve"));
  const base::FilePath destination =
      temp_dir_.GetPath().AppendASCII(std::string(64, '7') + ".tar.xz");
  const base::FilePath partial = internal::GetDownloadPartialPathForTesting(
      destination, "https://one.example/archive",
      "https://redirect.example/archive");
  const wchar_t quote = static_cast<wchar_t>(34);
  std::wstring command = L"cmd /c mklink /J ";
  command +=
      quote + partial.value() + quote + L" " + quote + target.value() + quote;
  ::_wsystem(command.c_str());
  if (!base::DirectoryExists(partial)) {
    GTEST_SKIP() << "Could not create junction point";
  }
  ASSERT_TRUE(IsReparsePoint(partial));

  EXPECT_EQ(DownloadError::kSuccess, DiscardDownloadPartials(destination));
  EXPECT_TRUE(base::PathExists(marker));
  EXPECT_FALSE(base::PathExists(partial));
}

#endif

TEST_F(InstallerDownloadTest, PruneCacheDirectoryNonExistent) {
  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII("no_such_dir");

  // Should not crash.
  PruneCacheDirectory(cache_dir);
}

// ============================================================================
// Local Download Path Tests
// ============================================================================

TEST_F(InstallerDownloadTest, LocalDownloadPathDownloadToString) {
  // Create a local directory with a file mimicking CDN layout.
  base::FilePath local_dir = temp_dir_.GetPath().AppendASCII("local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_dir));
  ASSERT_TRUE(base::WriteFile(local_dir.AppendASCII("manifest.json"),
                              R"({"version": "1.0"})"));

  DownloadOptions options;
  options.local_download_path = local_dir;

  std::string content;
  DownloadError result = DownloadToString(
      "https://cdn.example.com/path/manifest.json", &content, options);

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(R"({"version": "1.0"})", content);
}

TEST_F(InstallerDownloadTest, LocalDownloadPathDownloadFile) {
  base::FilePath local_dir = temp_dir_.GetPath().AppendASCII("local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_dir));
  std::string archive_content(1000, 'A');
  ASSERT_TRUE(base::WriteFile(local_dir.AppendASCII("archive.tar.xz"),
                              archive_content));

  DownloadOptions options;
  options.local_download_path = local_dir;

  base::FilePath dest = temp_dir_.GetPath().AppendASCII("downloaded.tar.xz");
  DownloadError result =
      DownloadFile("https://cdn.example.com/archive.tar.xz", dest, options);

  EXPECT_EQ(DownloadError::kSuccess, result);
  std::string downloaded;
  ASSERT_TRUE(base::ReadFileToString(dest, &downloaded));
  EXPECT_EQ(archive_content, downloaded);
}

TEST_F(InstallerDownloadTest, LocalDownloadPathFileMissing) {
  base::FilePath local_dir = temp_dir_.GetPath().AppendASCII("local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_dir));

  DownloadOptions options;
  options.local_download_path = local_dir;

  std::string content;
  DownloadError result = DownloadToString(
      "https://cdn.example.com/nonexistent.json", &content, options);

  EXPECT_EQ(DownloadError::kNetworkError, result);
}

TEST_F(InstallerDownloadTest, LocalDownloadPathSizeLimitExceeded) {
  base::FilePath local_dir = temp_dir_.GetPath().AppendASCII("local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_dir));
  std::string large_content(2000, 'X');
  ASSERT_TRUE(
      base::WriteFile(local_dir.AppendASCII("large.bin"), large_content));

  DownloadOptions options;
  options.local_download_path = local_dir;
  options.max_download_size = 1024;  // 1 KB limit

  std::string content;
  DownloadError result =
      DownloadToString("https://cdn.example.com/large.bin", &content, options);

  EXPECT_EQ(DownloadError::kFileTooLarge, result);
}

TEST_F(InstallerDownloadTest, LocalDownloadPathProgressCallback) {
  base::FilePath local_dir = temp_dir_.GetPath().AppendASCII("local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_dir));
  std::string content(10000, 'Z');
  ASSERT_TRUE(base::WriteFile(local_dir.AppendASCII("data.bin"), content));

  int callback_count = 0;
  DownloadOptions options;
  options.local_download_path = local_dir;
  options.progress_callback = base::BindRepeating(
      [](int* count, uint64_t downloaded, uint64_t total) -> bool {
        (*count)++;
        return true;
      },
      &callback_count);

  base::FilePath dest = temp_dir_.GetPath().AppendASCII("data.bin");
  DownloadError result =
      DownloadFile("https://cdn.example.com/data.bin", dest, options);

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_GT(callback_count, 0);
}

TEST_F(InstallerDownloadTest, LocalDownloadPathCancellation) {
  base::FilePath local_dir = temp_dir_.GetPath().AppendASCII("local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_dir));
  std::string content(10000, 'C');
  ASSERT_TRUE(base::WriteFile(local_dir.AppendASCII("data.bin"), content));

  DownloadOptions options;
  options.local_download_path = local_dir;
  options.progress_callback =
      base::BindRepeating([](uint64_t, uint64_t) -> bool { return false; });

  base::FilePath dest = temp_dir_.GetPath().AppendASCII("data.bin");
  DownloadError result =
      DownloadFile("https://cdn.example.com/data.bin", dest, options);

  EXPECT_EQ(DownloadError::kCancelled, result);
}

TEST_F(InstallerDownloadTest, LocalDownloadPathHeadRequestReturnsModified) {
  base::FilePath local_dir = temp_dir_.GetPath().AppendASCII("local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_dir));

  DownloadOptions options;
  options.local_download_path = local_dir;

  HeadResult result = HeadRequest("https://cdn.example.com/test.json",
                                  base::Time::Now(), options);

  EXPECT_EQ(HeadResult::kModified, result);
}

TEST_F(InstallerDownloadTest, LocalDownloadPathDoesNotPopulateCache) {
  base::FilePath local_dir = temp_dir_.GetPath().AppendASCII("local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_dir));
  ASSERT_TRUE(base::WriteFile(local_dir.AppendASCII("manifest.json"),
                              R"({"version": "1.0"})"));

  base::FilePath cache_dir = temp_dir_.GetPath().AppendASCII(".cache");
  ASSERT_TRUE(base::CreateDirectory(cache_dir));

  DownloadOptions options;
  options.local_download_path = local_dir;

  std::string url = "https://cdn.example.com/manifest.json";
  std::string content;
  DownloadError result = DownloadWithCache(url, cache_dir, &content, options);

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ(R"({"version": "1.0"})", content);

  // Cache file must NOT be created — local manifests contain filenames
  // that don't exist on CDN.
  base::FilePath cache_path = GetCacheFilePath(cache_dir, url);
  EXPECT_FALSE(base::PathExists(cache_path));
}

TEST_F(InstallerDownloadTest, LocalDownloadPathExtractsFilenameFromUrl) {
  // Verify that only the last path component is used as the filename.
  base::FilePath local_dir = temp_dir_.GetPath().AppendASCII("local_cdn");
  ASSERT_TRUE(base::CreateDirectory(local_dir));
  ASSERT_TRUE(
      base::WriteFile(local_dir.AppendASCII("file.json"), "file content"));

  DownloadOptions options;
  options.local_download_path = local_dir;

  std::string content;
  DownloadError result = DownloadToString(
      "https://cdn.example.com/deep/nested/path/file.json", &content, options);

  EXPECT_EQ(DownloadError::kSuccess, result);
  EXPECT_EQ("file content", content);
}

}  // namespace
}  // namespace cef_installer
