// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

// Sample implementation for the NSAccessibility protocol for interacting with
// VoiceOver and other accessibility clients.

#include "tests/cefclient/browser/osr_accessibility_node.h"

#import <AppKit/NSAccessibility.h>
#import <Cocoa/Cocoa.h>

#include "tests/cefclient/browser/osr_accessibility_helper.h"

namespace {

NSString* AxRoleToNSAxRole(const std::string& role_string) {
  if (role_string == "abbr")
    return NSAccessibilityGroupRole;
  if (role_string == "alertDialog")
    return NSAccessibilityGroupRole;
  if (role_string == "alert")
    return NSAccessibilityGroupRole;
  if (role_string == "annotation")
    return NSAccessibilityUnknownRole;
  if (role_string == "application")
    return NSAccessibilityGroupRole;
  if (role_string == "article")
    return NSAccessibilityGroupRole;
  if (role_string == "audio")
    return NSAccessibilityGroupRole;
  if (role_string == "banner")
    return NSAccessibilityGroupRole;
  if (role_string == "blockquote")
    return NSAccessibilityGroupRole;
  if (role_string == "busyIndicator")
    return NSAccessibilityBusyIndicatorRole;
  if (role_string == "button")
    return NSAccessibilityButtonRole;
  if (role_string == "buttonDropDown")
    return NSAccessibilityButtonRole;
  if (role_string == "canvas")
    return NSAccessibilityImageRole;
  if (role_string == "caption")
    return NSAccessibilityGroupRole;
  if (role_string == "checkBox")
    return NSAccessibilityCheckBoxRole;
  if (role_string == "colorWell")
    return NSAccessibilityColorWellRole;
  if (role_string == "column")
    return NSAccessibilityColumnRole;
  if (role_string == "comboBox")
    return NSAccessibilityComboBoxRole;
  if (role_string == "complementary")
    return NSAccessibilityGroupRole;
  if (role_string == "contentInfo")
    return NSAccessibilityGroupRole;
  if (role_string == "definition")
    return NSAccessibilityGroupRole;
  if (role_string == "descriptionListDetail")
    return NSAccessibilityGroupRole;
  if (role_string == "descriptionList")
    return NSAccessibilityListRole;
  if (role_string == "descriptionListTerm")
    return NSAccessibilityGroupRole;
  if (role_string == "details")
    return NSAccessibilityGroupRole;
  if (role_string == "dialog")
    return NSAccessibilityGroupRole;
  if (role_string == "directory")
    return NSAccessibilityListRole;
  if (role_string == "disclosureTriangle")
    return NSAccessibilityDisclosureTriangleRole;
  if (role_string == "div")
    return NSAccessibilityGroupRole;
  if (role_string == "document")
    return NSAccessibilityGroupRole;
  if (role_string == "embeddedObject")
    return NSAccessibilityGroupRole;
  if (role_string == "figcaption")
    return NSAccessibilityGroupRole;
  if (role_string == "figure")
    return NSAccessibilityGroupRole;
  if (role_string == "footer")
    return NSAccessibilityGroupRole;
  if (role_string == "form")
    return NSAccessibilityGroupRole;
  if (role_string == "genericContainer")
    return NSAccessibilityGroupRole;
  if (role_string == "grid")
    return NSAccessibilityGroupRole;
  if (role_string == "group")
    return NSAccessibilityGroupRole;
  if (role_string == "iframe")
    return NSAccessibilityGroupRole;
  if (role_string == "iframePresentational")
    return NSAccessibilityGroupRole;
  if (role_string == "ignored")
    return NSAccessibilityUnknownRole;
  if (role_string == "imageMapLink")
    return NSAccessibilityLinkRole;
  if (role_string == "imageMap")
    return NSAccessibilityGroupRole;
  if (role_string == "image")
    return NSAccessibilityImageRole;
  if (role_string == "labelText")
    return NSAccessibilityGroupRole;
  if (role_string == "legend")
    return NSAccessibilityGroupRole;
  if (role_string == "link")
    return NSAccessibilityLinkRole;
  if (role_string == "listBoxOption")
    return NSAccessibilityStaticTextRole;
  if (role_string == "listBox")
    return NSAccessibilityListRole;
  if (role_string == "listItem")
    return NSAccessibilityGroupRole;
  if (role_string == "list")
    return NSAccessibilityListRole;
  if (role_string == "log")
    return NSAccessibilityGroupRole;
  if (role_string == "main")
    return NSAccessibilityGroupRole;
  if (role_string == "mark")
    return NSAccessibilityGroupRole;
  if (role_string == "marquee")
    return NSAccessibilityGroupRole;
  if (role_string == "math")
    return NSAccessibilityGroupRole;
  if (role_string == "menu")
    return NSAccessibilityMenuRole;
  if (role_string == "menuBar")
    return NSAccessibilityMenuBarRole;
  if (role_string == "menuButton")
    return NSAccessibilityButtonRole;
  if (role_string == "menuItem")
    return NSAccessibilityMenuItemRole;
  if (role_string == "menuItemCheckBox")
    return NSAccessibilityMenuItemRole;
  if (role_string == "menuItemRadio")
    return NSAccessibilityMenuItemRole;
  if (role_string == "menuListOption")
    return NSAccessibilityMenuItemRole;
  if (role_string == "menuListPopup")
    return NSAccessibilityUnknownRole;
  if (role_string == "meter")
    return NSAccessibilityProgressIndicatorRole;
  if (role_string == "navigation")
    return NSAccessibilityGroupRole;
  if (role_string == "note")
    return NSAccessibilityGroupRole;
  if (role_string == "outline")
    return NSAccessibilityOutlineRole;
  if (role_string == "paragraph")
    return NSAccessibilityGroupRole;
  if (role_string == "popUpButton")
    return NSAccessibilityPopUpButtonRole;
  if (role_string == "pre")
    return NSAccessibilityGroupRole;
  if (role_string == "presentational")
    return NSAccessibilityGroupRole;
  if (role_string == "progressIndicator")
    return NSAccessibilityProgressIndicatorRole;
  if (role_string == "radioButton")
    return NSAccessibilityRadioButtonRole;
  if (role_string == "radioGroup")
    return NSAccessibilityRadioGroupRole;
  if (role_string == "region")
    return NSAccessibilityGroupRole;
  if (role_string == "row")
    return NSAccessibilityRowRole;
  if (role_string == "ruler")
    return NSAccessibilityRulerRole;
  if (role_string == "scrollBar")
    return NSAccessibilityScrollBarRole;
  if (role_string == "search")
    return NSAccessibilityGroupRole;
  if (role_string == "searchBox")
    return NSAccessibilityTextFieldRole;
  if (role_string == "slider")
    return NSAccessibilitySliderRole;
  if (role_string == "sliderThumb")
    return NSAccessibilityValueIndicatorRole;
  if (role_string == "spinButton")
    return NSAccessibilityIncrementorRole;
  if (role_string == "splitter")
    return NSAccessibilitySplitterRole;
  if (role_string == "staticText")
    return NSAccessibilityStaticTextRole;
  if (role_string == "status")
    return NSAccessibilityGroupRole;
  if (role_string == "svgRoot")
    return NSAccessibilityGroupRole;
  if (role_string == "switch")
    return NSAccessibilityCheckBoxRole;
  if (role_string == "tabGroup")
    return NSAccessibilityTabGroupRole;
  if (role_string == "tabList")
    return NSAccessibilityTabGroupRole;
  if (role_string == "tabPanel")
    return NSAccessibilityGroupRole;
  if (role_string == "tab")
    return NSAccessibilityRadioButtonRole;
  if (role_string == "tableHeaderContainer")
    return NSAccessibilityGroupRole;
  if (role_string == "table")
    return NSAccessibilityTableRole;
  if (role_string == "textField")
    return NSAccessibilityTextFieldRole;
  if (role_string == "time")
    return NSAccessibilityGroupRole;
  if (role_string == "timer")
    return NSAccessibilityGroupRole;
  if (role_string == "toggleButton")
    return NSAccessibilityCheckBoxRole;
  if (role_string == "toolbar")
    return NSAccessibilityToolbarRole;
  if (role_string == "treeGrid")
    return NSAccessibilityTableRole;
  if (role_string == "treeItem")
    return NSAccessibilityRowRole;
  if (role_string == "tree")
    return NSAccessibilityOutlineRole;
  if (role_string == "unknown")
    return NSAccessibilityUnknownRole;
  if (role_string == "tooltip")
    return NSAccessibilityGroupRole;
  if (role_string == "video")
    return NSAccessibilityGroupRole;
  if (role_string == "window")
    return NSAccessibilityWindowRole;
  return [NSString stringWithUTF8String:role_string.c_str()];
}

inline int MiddleX(const CefRect& rect) {
  return rect.x + rect.width / 2;
}

inline int MiddleY(const CefRect& rect) {
  return rect.y + rect.height / 2;
}

}  // namespace

// OsrAXNodeObject is sample implementation for the NSAccessibility protocol
// for interacting with VoiceOver and other accessibility clients.
@interface OsrAXNodeObject : NSObject {
  // OsrAXNode* proxy object
  client::OsrAXNode* node_;
  CefNativeAccessible* parent_;
}

- (id)init:(client::OsrAXNode*)node;
+ (OsrAXNodeObject*)elementWithNode:(client::OsrAXNode*)node;
@end

@implementation OsrAXNodeObject
- (id)init:(client::OsrAXNode*)node {
  node_ = node;
  parent_ = node_->GetParentAccessibleObject();
  if (!parent_) {
    parent_ = node_->GetWindowHandle();
  }
  return self;
}

+ (OsrAXNodeObject*)elementWithNode:(client::OsrAXNode*)node {
  // We manage the release ourself
  return [[OsrAXNodeObject alloc] init:node];
}

- (BOOL)isEqual:(id)object {
  if ([object isKindOfClass:[OsrAXNodeObject self]]) {
    OsrAXNodeObject* other = object;
    return (node_ == other->node_);
  } else {
    return NO;
  }
}

// Utility methods to map AX information received from renderer
// to platform properties
- (NSString*)axRole {
  // Get the Role from CefAccessibilityHelper and Map to NSRole
  return AxRoleToNSAxRole(node_->AxRole());
}

- (NSString*)axDescription {
  std::string desc = node_->AxDescription();
  return [NSString stringWithUTF8String:desc.c_str()];
}

- (NSString*)axName {
  std::string desc = node_->AxName();
  return [NSString stringWithUTF8String:desc.c_str()];
}

- (NSString*)axValue {
  std::string desc = node_->AxValue();
  return [NSString stringWithUTF8String:desc.c_str()];
}

- (void)doMouseClick:(cef_mouse_button_type_t)type {
  CefRefPtr<CefBrowser> browser = node_->GetBrowser();
  if (browser) {
    CefMouseEvent mouse_event;
    const CefRect& rect = node_->AxLocation();
    mouse_event.x = MiddleX(rect);
    mouse_event.y = MiddleY(rect);

    mouse_event.modifiers = 0;
    browser->GetHost()->SendMouseClickEvent(mouse_event, type, false, 1);
    browser->GetHost()->SendMouseClickEvent(mouse_event, type, true, 1);
  }
}

- (NSMutableArray*)getKids {
  int numChilds = node_->GetChildCount();
  if (numChilds > 0) {
    NSMutableArray* kids = [NSMutableArray arrayWithCapacity:numChilds];
    for (int index = 0; index < numChilds; index++) {
      client::OsrAXNode* child = node_->ChildAtIndex(index);
      [kids addObject:child ? child->GetNativeAccessibleObject(node_) : nil];
    }
    return kids;
  }
  return nil;
}

- (NSPoint)position {
  CefRect cef_rect = node_->AxLocation();
  NSPoint origin = NSMakePoint(cef_rect.x, cef_rect.y);
  NSSize size = NSMakeSize(cef_rect.width, cef_rect.height);

  NSView* view = node_->GetWindowHandle();
  origin.y = NSHeight([view bounds]) - origin.y;
  NSPoint originInWindow = [view convertPoint:origin toView:nil];

  NSRect point_rect = NSMakeRect(originInWindow.x, originInWindow.y, 0, 0);
  NSPoint originInScreen =
      [[view window] convertRectToScreen:point_rect].origin;

  originInScreen.y = originInScreen.y - size.height;
  return originInScreen;
}

- (NSSize)size {
  CefRect cef_rect = node_->AxLocation();
  NSRect rect =
      NSMakeRect(cef_rect.x, cef_rect.y, cef_rect.width, cef_rect.height);
  NSView* view = node_->GetWindowHandle();
  rect = [[view window] convertRectToScreen:rect];
  return rect.size;
}

//
// accessibility protocol
//

// attributes

- (BOOL)accessibilityIsIgnored {
  return NO;
}

- (NSArray*)accessibilityAttributeNames {
  static NSArray* attributes = nil;
  if (attributes == nil) {
    attributes = [[NSArray alloc]
        initWithObjects:NSAccessibilityRoleAttribute,
                        NSAccessibilityRoleDescriptionAttribute,
                        NSAccessibilityChildrenAttribute,
                        NSAccessibilityValueAttribute,
                        NSAccessibilityTitleAttribute,
                        NSAccessibilityDescriptionAttribute,
                        NSAccessibilityFocusedAttribute,
                        NSAccessibilityParentAttribute,
                        NSAccessibilityWindowAttribute,
                        NSAccessibilityTopLevelUIElementAttribute,
                        NSAccessibilityPositionAttribute,
                        NSAccessibilitySizeAttribute, nil];
  }
  return attributes;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (!node_)
    return nil;
  if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
    return [self axRole];
  } else if ([attribute
                 isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
    return NSAccessibilityRoleDescription([self axRole], nil);
  } else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    // Just check if the app thinks we're focused.
    id focusedElement = [NSApp
        accessibilityAttributeValue:NSAccessibilityFocusedUIElementAttribute];
    return [NSNumber numberWithBool:[focusedElement isEqual:self]];
  } else if ([attribute isEqualToString:NSAccessibilityParentAttribute]) {
    return NSAccessibilityUnignoredAncestor(parent_);
  } else if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    return NSAccessibilityUnignoredChildren([self getKids]);
  } else if ([attribute isEqualToString:NSAccessibilityWindowAttribute]) {
    // We're in the same window as our parent.
    return [parent_ accessibilityAttributeValue:NSAccessibilityWindowAttribute];
  } else if ([attribute
                 isEqualToString:NSAccessibilityTopLevelUIElementAttribute]) {
    // We're in the same top level element as our parent.
    return [parent_
        accessibilityAttributeValue:NSAccessibilityTopLevelUIElementAttribute];
  } else if ([attribute isEqualToString:NSAccessibilityPositionAttribute]) {
    return [NSValue valueWithPoint:[self position]];
  } else if ([attribute isEqualToString:NSAccessibilitySizeAttribute]) {
    return [NSValue valueWithSize:[self size]];
  } else if ([attribute isEqualToString:NSAccessibilityDescriptionAttribute]) {
    return [self axDescription];
  } else if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    return [self axValue];
  } else if ([attribute isEqualToString:NSAccessibilityTitleAttribute]) {
    return [self axName];
  }
  return nil;
}

- (id)accessibilityHitTest:(NSPoint)point {
  return NSAccessibilityUnignoredAncestor(self);
}

- (NSArray*)accessibilityActionNames {
  return [NSArray arrayWithObject:NSAccessibilityPressAction];
}

- (NSString*)accessibilityActionDescription:(NSString*)action {
  return NSAccessibilityActionDescription(action);
}

- (void)accessibilityPerformAction:(NSString*)action {
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    // Do Click on Default action
    [self doMouseClick:MBT_LEFT];
  } else if ([action isEqualToString:NSAccessibilityShowMenuAction]) {
    // Right click for Context Menu
    [self doMouseClick:MBT_RIGHT];
  }
}

- (id)accessibilityFocusedUIElement {
  return NSAccessibilityUnignoredAncestor(self);
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  // Indicate that BrowserAccessibilityCocoa will post a notification when it's
  // destroyed (see -detach). This allows VoiceOver to do some internal things
  // more efficiently.
  return YES;
}

@end

namespace client {

void OsrAXNode::NotifyAccessibilityEvent(std::string event_type) const {
  if (event_type == "focus") {
    NSAccessibilityPostNotification(
        GetWindowHandle(), NSAccessibilityFocusedUIElementChangedNotification);
  } else if (event_type == "textChanged") {
    NSAccessibilityPostNotification(GetWindowHandle(),
                                    NSAccessibilityTitleChangedNotification);
  } else if (event_type == "valueChanged") {
    NSAccessibilityPostNotification(GetWindowHandle(),
                                    NSAccessibilityValueChangedNotification);
  } else if (event_type == "textSelectionChanged") {
    NSAccessibilityPostNotification(GetWindowHandle(),
                                    NSAccessibilityValueChangedNotification);
  }
}

void OsrAXNode::Destroy() {
  if (platform_accessibility_) {
    NSAccessibilityPostNotification(
        platform_accessibility_, NSAccessibilityUIElementDestroyedNotification);
  }

  delete this;
}

// Create and return NSAccessibility Implementation Object for Mac
CefNativeAccessible* OsrAXNode::GetNativeAccessibleObject(
    client::OsrAXNode* parent) {
  if (!platform_accessibility_) {
    platform_accessibility_ = [OsrAXNodeObject elementWithNode:this];
    SetParent(parent);
  }
  return platform_accessibility_;
}

}  // namespace client
