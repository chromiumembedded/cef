# Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

# Execute the following command to run unit tests:
# python3 -m unittest discover -s tools -p *_test.py

from cef_api_hash import *
from collections import Counter
import unittest

# Test constants:
FILENAME = 'test.h'
NONE_DEBUG_DIR = None
VERBOSE = False
ADDED_DEFINES = []

CEF_TYPES_WIN = """#ifndef CEF_INCLUDE_INTERNAL_CEF_TYPES_WIN_H_
#define CEF_INCLUDE_INTERNAL_CEF_TYPES_WIN_H_
#pragma once

#if !defined(GENERATING_CEF_API_HASH)
#include "include/base/cef_build.h"
#endif

#if defined(OS_WIN)

#if !defined(GENERATING_CEF_API_HASH)
#include <windows.h>
#endif

#include "include/cef_api_hash.h"
#include "include/internal/cef_types_runtime.h"

#define kNullCursorHandle NULL
#define kNullEventHandle NULL
#define kNullWindowHandle NULL

#ifdef __cplusplus
extern "C" {
#endif

// Handle types.
typedef HWND cef_window_handle_t;

typedef struct _cef_window_info_t {
  size_t size;
  DWORD ex_style;
  HMENU menu;
  cef_window_handle_t parent_window;
  cef_runtime_style_t runtime_style;
#if CEF_API_ADDED(13304)
  int api_version_test;
#endif
} cef_window_info_t;


#ifdef __cplusplus
}
#endif
#endif  // OS_WIN
#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_WIN_H_
"""

CEF_TYPES_MAC = """#ifndef CEF_INCLUDE_INTERNAL_CEF_TYPES_MAC_H_
#define CEF_INCLUDE_INTERNAL_CEF_TYPES_MAC_H_
#pragma once

#if !defined(GENERATING_CEF_API_HASH)
#include "include/base/cef_build.h"
#endif

#include <this_header_does_not_exist.h>

#if defined(OS_MAC)
#include "include/internal/cef_string.h"
#include "include/internal/cef_types_geometry.h"
#include "include/internal/cef_types_runtime.h"

#define kNullCursorHandle NULL

#ifdef __OBJC__
#if __has_feature(objc_arc)
#define CAST_CEF_CURSOR_HANDLE_TO_NSCURSOR(handle) ((__bridge NSCursor*)handle)
#define CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(handle) ((__bridge NSView*)handle)

#define CAST_NSCURSOR_TO_CEF_CURSOR_HANDLE(cursor) ((__bridge void*)cursor)
#define CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(view) ((__bridge void*)view)
#else  // __has_feature(objc_arc)
#define CAST_CEF_CURSOR_HANDLE_TO_NSCURSOR(handle) ((NSCursor*)handle)
#define CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(handle) ((NSView*)handle)

#define CAST_NSCURSOR_TO_CEF_CURSOR_HANDLE(cursor) ((void*)cursor)
#define CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(view) ((void*)view)
#endif  // __has_feature(objc_arc)
#endif  // __OBJC__

#ifdef __cplusplus
extern "C" {
#endif

// Actually NSView*
typedef void* cef_window_handle_t;

///
/// Class representing window information.
///
typedef struct _cef_window_info_t {
  size_t size;
  cef_string_t window_name;
  cef_rect_t bounds;
  cef_window_handle_t view;
  cef_runtime_style_t runtime_style;
} cef_window_info_t;

#ifdef __cplusplus
}
#endif

#endif  // OS_MAC
#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_MAC_H_
"""

CEF_TYPES_LINUX = """#ifndef CEF_INCLUDE_INTERNAL_CEF_TYPES_LINUX_H_
#define CEF_INCLUDE_INTERNAL_CEF_TYPES_LINUX_H_
#pragma once

#if !defined(GENERATING_CEF_API_HASH)
#include "include/base/cef_build.h"
#endif

#if defined(OS_LINUX)

#include "include/internal/cef_export.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types_color.h"
#include "include/internal/cef_types_geometry.h"
#include "include/internal/cef_types_osr.h"
#include "include/internal/cef_types_runtime.h"

#define kNullCursorHandle 0
#define kNullEventHandle NULL
#define kNullWindowHandle 0

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CEF_X11)
typedef union _XEvent XEvent;
typedef struct _XDisplay XDisplay;

// Handle types.
typedef XEvent* cef_event_handle_t;
#else
typedef void* cef_event_handle_t;
#endif

typedef unsigned long cef_window_handle_t;

///
/// Return the singleton X11 display shared with Chromium. The display is not
/// thread-safe and must only be accessed on the browser process UI thread.
///
#if defined(CEF_X11)
CEF_EXPORT XDisplay* cef_get_xdisplay(void);
#endif

typedef struct _cef_window_info_t {
  size_t size;
  cef_string_t window_name;
  cef_rect_t bounds;
  cef_window_handle_t parent_window;
  cef_window_handle_t window;
  cef_runtime_style_t runtime_style;
} cef_window_info_t;

#ifdef __cplusplus
}
#endif

#endif  // OS_LINUX

#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_LINUX_H_
"""

CEF_TYPES = """#ifndef CEF_INCLUDE_INTERNAL_CEF_TYPES_H_
#define CEF_INCLUDE_INTERNAL_CEF_TYPES_H_
#pragma once

#include "include/cef_api_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CEF_CPAIT_OPTIMIZATION_GUIDE,
#if CEF_API_ADDED(13304)
  CEF_CPAIT_COLLABORATION_MESSAGING,
#endif
  CEF_CPAIT_NUM_VALUES,
} cef_chrome_page_action_icon_type_t;

#ifdef __cplusplus
}
#endif

#endif  // CEF_INCLUDE_INTERNAL_CEF_TYPES_H_
"""


class TestRunClangEval(unittest.TestCase):

  def test_parse_platformed_content_version_13304(self):
    expected_objects = [{
        'filename': FILENAME,
        'name': 'cef_window_handle_t',
        'platform': 'windows',
        'text': 'typedef HWND cef_window_handle_t;'
    }, {
        'filename':
            FILENAME,
        'name':
            'cef_window_info_t',
        'platform':
            'windows',
        'text':
            'typedef struct _cef_window_info_t { size_t size; DWORD ex_style; HMENU menu; cef_window_handle_t parent_window; cef_runtime_style_t runtime_style; int api_version_test; } cef_window_info_t;'
    }]

    objects = parse_platformed_content(FILENAME, CEF_TYPES_WIN, NONE_DEBUG_DIR,
                                       VERBOSE, '13304', ADDED_DEFINES)

    self.__assert_equal_objects(objects, expected_objects)

  def test_parse_platformed_content_version_13300(self):
    expected_objects = [{
        'filename': FILENAME,
        'name': 'cef_window_handle_t',
        'platform': 'windows',
        'text': 'typedef HWND cef_window_handle_t;'
    }, {
        'filename':
            FILENAME,
        'name':
            'cef_window_info_t',
        'platform':
            'windows',
        'text':
            'typedef struct _cef_window_info_t { size_t size; DWORD ex_style; HMENU menu; cef_window_handle_t parent_window; cef_runtime_style_t runtime_style; } cef_window_info_t;'
    }]

    objects = parse_platformed_content(FILENAME, CEF_TYPES_WIN, NONE_DEBUG_DIR,
                                       VERBOSE, '13300', ADDED_DEFINES)

    self.__assert_equal_objects(objects, expected_objects)

  def test_parse_platformed_content_mac(self):
    expected_objects = [{
        'filename': FILENAME,
        'name': 'cef_window_handle_t',
        'platform': 'mac',
        'text': 'typedef void* cef_window_handle_t;'
    }, {
        'filename':
            FILENAME,
        'name':
            'cef_window_info_t',
        'platform':
            'mac',
        'text':
            'typedef struct _cef_window_info_t { size_t size; cef_string_t window_name; cef_rect_t bounds; cef_window_handle_t view; cef_runtime_style_t runtime_style; } cef_window_info_t;'
    }]

    objects = parse_platformed_content(
        FILENAME,
        CEF_TYPES_MAC,
        NONE_DEBUG_DIR,
        VERBOSE,
        api_version=None,
        added_defines=[])

    self.__assert_equal_objects(objects, expected_objects)

  def test_parse_platformed_content_linux(self):
    expected_objects = [{
        'filename': FILENAME,
        'name': 'XEvent',
        'platform': 'linux',
        'text': 'typedef union _XEvent XEvent;'
    }, {
        'filename': FILENAME,
        'name': 'XDisplay',
        'platform': 'linux',
        'text': 'typedef struct _XDisplay XDisplay;'
    }, {
        'filename': FILENAME,
        'name': 'cef_event_handle_t',
        'platform': 'linux',
        'text': 'typedef XEvent* cef_event_handle_t;'
    }, {
        'filename': FILENAME,
        'name': 'cef_window_handle_t',
        'platform': 'linux',
        'text': 'typedef unsigned long cef_window_handle_t;'
    }, {
        'filename': FILENAME,
        'name': 'cef_get_xdisplay',
        'platform': 'linux',
        'text': 'CEF_EXPORT XDisplay* cef_get_xdisplay(void);'
    }, {
        'filename':
            FILENAME,
        'name':
            'cef_window_info_t',
        'platform':
            'linux',
        'text':
            'typedef struct _cef_window_info_t { size_t size; cef_string_t window_name; cef_rect_t bounds; cef_window_handle_t parent_window; cef_window_handle_t window; cef_runtime_style_t runtime_style; } cef_window_info_t;'
    }]

    objects = parse_platformed_content(
        FILENAME,
        CEF_TYPES_LINUX,
        NONE_DEBUG_DIR,
        VERBOSE,
        api_version=None,
        added_defines=[])

    self.__assert_equal_objects(objects, expected_objects)

  def test_parse_versioned_content_version_13304(self):
    expected_objects = [{
        'filename':
            FILENAME,
        'name':
            'cef_chrome_page_action_icon_type_t',
        'platform':
            'universal',
        'text':
            'typedef enum { CEF_CPAIT_OPTIMIZATION_GUIDE, CEF_CPAIT_COLLABORATION_MESSAGING, CEF_CPAIT_NUM_VALUES, } cef_chrome_page_action_icon_type_t;'
    }]

    objects = parse_versioned_content(FILENAME, CEF_TYPES, '13304',
                                      ADDED_DEFINES, NONE_DEBUG_DIR, VERBOSE)

    self.assertEqual(objects, expected_objects)

  def test_parse_versioned_content_version_13303(self):
    expected_objects = [{
        'filename':
            FILENAME,
        'name':
            'cef_chrome_page_action_icon_type_t',
        'platform':
            'universal',
        'text':
            'typedef enum { CEF_CPAIT_OPTIMIZATION_GUIDE, CEF_CPAIT_NUM_VALUES, } cef_chrome_page_action_icon_type_t;'
    }]

    objects = parse_versioned_content(FILENAME, CEF_TYPES, '13303',
                                      ADDED_DEFINES, NONE_DEBUG_DIR, VERBOSE)

    self.assertEqual(objects, expected_objects)

  def __assert_equal_objects(self, actual, expected):
    # Compare the objects as sets since the order is not guaranteed
    expected = Counter(tuple(sorted(d.items())) for d in expected)
    actual = Counter(tuple(sorted(d.items())) for d in actual)
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  unittest.main()
