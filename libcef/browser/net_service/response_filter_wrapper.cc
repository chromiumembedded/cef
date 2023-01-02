// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/net_service/response_filter_wrapper.h"

#include <queue>

#include "base/logging.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "mojo/public/cpp/system/string_data_source.h"

namespace net_service {

namespace {

// Match the default |capacity_num_bytes| value from mojo::Core::CreateDataPipe.
static const size_t kBufferSize = 64 * 1024;  // 64 Kbytes.
static const size_t kMinBufferSpace = 1024;   // 1 Kbytes.

class ResponseFilterWrapper {
 public:
  ResponseFilterWrapper(CefRefPtr<CefResponseFilter> filter,
                        mojo::ScopedDataPipeConsumerHandle source_handle,
                        base::OnceClosure error_callback);

  ResponseFilterWrapper(const ResponseFilterWrapper&) = delete;
  ResponseFilterWrapper& operator=(const ResponseFilterWrapper&) = delete;

  // Creates and returns the output handle, or |source_handle| on failure.
  bool CreateOutputHandle(mojo::ScopedDataPipeConsumerHandle* output_handle);

 private:
  void OnSourceReadable(MojoResult, const mojo::HandleSignalsState&);
  void Filter(const char* data, size_t size);
  void Write(std::unique_ptr<std::string> data);
  void OnWriteComplete(std::unique_ptr<std::string>, MojoResult result);
  void Drain(bool complete);
  void MaybeSuccess();
  void Cleanup(bool success);

  CefRefPtr<CefResponseFilter> filter_;
  mojo::ScopedDataPipeConsumerHandle source_handle_;
  base::OnceClosure error_callback_;

  std::unique_ptr<mojo::DataPipeProducer> forwarder_;
  mojo::SimpleWatcher source_watcher_;

  bool read_pending_ = false;
  bool write_pending_ = false;
  std::queue<std::unique_ptr<std::string>> pending_data_;
  cef_response_filter_status_t last_status_ = RESPONSE_FILTER_NEED_MORE_DATA;
};

ResponseFilterWrapper::ResponseFilterWrapper(
    CefRefPtr<CefResponseFilter> filter,
    mojo::ScopedDataPipeConsumerHandle source_handle,
    base::OnceClosure error_callback)
    : filter_(filter),
      source_handle_(std::move(source_handle)),
      error_callback_(std::move(error_callback)),
      source_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

bool ResponseFilterWrapper::CreateOutputHandle(
    mojo::ScopedDataPipeConsumerHandle* output_handle) {
  if (!filter_->InitFilter()) {
    *output_handle = std::move(source_handle_);
    return false;
  }

  mojo::ScopedDataPipeProducerHandle forwarding_handle;
  if (CreateDataPipe(nullptr, forwarding_handle, *output_handle) !=
      MOJO_RESULT_OK) {
    *output_handle = std::move(source_handle_);
    return false;
  }

  forwarder_ =
      std::make_unique<mojo::DataPipeProducer>(std::move(forwarding_handle));

  source_watcher_.Watch(
      source_handle_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&ResponseFilterWrapper::OnSourceReadable,
                          base::Unretained(this)));
  source_watcher_.ArmOrNotify();
  read_pending_ = true;

  return true;
}

void ResponseFilterWrapper::OnSourceReadable(MojoResult,
                                             const mojo::HandleSignalsState&) {
  const void* buffer = nullptr;
  uint32_t read_bytes = 0;
  MojoResult result = source_handle_->BeginReadData(&buffer, &read_bytes,
                                                    MOJO_READ_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    source_watcher_.ArmOrNotify();
    return;
  }

  if (result != MOJO_RESULT_OK) {
    // Whole body has been read, or something went wrong.
    Drain(result == MOJO_RESULT_FAILED_PRECONDITION);
    return;
  }

  Filter(static_cast<const char*>(buffer), read_bytes);
  if (last_status_ == RESPONSE_FILTER_ERROR) {
    // Something went wrong.
    Drain(false);
    return;
  }

  source_handle_->EndReadData(read_bytes);
  source_watcher_.ArmOrNotify();
}

void ResponseFilterWrapper::Filter(const char* data, size_t size) {
  size_t data_in_size = size;
  auto data_in_ptr = data_in_size > 0 ? data : nullptr;

  size_t data_out_offset = 0;
  std::unique_ptr<std::string> data_out;

  while (true) {
    size_t data_in_read = 0;

    if (!data_out) {
      // Start a new buffer. Should have no offset to begin with.
      DCHECK_EQ(0U, data_out_offset);
      data_out = std::make_unique<std::string>();
      data_out->resize(kBufferSize);
    }

    auto data_out_ptr = data_out->data() + data_out_offset;
    size_t data_out_size = kBufferSize - data_out_offset;
    size_t data_out_written = 0;

    last_status_ = filter_->Filter(
        const_cast<char*>(data_in_ptr), data_in_size, data_in_read,
        const_cast<char*>(data_out_ptr), data_out_size, data_out_written);
    if (last_status_ == RESPONSE_FILTER_ERROR) {
      break;
    }

    // Validate the out values.
    if (data_in_read > data_in_size) {
      LOG(ERROR) << "potential buffer overflow; data_in_read > data_in_size";
      last_status_ = RESPONSE_FILTER_ERROR;
      break;
    }
    if (data_out_written > data_out_size) {
      LOG(ERROR)
          << "potential buffer overflow; data_out_written > data_out_size";
      last_status_ = RESPONSE_FILTER_ERROR;
      break;
    }
    if (data_out_written == 0 && data_in_read != data_in_size) {
      LOG(ERROR) << "when no data is written all input must be consumed; "
                    "data_out_written == 0 && data_in_read != data_in_size";
      last_status_ = RESPONSE_FILTER_ERROR;
      break;
    }

    if (data_out_written > 0) {
      data_out_offset += data_out_written;
      if (data_out_offset > kBufferSize - kMinBufferSpace) {
        // The buffer is full or almost full. Write the data that we've
        // received so far and start a new buffer.
        data_out->resize(data_out_offset);
        Write(std::move(data_out));
        data_out_offset = 0;
      }
    }

    if (data_in_read < data_in_size) {
      // Keep going until the user reads all data.
      data_in_ptr += data_in_read;
      data_in_size -= data_in_read;
      continue;
    }

    // At this point the user has read all data...
    if (data_in_ptr) {
      // Clear the input buffer.
      data_in_read = data_in_size = 0;
      data_in_ptr = nullptr;
    }

    if (data_out_written == data_out_size &&
        last_status_ == RESPONSE_FILTER_NEED_MORE_DATA) {
      // Output buffer was filled, but data is still pending.
      continue;
    }

    if (data_out_offset > 0) {
      // Write the last of the data that we've received.
      data_out->resize(data_out_offset);
      Write(std::move(data_out));
    }

    break;
  }
}

void ResponseFilterWrapper::Write(std::unique_ptr<std::string> data) {
  if (write_pending_) {
    // Only one write at a time is supported.
    pending_data_.push(std::move(data));
    return;
  }

  write_pending_ = true;

  base::StringPiece string_piece(*data);
  forwarder_->Write(std::make_unique<mojo::StringDataSource>(
                        string_piece, mojo::StringDataSource::AsyncWritingMode::
                                          STRING_STAYS_VALID_UNTIL_COMPLETION),
                    base::BindOnce(&ResponseFilterWrapper::OnWriteComplete,
                                   base::Unretained(this), std::move(data)));
}

void ResponseFilterWrapper::OnWriteComplete(std::unique_ptr<std::string>,
                                            MojoResult result) {
  write_pending_ = false;

  if (result != MOJO_RESULT_OK) {
    // Something went wrong.
    Cleanup(false);
    return;
  }

  MaybeSuccess();
}

void ResponseFilterWrapper::Drain(bool complete) {
  read_pending_ = false;
  source_handle_.reset();
  source_watcher_.Cancel();

  if (!complete) {
    // Something went wrong.
    Cleanup(false);
    return;
  }

  if (last_status_ == RESPONSE_FILTER_NEED_MORE_DATA) {
    // Let the user write any remaining data.
    Filter(nullptr, 0);
    if (last_status_ != RESPONSE_FILTER_DONE) {
      // Something went wrong.
      Cleanup(false);
      return;
    }
  }

  MaybeSuccess();
}

void ResponseFilterWrapper::MaybeSuccess() {
  if (!write_pending_ && !pending_data_.empty()) {
    // Write the next data segment.
    auto next = std::move(pending_data_.front());
    pending_data_.pop();
    Write(std::move(next));
    return;
  }

  if (!read_pending_ && !write_pending_) {
    Cleanup(true);
  }
}

void ResponseFilterWrapper::Cleanup(bool success) {
  if (!success && error_callback_) {
    std::move(error_callback_).Run();
  }
  delete this;
}

}  // namespace

mojo::ScopedDataPipeConsumerHandle CreateResponseFilterHandler(
    CefRefPtr<CefResponseFilter> filter,
    mojo::ScopedDataPipeConsumerHandle source_handle,
    base::OnceClosure error_callback) {
  // |filter_wrapper| will delete itself when filtering is complete if
  // CreateOutputHandle returns true. Otherwise, it will return the
  // original |source_handle|.
  auto filter_wrapper = new ResponseFilterWrapper(
      filter, std::move(source_handle), std::move(error_callback));
  mojo::ScopedDataPipeConsumerHandle output_handle;
  if (!filter_wrapper->CreateOutputHandle(&output_handle)) {
    delete filter_wrapper;
  }
  return output_handle;
}

}  // namespace net_service
