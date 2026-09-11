// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <memory>

#include "include/cef_parser.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/osr_accessibility_helper.h"
#include "tests/shared/browser/osr_accessibility_node.h"

// Exercise the legacy NSAccessibility entry points implemented by the sample.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Observe actual object deallocation without retaining the accessibility
// object.
@interface OsrAXLifetimeObserver : NSObject {
 @public
  int* destruction_count_;
}
@end

@implementation OsrAXLifetimeObserver
- (void)dealloc {
  ++*destruction_count_;
#if !__has_feature(objc_arc)
  [super dealloc];
#endif
}
@end

namespace {

void ObserveLifetime(NSObject* object, int* count) {
  static char key;
  OsrAXLifetimeObserver* observer = [[OsrAXLifetimeObserver alloc] init];
  observer->destruction_count_ = count;
  objc_setAssociatedObject(object, &key, observer, OBJC_ASSOCIATION_RETAIN);
#if !__has_feature(objc_arc)
  [observer release];
#endif
}

client::OsrAXNode* CreateNode(int id = 1) {
  auto value = CefDictionaryValue::Create();
  value->SetString("role", "button");
  auto attributes = CefDictionaryValue::Create();
  attributes->SetString("name", "Test button");
  value->SetDictionary("attributes", attributes);
  return client::OsrAXNode::CreateNode("test-tree", id, value, nullptr);
}

NSObject* NativeObject(client::OsrAXNode* node,
                       client::OsrAXNode* parent = nullptr) {
  return CAST_CEF_NATIVE_ACCESSIBLE_TO_NSOBJECT(
      node->GetNativeAccessibleObject(parent));
}

void NativeObjectSurvivesAutoreleasePoolsImpl() {
  int destroyed = 0;
  auto* node = CreateNode();
  for (int i = 0; i < 3; ++i) {
    @autoreleasepool {
      NSObject* object = NativeObject(node);
      if (i == 0) {
        ObserveLifetime(object, &destroyed);
      }
      EXPECT_TRUE([NSAccessibilityButtonRole
          isEqual:[object accessibilityAttributeValue:
                              NSAccessibilityRoleAttribute]]);
      EXPECT_TRUE(
          [@"Test button" isEqual:[object accessibilityAttributeValue:
                                              NSAccessibilityTitleAttribute]]);
    }
    // Without ownership, the first pool drain frees the cached native object.
    ASSERT_EQ(0, destroyed);
  }
  @autoreleasepool {
    node->Destroy();
  }
  EXPECT_EQ(1, destroyed);
}

void RetainedNativeObjectDetachesImpl() {
  int destroyed = 0;
  CFTypeRef retained;
  @autoreleasepool {
    auto* node = CreateNode();
    NSObject* object = NativeObject(node);
    ObserveLifetime(object, &destroyed);
    retained = CFRetain((__bridge CFTypeRef)object);
    node->Destroy();
  }
  EXPECT_EQ(0, destroyed);
  @autoreleasepool {
    NSObject* object = (__bridge NSObject*)retained;
    EXPECT_TRUE([object accessibilityIsIgnored]);
    for (NSString* attribute in [object accessibilityAttributeNames]) {
      EXPECT_EQ(nil, [object accessibilityAttributeValue:attribute]);
    }
    EXPECT_EQ(nil, [object accessibilityFocusedUIElement]);
    EXPECT_EQ(nil, [object accessibilityHitTest:NSZeroPoint]);
    EXPECT_EQ(0U, [[object accessibilityActionNames] count]);
    EXPECT_EQ(
        nil,
        [object accessibilityActionDescription:NSAccessibilityPressAction]);
    [object accessibilityPerformAction:NSAccessibilityPressAction];
    [object accessibilityPerformAction:NSAccessibilityShowMenuAction];
    CFRelease(retained);
  }
  EXPECT_EQ(1, destroyed);
}

void NativeParentCanChangeImpl() {
  @autoreleasepool {
    auto* first = CreateNode(1);
    auto* second = CreateNode(2);
    auto* child = CreateNode(3);
    NSObject* first_object = NativeObject(first);
    NSObject* second_object = NativeObject(second);
    // A focus query can create the child before its parent is assigned.
    NSObject* child_object = NativeObject(child);
    EXPECT_EQ(child_object, NativeObject(child, first));
    EXPECT_EQ(first_object,
              [child_object
                  accessibilityAttributeValue:NSAccessibilityParentAttribute]);
    EXPECT_EQ(child_object, NativeObject(child, second));
    first->Destroy();
    EXPECT_EQ(second_object,
              [child_object
                  accessibilityAttributeValue:NSAccessibilityParentAttribute]);
    child->SetParent(nullptr);
    second->Destroy();
    EXPECT_EQ(nil,
              [child_object
                  accessibilityAttributeValue:NSAccessibilityParentAttribute]);
    child->Destroy();
  }
}

void HelperTeardownDetachesAllNodesImpl() {
  // No browser is needed to verify role queries and teardown of rootless nodes.
  auto tree = CefParseJSON(R"({"ax_tree_id":"test-tree","updates":[{
    "nodes":[{"id":1,"role":"button"},{"id":2,"role":"button"}]
  }]})",
                           JSON_PARSER_RFC);
  auto helper = std::make_unique<client::OsrAccessibilityHelper>(tree, nullptr);
  auto* first = helper->GetNode("test-tree", 1);
  auto* second = helper->GetNode("test-tree", 2);
  ASSERT_NE(nullptr, first);
  ASSERT_NE(nullptr, second);
  int destroyed = 0;
  CFTypeRef retained;
  @autoreleasepool {
    // Construction does not query the browser/window; parent resolution is
    // lazy.
    NSObject* object = NativeObject(first);
    ObserveLifetime(object, &destroyed);
    retained = CFRetain((__bridge CFTypeRef)object);
    ObserveLifetime(NativeObject(second), &destroyed);
    helper.reset();
  }
  EXPECT_EQ(1, destroyed);
  @autoreleasepool {
    NSObject* object = (__bridge NSObject*)retained;
    EXPECT_TRUE([object accessibilityIsIgnored]);
    EXPECT_EQ(
        nil, [object accessibilityAttributeValue:NSAccessibilityRoleAttribute]);
    CFRelease(retained);
  }
  EXPECT_EQ(2, destroyed);
}

}  // namespace

UI_THREAD_TEST(OsrAccessibilityTest, NativeObjectSurvivesAutoreleasePools)
UI_THREAD_TEST(OsrAccessibilityTest, RetainedNativeObjectDetaches)
UI_THREAD_TEST(OsrAccessibilityTest, NativeParentCanChange)
UI_THREAD_TEST(OsrAccessibilityTest, HelperTeardownDetachesAllNodes)

#pragma clang diagnostic pop
