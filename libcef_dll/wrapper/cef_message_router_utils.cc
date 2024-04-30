// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef_dll/wrapper/cef_message_router_utils.h"

#include <type_traits>

#include "include/cef_shared_process_message_builder.h"

namespace cef_message_router_utils {

namespace {

constexpr int kNoError = 0;

constexpr size_t kContextId = 0;
constexpr size_t kRequestId = 1;
constexpr size_t kRendererPayload = 2;
constexpr size_t kIsSuccess = 2;
constexpr size_t kBrowserPayload = 3;
constexpr size_t kIsPersistent = 3;

struct BrowserMsgHeader {
  int context_id;
  int request_id;
  bool is_binary;
};

static_assert(
    std::is_trivially_copyable_v<BrowserMsgHeader>,
    "Copying non-trivially-copyable object across memory spaces is dangerous");

struct RendererMsgHeader {
  int context_id;
  int request_id;
  bool is_persistent;
  bool is_binary;
};

static_assert(
    std::is_trivially_copyable_v<RendererMsgHeader>,
    "Copying non-trivially-copyable object across memory spaces is dangerous");

//
// This is a workaround for handling empty CefBinaryValues, as it's not possible
// to create an empty one directly. We use this empty struct as a tag to invoke
// the SetNull function within the BuildBrowserListMsg and BuildRendererListMsg
// functions.
//
struct Empty {};

size_t GetByteLength(const CefString& value) {
  return value.size() * sizeof(CefString::char_type);
}

size_t GetByteLength(const CefRefPtr<CefV8Value>& value) {
  return value->GetArrayBufferByteLength();
}

const CefString& GetListRepresentation(const CefString& value) {
  return value;
}

CefRefPtr<CefBinaryValue> GetListRepresentation(
    const CefRefPtr<CefV8Value>& value) {
  return CefBinaryValue::Create(value->GetArrayBufferData(),
                                value->GetArrayBufferByteLength());
}

template <class Header, class T>
size_t GetMessageSize(const T& value) {
  return sizeof(Header) + GetByteLength(value);
}

template <class Header>
void CopyIntoMemory(void* memory, const void* data, size_t bytes) {
  if (bytes > 0) {
    void* dest = static_cast<uint8_t*>(memory) + sizeof(Header);
    memcpy(dest, data, bytes);
  }
}

template <class Header>
void CopyIntoMemory(void* memory, const CefRefPtr<CefV8Value>& value) {
  CopyIntoMemory<Header>(memory, value->GetArrayBufferData(),
                         value->GetArrayBufferByteLength());
}

template <class Header>
void CopyIntoMemory(void* memory, const CefString& value) {
  const size_t bytes = GetByteLength(value);
  CopyIntoMemory<Header>(memory, value.c_str(), bytes);
}

template <class Header>
CefString GetStringFromMemory(const void* memory, size_t size) {
  const size_t bytes = size - sizeof(Header);
  const size_t string_len = bytes / sizeof(CefString::char_type);
  const void* string_data =
      static_cast<const uint8_t*>(memory) + sizeof(Header);
  const CefString::char_type* src =
      static_cast<const CefString::char_type*>(string_data);
  CefString result;
  result.FromString(src, string_len, /*copy=*/true);
  return result;
}

template <typename T>
constexpr bool IsCefString() {
  return std::is_same_v<std::remove_cv_t<T>, CefString>;
}

template <typename T>
constexpr bool IsEmpty() {
  return std::is_same_v<std::remove_cv_t<T>, Empty>;
}

template <class ResponseType>
CefRefPtr<CefProcessMessage> BuildBrowserListMsg(const CefString& name,
                                                 int context_id,
                                                 int request_id,
                                                 const ResponseType& response) {
  auto message = CefProcessMessage::Create(name);
  CefRefPtr<CefListValue> args = message->GetArgumentList();
  args->SetInt(kContextId, context_id);
  args->SetInt(kRequestId, request_id);
  args->SetBool(kIsSuccess, true);

  if constexpr (IsCefString<ResponseType>()) {
    args->SetString(kBrowserPayload, response);
  } else if constexpr (IsEmpty<ResponseType>()) {
    args->SetNull(kBrowserPayload);
  } else {
    args->SetBinary(kBrowserPayload, response);
  }

  return message;
}

class EmptyResponseBuilder final : public BrowserResponseBuilder {
 public:
  explicit EmptyResponseBuilder(const std::string& name) : name_(name) {}
  EmptyResponseBuilder(const EmptyResponseBuilder&) = delete;
  EmptyResponseBuilder& operator=(const EmptyResponseBuilder&) = delete;

  CefRefPtr<CefProcessMessage> Build(int context_id, int request_id) override {
    return BuildBrowserListMsg(name_, context_id, request_id, Empty{});
  }

 private:
  const CefString name_;

  IMPLEMENT_REFCOUNTING(EmptyResponseBuilder);
};

class BinaryResponseBuilder final : public BrowserResponseBuilder {
 public:
  BinaryResponseBuilder(const std::string& name, const void* data, size_t size)
      : name_(name), value_(CefBinaryValue::Create(data, size)) {}
  BinaryResponseBuilder(const BinaryResponseBuilder&) = delete;
  BinaryResponseBuilder& operator=(const BinaryResponseBuilder&) = delete;

  CefRefPtr<CefProcessMessage> Build(int context_id, int request_id) override {
    return BuildBrowserListMsg(name_, context_id, request_id, value_);
  }

 private:
  const CefString name_;
  const CefRefPtr<CefBinaryValue> value_;

  IMPLEMENT_REFCOUNTING(BinaryResponseBuilder);
};

class StringResponseBuilder final : public BrowserResponseBuilder {
 public:
  StringResponseBuilder(const std::string& name, const CefString& value)
      : name_(name), value_(value) {}
  StringResponseBuilder(const StringResponseBuilder&) = delete;
  StringResponseBuilder& operator=(const StringResponseBuilder&) = delete;

  CefRefPtr<CefProcessMessage> Build(int context_id, int request_id) override {
    return BuildBrowserListMsg(name_, context_id, request_id, value_);
  }

 private:
  const CefString name_;
  const CefString value_;

  IMPLEMENT_REFCOUNTING(StringResponseBuilder);
};

// SharedProcessMessageResponseBuilder
class SPMResponseBuilder final : public BrowserResponseBuilder {
 public:
  SPMResponseBuilder(const SPMResponseBuilder&) = delete;
  SPMResponseBuilder& operator=(const SPMResponseBuilder&) = delete;

  static CefRefPtr<BrowserResponseBuilder> Create(const std::string& name,
                                                  const void* data,
                                                  size_t size) {
    const size_t message_size = sizeof(BrowserMsgHeader) + size;
    auto builder = CefSharedProcessMessageBuilder::Create(name, message_size);
    if (!builder->IsValid()) {
      LOG(ERROR) << "Failed to allocate shared memory region of size "
                 << message_size;
      return new BinaryResponseBuilder(name, data, size);
    }

    CopyIntoMemory<BrowserMsgHeader>(builder->Memory(), data, size);
    return new SPMResponseBuilder(builder, /*is_binary=*/true);
  }

  static CefRefPtr<BrowserResponseBuilder> Create(const std::string& name,
                                                  const CefString& value) {
    const size_t message_size = GetMessageSize<BrowserMsgHeader>(value);
    auto builder = CefSharedProcessMessageBuilder::Create(name, message_size);
    if (!builder->IsValid()) {
      LOG(ERROR) << "Failed to allocate shared memory region of size "
                 << message_size;
      return new StringResponseBuilder(name, value);
    }

    CopyIntoMemory<BrowserMsgHeader>(builder->Memory(), value);
    return new SPMResponseBuilder(builder, /*is_binary=*/false);
  }

  CefRefPtr<CefProcessMessage> Build(int context_id, int request_id) override {
    auto header = static_cast<BrowserMsgHeader*>(builder_->Memory());
    header->context_id = context_id;
    header->request_id = request_id;
    header->is_binary = is_binary_;
    return builder_->Build();
  }

 private:
  explicit SPMResponseBuilder(
      const CefRefPtr<CefSharedProcessMessageBuilder>& builder,
      bool is_binary)
      : builder_(builder), is_binary_(is_binary) {}

  CefRefPtr<CefSharedProcessMessageBuilder> builder_;
  const bool is_binary_;

  IMPLEMENT_REFCOUNTING(SPMResponseBuilder);
};

class EmptyBinaryBuffer final : public CefBinaryBuffer {
 public:
  EmptyBinaryBuffer() = default;
  EmptyBinaryBuffer(const EmptyBinaryBuffer&) = delete;
  EmptyBinaryBuffer& operator=(const EmptyBinaryBuffer&) = delete;

  const void* GetData() const override { return nullptr; }
  void* GetData() override { return nullptr; }
  size_t GetSize() const override { return 0; }

 private:
  IMPLEMENT_REFCOUNTING(EmptyBinaryBuffer);
};

class BinaryValueBuffer final : public CefBinaryBuffer {
 public:
  BinaryValueBuffer(CefRefPtr<CefProcessMessage> message,
                    CefRefPtr<CefBinaryValue> value)
      : message_(std::move(message)), value_(std::move(value)) {}
  BinaryValueBuffer(const BinaryValueBuffer&) = delete;
  BinaryValueBuffer& operator=(const BinaryValueBuffer&) = delete;

  const void* GetData() const override { return value_->GetRawData(); }
  void* GetData() override {
    // This is not UB as long as underlying storage is vector<uint8_t>
    return const_cast<void*>(value_->GetRawData());
  }
  size_t GetSize() const override { return value_->GetSize(); }

 private:
  const CefRefPtr<CefProcessMessage> message_;
  const CefRefPtr<CefBinaryValue> value_;

  IMPLEMENT_REFCOUNTING(BinaryValueBuffer);
};

class SharedMemoryRegionBuffer final : public CefBinaryBuffer {
 public:
  SharedMemoryRegionBuffer(const CefRefPtr<CefSharedMemoryRegion>& region,
                           size_t offset)
      : region_(region),
        data_(static_cast<uint8_t*>(region->Memory()) + offset),
        size_(region->Size() - offset) {}
  SharedMemoryRegionBuffer(const SharedMemoryRegionBuffer&) = delete;
  SharedMemoryRegionBuffer& operator=(const SharedMemoryRegionBuffer&) = delete;

  const void* GetData() const override { return data_; }
  void* GetData() override { return data_; }
  size_t GetSize() const override { return size_; }

 private:
  const CefRefPtr<CefSharedMemoryRegion> region_;
  void* const data_;
  const size_t size_;

  IMPLEMENT_REFCOUNTING(SharedMemoryRegionBuffer);
};

template <class RequestType>
CefRefPtr<CefProcessMessage> BuildRendererListMsg(const std::string& name,
                                                  int context_id,
                                                  int request_id,
                                                  const RequestType& request,
                                                  bool persistent) {
  auto message = CefProcessMessage::Create(name);
  CefRefPtr<CefListValue> args = message->GetArgumentList();
  args->SetInt(kContextId, context_id);
  args->SetInt(kRequestId, request_id);

  if constexpr (IsCefString<RequestType>()) {
    args->SetString(kRendererPayload, request);
  } else if constexpr (IsEmpty<RequestType>()) {
    args->SetNull(kRendererPayload);
  } else {
    args->SetBinary(kRendererPayload, request);
  }

  args->SetBool(kIsPersistent, persistent);
  return message;
}

template <class RequestType>
CefRefPtr<CefProcessMessage> BuildRendererSharedMsg(const std::string& name,
                                                    int context_id,
                                                    int request_id,
                                                    const RequestType& request,
                                                    bool persistent) {
  const size_t message_size = GetMessageSize<RendererMsgHeader>(request);
  auto builder = CefSharedProcessMessageBuilder::Create(name, message_size);
  if (!builder->IsValid()) {
    LOG(ERROR) << "Failed to allocate shared memory region of size "
               << message_size;
    return BuildRendererListMsg(name, context_id, request_id,
                                GetListRepresentation(request), persistent);
  }

  auto header = static_cast<RendererMsgHeader*>(builder->Memory());
  header->context_id = context_id;
  header->request_id = request_id;
  header->is_persistent = persistent;
  header->is_binary = !IsCefString<RequestType>();

  CopyIntoMemory<RendererMsgHeader>(builder->Memory(), request);

  return builder->Build();
}

CefRefPtr<CefProcessMessage> BuildRendererMsg(size_t threshold,
                                              const std::string& name,
                                              int context_id,
                                              int request_id,
                                              const CefString& request,
                                              bool persistent) {
  if (GetByteLength(request) < threshold) {
    return BuildRendererListMsg(name, context_id, request_id, request,
                                persistent);
  }

  return BuildRendererSharedMsg(name, context_id, request_id, request,
                                persistent);
}

}  // namespace

CefRefPtr<BrowserResponseBuilder> CreateBrowserResponseBuilder(
    size_t threshold,
    const std::string& name,
    const CefString& response) {
  if (GetByteLength(response) < threshold) {
    return new StringResponseBuilder(name, response);
  }

  return SPMResponseBuilder::Create(name, response);
}

CefRefPtr<BrowserResponseBuilder> CreateBrowserResponseBuilder(
    size_t threshold,
    const std::string& name,
    const void* data,
    size_t size) {
  if (size == 0) {
    return new EmptyResponseBuilder(name);
  }

  if (size < threshold) {
    return new BinaryResponseBuilder(name, data, size);
  }

  return SPMResponseBuilder::Create(name, data, size);
}

CefRefPtr<CefProcessMessage> BuildRendererMsg(
    size_t threshold,
    const std::string& name,
    int context_id,
    int request_id,
    const CefRefPtr<CefV8Value>& request,
    bool persistent) {
  if (request->IsString()) {
    return BuildRendererMsg(threshold, name, context_id, request_id,
                            request->GetStringValue(), persistent);
  }

  const auto size = request->GetArrayBufferByteLength();
  if (size == 0) {
    return BuildRendererListMsg(name, context_id, request_id, Empty{},
                                persistent);
  }

  if (size < threshold) {
    return BuildRendererListMsg(name, context_id, request_id,
                                GetListRepresentation(request), persistent);
  }

  return BuildRendererSharedMsg(name, context_id, request_id, request,
                                persistent);
}

BrowserMessage ParseBrowserMessage(
    const CefRefPtr<CefProcessMessage>& message) {
  if (auto args = message->GetArgumentList()) {
    DCHECK_GT(args->GetSize(), 3U);

    const int context_id = args->GetInt(kContextId);
    const int request_id = args->GetInt(kRequestId);
    const bool is_success = args->GetBool(kIsSuccess);

    if (is_success) {
      DCHECK_EQ(args->GetSize(), 4U);
      const auto payload_type = args->GetType(kBrowserPayload);
      if (payload_type == CefValueType::VTYPE_STRING) {
        return {context_id, request_id, is_success, kNoError,
                args->GetString(kBrowserPayload)};
      }

      if (payload_type == CefValueType::VTYPE_BINARY) {
        return {
            context_id, request_id, is_success, kNoError,
            new BinaryValueBuffer(message, args->GetBinary(kBrowserPayload))};
      }

      DCHECK(payload_type == CefValueType::VTYPE_NULL);
      return {context_id, request_id, is_success, kNoError,
              new EmptyBinaryBuffer()};
    }

    DCHECK_EQ(args->GetSize(), 5U);
    return {context_id, request_id, is_success, args->GetInt(3),
            args->GetString(4)};
  }

  const auto region = message->GetSharedMemoryRegion();
  if (region && region->IsValid()) {
    DCHECK_GE(region->Size(), sizeof(BrowserMsgHeader));
    auto header = static_cast<const BrowserMsgHeader*>(region->Memory());

    if (header->is_binary) {
      return {header->context_id, header->request_id, true, kNoError,
              new SharedMemoryRegionBuffer(region, sizeof(BrowserMsgHeader))};
    }
    return {header->context_id, header->request_id, true, kNoError,
            GetStringFromMemory<BrowserMsgHeader>(region->Memory(),
                                                  region->Size())};
  }

  NOTREACHED();
  return {};
}

RendererMessage ParseRendererMessage(
    const CefRefPtr<CefProcessMessage>& message) {
  if (auto args = message->GetArgumentList()) {
    DCHECK_EQ(args->GetSize(), 4U);

    const int context_id = args->GetInt(kContextId);
    const int request_id = args->GetInt(kRequestId);
    const auto payload_type = args->GetType(kRendererPayload);
    const bool persistent = args->GetBool(kIsPersistent);

    if (payload_type == CefValueType::VTYPE_STRING) {
      return {context_id, request_id, persistent,
              args->GetString(kRendererPayload)};
    }

    if (payload_type == CefValueType::VTYPE_BINARY) {
      return {
          context_id, request_id, persistent,
          new BinaryValueBuffer(message, args->GetBinary(kRendererPayload))};
    }

    DCHECK(payload_type == CefValueType::VTYPE_NULL);
    return {context_id, request_id, persistent, new EmptyBinaryBuffer()};
  }

  const auto region = message->GetSharedMemoryRegion();
  if (region && region->IsValid()) {
    DCHECK_GE(region->Size(), sizeof(RendererMsgHeader));
    auto header = static_cast<const RendererMsgHeader*>(region->Memory());

    if (header->is_binary) {
      return {header->context_id, header->request_id, header->is_persistent,
              new SharedMemoryRegionBuffer(region, sizeof(RendererMsgHeader))};
    }

    return {
        header->context_id,
        header->request_id,
        header->is_persistent,
        GetStringFromMemory<RendererMsgHeader>(region->Memory(),
                                               region->Size()),
    };
  }

  NOTREACHED();
  return {};
}

}  // namespace cef_message_router_utils
