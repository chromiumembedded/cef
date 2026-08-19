// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <vector>

#include "cef/include/cef_base.h"
#include "testing/gtest/include/gtest/gtest.h"

// The Wayland embedding work appended |parent_xdg_surface| to
// cef_window_info_t under CEF_API_ADDED(CEF_EXPERIMENTAL). Appending keeps the
// offsets of every older member, but that is only half of ABI safety: an
// application built against a smaller struct passes a smaller allocation, and
// |size| is the only thing that says so at runtime. Copying the new member
// without consulting it reads past the end of what the caller owns.
//
// These tests pin that down, because the failure is silent on a good day: the
// value read is whatever the application happened to place after the struct,
// and it ends up in a field CEF later treats as a pointer.

namespace {

#if CEF_API_ADDED(CEF_EXPERIMENTAL)

// Size of cef_window_info_t as it was before |parent_xdg_surface| was added.
constexpr size_t kSizeBeforeWaylandMembers =
    offsetof(cef_window_info_t, parent_xdg_surface);

// A cef_window_info_t truncated to an older layout, with a recognisable
// pattern in the memory immediately after it. That pattern stands in for
// whatever an application has next to the struct on its stack: if the copy
// reads past |size|, it is what lands in the target.
class TruncatedWindowInfo {
 public:
  static constexpr uintptr_t kPoison = 0x0badc0de0badc0deULL;

  TruncatedWindowInfo() : storage_(sizeof(cef_window_info_t) * 2, 0) {
    memset(storage_.data() + kSizeBeforeWaylandMembers, 0xde,
           storage_.size() - kSizeBeforeWaylandMembers);
    auto* poison = reinterpret_cast<uintptr_t*>(storage_.data() +
                                                kSizeBeforeWaylandMembers);
    *poison = kPoison;

    get()->size = kSizeBeforeWaylandMembers;
  }

  cef_window_info_t* get() {
    return reinterpret_cast<cef_window_info_t*>(storage_.data());
  }

 private:
  std::vector<uint8_t> storage_;
};

TEST(CefWindowInfoLinuxTest, CopyingAnOlderStructDoesNotReadPastIt) {
  TruncatedWindowInfo older;
  older.get()->bounds = {1, 2, 3, 4};
  older.get()->parent_window = 0x1234;

  CefWindowInfo target;
  target = *older.get();

  // The members the older application does have are copied.
  EXPECT_EQ(target.bounds.x, 1);
  EXPECT_EQ(target.bounds.width, 3);
  EXPECT_EQ(target.parent_window, static_cast<cef_window_handle_t>(0x1234));

  // The one it does not have is cleared, not filled with what followed it.
  EXPECT_EQ(target.parent_xdg_surface, nullptr);
  EXPECT_NE(reinterpret_cast<uintptr_t>(target.parent_xdg_surface),
            TruncatedWindowInfo::kPoison);
}

// A target that has already been used must not keep a stale value when a
// smaller source is assigned over it. Otherwise a second CreateBrowser() call
// from an older application would inherit the previous browser's xdg_surface.
TEST(CefWindowInfoLinuxTest, CopyingAnOlderStructClearsWhatItCannotSet) {
  CefWindowInfo target;
  auto* sentinel = reinterpret_cast<cef_xdg_surface_handle_t>(0x4321);
  target.parent_xdg_surface = sentinel;
  ASSERT_EQ(target.parent_xdg_surface, sentinel);

  TruncatedWindowInfo older;
  target = *older.get();

  EXPECT_EQ(target.parent_xdg_surface, nullptr);
}

// The current layout round-trips in full, so the guard above does not cost
// anything to an application built against this version.
TEST(CefWindowInfoLinuxTest, CopyingACurrentStructKeepsEveryMember) {
  CefWindowInfo source;
  source.bounds = {5, 6, 7, 8};
  source.parent_window = 0xabcd;
  source.parent_xdg_surface = reinterpret_cast<cef_xdg_surface_handle_t>(0x99);

  CefWindowInfo target;
  target = source;

  EXPECT_EQ(target.bounds.y, 6);
  EXPECT_EQ(target.parent_window, static_cast<cef_window_handle_t>(0xabcd));
  EXPECT_EQ(target.parent_xdg_surface,
            reinterpret_cast<cef_xdg_surface_handle_t>(0x99));
}

// The three-argument overload is the whole API surface an embedder touches for
// this; it must not disturb anything the two-argument one sets.
TEST(CefWindowInfoLinuxTest, SetAsChildOverloadsAgreeOnTheSharedMembers) {
  const CefRect bounds(10, 20, 30, 40);
  auto* xdg = reinterpret_cast<cef_xdg_surface_handle_t>(0x77);

  CefWindowInfo two_arg;
  two_arg.SetAsChild(0x55, bounds);

  CefWindowInfo three_arg;
  three_arg.SetAsChild(0x55, xdg, bounds);

  EXPECT_EQ(two_arg.parent_window, three_arg.parent_window);
  EXPECT_EQ(two_arg.bounds.x, three_arg.bounds.x);
  EXPECT_EQ(two_arg.bounds.height, three_arg.bounds.height);

  // Only the new member differs, and the two-argument form leaves it null so
  // that existing embedders keep the old behaviour exactly.
  EXPECT_EQ(two_arg.parent_xdg_surface, nullptr);
  EXPECT_EQ(three_arg.parent_xdg_surface, xdg);
}

#endif  // CEF_API_ADDED(CEF_EXPERIMENTAL)

}  // namespace
