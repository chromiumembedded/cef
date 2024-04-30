// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <array>
#include <type_traits>

#include "include/cef_shared_process_message_builder.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

constexpr bool kTestFlag = true;
constexpr int kTestValue = 42;
constexpr double kTestDoubleValue = 123.456;

struct TestData {
  bool flag = kTestFlag;
  int value = kTestValue;
  double doubleValue = kTestDoubleValue;
  std::array<size_t, 50> buffer{};
};

const char kSharedMessageName[] = "SharedProcessMessageTest";

CefRefPtr<CefSharedProcessMessageBuilder> CreateTestBuilder() {
  auto builder = CefSharedProcessMessageBuilder::Create(kSharedMessageName,
                                                        sizeof(TestData));
  EXPECT_NE(builder, nullptr);
  EXPECT_TRUE(builder->IsValid());

  static_assert(
      std::is_trivially_copyable_v<TestData>,
      "Do not copy non-trivially-copyable object across memory spaces");
  auto data = static_cast<TestData*>(builder->Memory());
  EXPECT_NE(data, nullptr);

  data->value = kTestValue;
  data->doubleValue = kTestDoubleValue;
  data->flag = kTestFlag;
  for (size_t i = 0; i < data->buffer.size(); ++i) {
    data->buffer[i] = i;
  }

  return builder;
}

}  // namespace

TEST(SharedProcessMessageTest, CanBuildSharedMessageUsingBuilder) {
  auto builder = CreateTestBuilder();

  auto message = builder->Build();
  EXPECT_FALSE(builder->IsValid());
  EXPECT_NE(message, nullptr);
  EXPECT_TRUE(message->IsValid());
  EXPECT_TRUE(message->IsReadOnly());

  auto region = message->GetSharedMemoryRegion();
  EXPECT_TRUE(region->IsValid());
  auto read_data = static_cast<const TestData*>(region->Memory());

  EXPECT_EQ(read_data->flag, kTestFlag);
  EXPECT_EQ(read_data->value, kTestValue);
  EXPECT_EQ(read_data->doubleValue, kTestDoubleValue);
  for (size_t i = 0; i < read_data->buffer.size(); ++i) {
    EXPECT_EQ(read_data->buffer[i], i);
  }
}

TEST(SharedProcessMessageTest, CopyingIsNotSupportedBySharedMessage) {
  auto builder = CefSharedProcessMessageBuilder::Create(kSharedMessageName,
                                                        sizeof(TestData));
  CefRefPtr<CefProcessMessage> message = builder->Build();
  CefRefPtr<CefProcessMessage> message_copy = message->Copy();
  EXPECT_EQ(message_copy, nullptr);
}

TEST(SharedProcessMessageTest,
     RegionRemainsValidAfterSharedMessageDestruction) {
  CefRefPtr<CefSharedMemoryRegion> region;
  {
    auto builder = CreateTestBuilder();
    auto message = builder->Build();
    region = message->GetSharedMemoryRegion();
  }

  EXPECT_TRUE(region->IsValid());
  auto read_data = static_cast<const TestData*>(region->Memory());

  EXPECT_EQ(read_data->flag, kTestFlag);
  EXPECT_EQ(read_data->value, kTestValue);
  EXPECT_EQ(read_data->doubleValue, kTestDoubleValue);
  for (size_t i = 0; i < read_data->buffer.size(); ++i) {
    EXPECT_EQ(read_data->buffer[i], i);
  }
}

TEST(SharedProcessMessageTest, WrittenValuesVisibleInOtherRegion) {
  CefRefPtr<CefSharedMemoryRegion> read_region;
  CefRefPtr<CefSharedMemoryRegion> write_region;
  {
    auto builder = CreateTestBuilder();
    auto message = builder->Build();
    read_region = message->GetSharedMemoryRegion();
    write_region = message->GetSharedMemoryRegion();
  }

  EXPECT_TRUE(write_region->IsValid());
  auto write_data = static_cast<TestData*>(write_region->Memory());
  write_data->flag = !kTestFlag;
  write_data->value = kTestValue * 2;
  auto new_double_value = kTestDoubleValue * 3;
  write_data->doubleValue = new_double_value;
  for (size_t i = 0; i < write_data->buffer.size(); ++i) {
    write_data->buffer[i] = i + 1;
  }

  EXPECT_TRUE(read_region->IsValid());
  auto read_data = static_cast<const TestData*>(read_region->Memory());
  EXPECT_EQ(read_data->flag, !kTestFlag);
  EXPECT_EQ(read_data->value, kTestValue * 2);
  EXPECT_EQ(read_data->doubleValue, new_double_value);
  for (size_t i = 0; i < read_data->buffer.size(); ++i) {
    EXPECT_EQ(read_data->buffer[i], i + 1);
  }
}
