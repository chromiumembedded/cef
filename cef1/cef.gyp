# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

{
  'variables': {
    'pkg-config': 'pkg-config',
    'chromium_code': 1,
    'repack_locales_cmd': ['python', 'tools/repack_locales.py'],
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/cef',
  },
  'conditions': [
    [ 'os_posix==1 and OS!="mac" and OS!="android" and gcc_version==46', {
      'target_defaults': {
        # Disable warnings about c++0x compatibility, as some names (such
        # as nullptr) conflict with upcoming c++0x types.
        'cflags_cc': ['-Wno-c++0x-compat'],
      },
    }],
  ],
  'includes': [
    # Bring in the source file lists.
    'cef_paths2.gypi',
   ],
  'targets': [
    {
      'target_name': 'cefclient',
      'type': 'executable',
      'mac_bundle': 1,
      'msvs_guid': '6617FED9-C5D4-4907-BF55-A90062A6683F',
      'dependencies': [
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        'libcef',
        'libcef_dll_wrapper',
      ],
      'defines': [
        'USING_CEF_SHARED',
      ],
      'include_dirs': [
        '.',
        # cefclient includes are relative to the tests directory to make
        # creation of binary releases easier.
        'tests'
      ],
      'sources': [
        '<@(includes_common)',
        '<@(includes_wrapper)',
        '<@(cefclient_sources_common)',
      ],
      'mac_bundle_resources': [
        '<@(cefclient_bundle_resources_mac)',
        '<(grit_out_dir)/devtools_resources.pak',
      ],
      'mac_bundle_resources!': [
        # TODO(mark): Come up with a fancier way to do this (mac_info_plist?)
        # that automatically sets the correct INFOPLIST_FILE setting and adds
        # the file to a source group.
        'tests/cefclient/mac/Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'tests/cefclient/mac/Info.plist',
      },
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'repack_path': '../tools/grit/grit/format/repack.py',
          },
          'actions': [
            {
              'action_name': 'repack_locales',
              'inputs': [
                'tools/repack_locales.py',
                # NOTE: Ideally the common command args would be shared
                # amongst inputs/outputs/action, but the args include shell
                # variables which need to be passed intact, and command
                # expansion wants to expand the shell variables. Adding the
                # explicit quoting here was the only way it seemed to work.
                '>!@(<(repack_locales_cmd) -i -g \"<(grit_out_dir)\" -s \"<(SHARED_INTERMEDIATE_DIR)\" -x \"<(INTERMEDIATE_DIR)\" <(locales))',
              ],
              'outputs': [
                '>!@(<(repack_locales_cmd) -o -g \"<(grit_out_dir)\" -s \"<(SHARED_INTERMEDIATE_DIR)\" -x \"<(INTERMEDIATE_DIR)\" <(locales))',
              ],
              'action': [
                '<@(repack_locales_cmd)',
                '-g', '<(grit_out_dir)',
                '-s', '<(SHARED_INTERMEDIATE_DIR)',
                '-x', '<(INTERMEDIATE_DIR)',
                '<@(locales)',
              ],
            },
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/locales',
              'files': [
                '<!@pymod_do_main(repack_locales -o -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(INTERMEDIATE_DIR) <(locales))'
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(grit_out_dir)/devtools_resources.pak'
              ],
            },
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              # Set /SUBSYSTEM:WINDOWS.
              'SubSystem': '2',
              'EntryPointSymbol' : 'wWinMainCRTStartup',
            },
          },
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
              '-lshlwapi.lib',
              '-lrpcrt4.lib',
              '-lopengl32.lib',
              '-lglu32.lib',
            ],
          },
          'sources': [
            '<@(includes_win)',
            '<@(cefclient_sources_win)',
          ],
        }],
        [ 'OS=="mac"', {
          'product_name': 'cefclient',
          'variables': {
            'repack_path': '../tools/grit/grit/format/repack.py',
          },
          'actions': [
            {
              'action_name': 'repack_locales',
              'process_outputs_as_mac_bundle_resources': 1,
              'inputs': [
                'tools/repack_locales.py',
                # NOTE: Ideally the common command args would be shared
                # amongst inputs/outputs/action, but the args include shell
                # variables which need to be passed intact, and command
                # expansion wants to expand the shell variables. Adding the
                # explicit quoting here was the only way it seemed to work.
                '>!@(<(repack_locales_cmd) -i -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
              ],
              'outputs': [
                '>!@(<(repack_locales_cmd) -o -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
              ],
              'action': [
                '<@(repack_locales_cmd)',
                '-g', '<(grit_out_dir)',
                '-s', '<(SHARED_INTERMEDIATE_DIR)',
                '-x', '<(INTERMEDIATE_DIR)',
                '<@(locales)',
              ],
            },
            {
              'action_name': 'repack_resources',
              'process_outputs_as_mac_bundle_resources': 1,
              'variables': {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                ],
              },
              'inputs': [
                '<(repack_path)',
                '<@(pak_inputs)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/repack/chrome.pak',
              ],
              'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
            },
          ],
          'copies': [
            {
              # Add library dependencies to the bundle.
              'destination': '<(PRODUCT_DIR)/cefclient.app/Contents/MacOS/',
              'files': [
                '<(PRODUCT_DIR)/libcef.dylib',
                '<(PRODUCT_DIR)/ffmpegsumo.so',
              ],
            },
            {
              # Add the WebCore resources to the bundle.
              'destination': '<(PRODUCT_DIR)/cefclient.app/Contents/',
              'files': [
                '../third_party/WebKit/Source/WebCore/Resources/',
              ],
            },
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
          'sources': [
            '<@(includes_mac)',
            '<@(cefclient_sources_mac)',
          ],
        }],
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'sources': [
            '<@(includes_linux)',
            '<@(cefclient_sources_linux)',
          ],
          'variables': {
            'repack_path': '../tools/grit/grit/format/repack.py',
          },
          'actions': [
            {
              'action_name': 'repack_locales',
              'inputs': [
                'tools/repack_locales.py',
                # NOTE: Ideally the common command args would be shared
                # amongst inputs/outputs/action, but the args include shell
                # variables which need to be passed intact, and command
                # expansion wants to expand the shell variables. Adding the
                # explicit quoting here was the only way it seemed to work.
                '>!@(<(repack_locales_cmd) -i -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
              ],
              'outputs': [
                '>!@(<(repack_locales_cmd) -o -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
              ],
              'action': [
                '<@(repack_locales_cmd)',
                '-g', '<(grit_out_dir)',
                '-s', '<(SHARED_INTERMEDIATE_DIR)',
                '-x', '<(INTERMEDIATE_DIR)',
                '<@(locales)',
              ],
            },
            {
              'action_name': 'repack_resources',
              'variables': {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                ],
              },
              'inputs': [
                '<(repack_path)',
                '<@(pak_inputs)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/repack/chrome.pak',
              ],
              'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
            },
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/locales',
              'files': [
                '<!@pymod_do_main(repack_locales -o -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(INTERMEDIATE_DIR) <(locales))'
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(INTERMEDIATE_DIR)/repack/chrome.pak',
                '<(grit_out_dir)/devtools_resources.pak',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/files',
              'files': [
                '<@(cefclient_bundle_resources_linux)',
              ],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'cef_unittests',
      'type': 'executable',
      'mac_bundle': 1,
      'msvs_guid': '8500027C-B11A-11DE-A16E-B80256D89593',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        'libcef',
        'libcef_dll_wrapper',
      ],
      'sources': [
        'tests/cefclient/cefclient_switches.cpp',
        'tests/cefclient/cefclient_switches.h',
        'tests/unittests/command_line_unittest.cc',
        'tests/unittests/content_filter_unittest.cc',
        'tests/unittests/cookie_unittest.cc',
        'tests/unittests/dom_unittest.cc',
        'tests/unittests/navigation_unittest.cc',
        'tests/unittests/request_unittest.cc',
        'tests/unittests/run_all_unittests.cc',
        'tests/unittests/scheme_handler_unittest.cc',
        'tests/unittests/stream_unittest.cc',
        'tests/unittests/string_unittest.cc',
        'tests/unittests/test_handler.cc',
        'tests/unittests/test_handler.h',
        'tests/unittests/test_suite.cc',
        'tests/unittests/test_suite.h',
        'tests/unittests/url_unittest.cc',
        'tests/unittests/v8_unittest.cc',
        'tests/unittests/web_urlrequest_unittest.cc',
        'tests/unittests/xml_reader_unittest.cc',
        'tests/unittests/zip_reader_unittest.cc',
      ],
      'mac_bundle_resources': [
        'tests/unittests/mac/unittests.icns',
        'tests/unittests/mac/English.lproj/InfoPlist.strings',
        'tests/unittests/mac/English.lproj/MainMenu.xib',
        'tests/unittests/mac/Info.plist',
      ],
      'mac_bundle_resources!': [
        # TODO(mark): Come up with a fancier way to do this (mac_info_plist?)
        # that automatically sets the correct INFOPLIST_FILE setting and adds
        # the file to a source group.
        'tests/unittests/mac/Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'tests/unittests/mac/Info.plist',
      },
      'include_dirs': [
        '.',
      ],
      'conditions': [
        [ 'OS=="mac"', {
          'product_name': 'cef_unittests',
          'variables': {
            'repack_path': '../tools/grit/grit/format/repack.py',
          },
          'run_as': {
            'action': ['${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.app/Contents/MacOS/${PRODUCT_NAME}'],
          },
          'actions': [
            {
              'action_name': 'repack_locales',
              'process_outputs_as_mac_bundle_resources': 1,
              'inputs': [
                'tools/repack_locales.py',
                # NOTE: Ideally the common command args would be shared
                # amongst inputs/outputs/action, but the args include shell
                # variables which need to be passed intact, and command
                # expansion wants to expand the shell variables. Adding the
                # explicit quoting here was the only way it seemed to work.
                '>!@(<(repack_locales_cmd) -i -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
              ],
              'outputs': [
                '>!@(<(repack_locales_cmd) -o -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
              ],
              'action': [
                '<@(repack_locales_cmd)',
                '-g', '<(grit_out_dir)',
                '-s', '<(SHARED_INTERMEDIATE_DIR)',
                '-x', '<(INTERMEDIATE_DIR)',
                '<@(locales)',
              ],
            },
            {
              'action_name': 'repack_resources',
              'process_outputs_as_mac_bundle_resources': 1,
              'variables': {
                'pak_inputs': [
                  '<(grit_out_dir)/devtools_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                ],
              },
              'inputs': [
                '<(repack_path)',
                '<@(pak_inputs)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/repack/chrome.pak',
              ],
              'action': ['python', '<(repack_path)', '<@(_outputs)', '<@(pak_inputs)'],
            },
          ],
          'copies': [
            {
              # Add library dependencies to the bundle.
              'destination': '<(PRODUCT_DIR)/cef_unittests.app/Contents/MacOS/',
              'files': [
                '<(PRODUCT_DIR)/libcef.dylib',
                '<(PRODUCT_DIR)/ffmpegsumo.so',
              ],
            },
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
            ],
          },
          'sources': [
            'tests/unittests/run_all_unittests_mac.mm',
          ],
        }],
      ],
    },
    {
      'target_name': 'libcef',
      'type': 'shared_library',
      'msvs_guid': 'C13650D5-CF1A-4259-BE45-B1EBA6280E47',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/printing/printing.gyp:printing',
        '<(DEPTH)/sdch/sdch.gyp:sdch',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/bzip2/bzip2.gyp:bzip2',
        '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '<(DEPTH)/third_party/libjpeg_turbo/libjpeg.gyp:libjpeg',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(DEPTH)/third_party/modp_b64/modp_b64.gyp:modp_b64',
        '<(DEPTH)/third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        '<(DEPTH)/ui/ui.gyp:ui_resources_standard',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/support/webkit_support.gyp:appcache',
        '<(DEPTH)/webkit/support/webkit_support.gyp:blob',
        '<(DEPTH)/webkit/support/webkit_support.gyp:database',
        '<(DEPTH)/webkit/support/webkit_support.gyp:fileapi',
        '<(DEPTH)/webkit/support/webkit_support.gyp:glue',
        '<(DEPTH)/webkit/support/webkit_support.gyp:quota',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_gpu',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_media',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_user_agent',
        'libcef_static',
      ],
      'defines': [
        'BUILDING_CEF_SHARED',
      ],
      'include_dirs': [
        '.',
      ],
      # Avoid "RC1102: internal error : too many arguments to RCPP" error by
      # explicitly specifying a short list of resource include directories.
      'resource_include_dirs' :  [
        '.',
        '..',
      ],
      'sources': [
        '<@(includes_common)',
        '<@(includes_capi)',
        '<@(libcef_sources_common)',
      ],
      'xcode_settings': {
        'INSTALL_PATH': '@executable_path',
        'DYLIB_INSTALL_NAME_BASE': '@executable_path',
        # The libcef_static target contains ObjC categories. Passing the -ObjC flag
        # is necessary to properly load them and avoid a "selector not recognized"
        # runtime error. See http://developer.apple.com/library/mac/#qa/qa1490/_index.html
        # for more information.
        'OTHER_LDFLAGS': ['-Wl,-ObjC'],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
            '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler',
            '<(DEPTH)/third_party/angle/src/build_angle.gyp:libEGL',
            '<(DEPTH)/third_party/angle/src/build_angle.gyp:libGLESv2',
          ],
          'sources': [
            '<@(includes_win)',
            '$(OutDir)/obj/global_intermediate/webkit/webkit_chromium_resources.rc',
            '$(OutDir)/obj/global_intermediate/webkit/webkit_resources.rc',
            'libcef_dll/libcef_dll.rc',
          ],
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
              '-llocationapi.lib',
            ],
          },
          'msvs_settings': {
            'VCLinkerTool': {
              # Generate a PDB symbol file for both Debug and Release builds.
              'GenerateDebugInformation': 'true',
            },
          },
        }],
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
         'dependencies':[
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
          'direct_dependent_settings': {
            'cflags': [
              '<!@(<(pkg-config) --cflags gtk+-2.0 gthread-2.0)',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(<(pkg-config) --libs-only-L --libs-only-other gtk+-2.0 gthread-2.0)',
            ],
            'libraries': [
              '<!@(<(pkg-config) --libs-only-l gtk+-2.0 gthread-2.0)',
            ],
          },
      	}],
      ],
    },
    {
      'target_name': 'libcef_dll_wrapper',
      'type': 'static_library',
      'msvs_guid': 'A9D6DC71-C0DC-4549-AEA0-3B15B44E86A9',
      'dependencies': [
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        'libcef',
      ],
      'defines': [
        'USING_CEF_SHARED',
      ],
      'include_dirs': [
        '.',
      ],
      'sources': [
        '<@(includes_common)',
        '<@(includes_capi)',
        '<@(includes_wrapper)',
        '<@(libcef_dll_wrapper_sources_common)',
      ],
    },
    {
      'target_name': 'cef_extra_resources',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:generate_devtools_grd',
      ],
      # These resources end up in chrome.pak because they are resources
      # used by internal pages.  Putting them in a spearate pak file makes
      # it easier for us to reference them internally.
      'actions': [
        {
          'action_name': 'devtools_resources',
          # This can't use ../build/grit_action.gypi because the grd file
          # is generated a build time, so the trick of using grit_info to get
          # the real inputs/outputs at GYP time isn't possible.
          'variables': {
            'grit_cmd': ['python', '../tools/grit/grit.py'],
            'grit_grd_file': '<(SHARED_INTERMEDIATE_DIR)/devtools/devtools_resources.grd',
          },
          'inputs': [
            '<(grit_grd_file)',
            '<!@pymod_do_main(grit_info --inputs)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/devtools_resources.h',
            '<(grit_out_dir)/devtools_resources.pak',
            '<(grit_out_dir)/grit/devtools_resources_map.cc',
            '<(grit_out_dir)/grit/devtools_resources_map.h',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(grit_grd_file)', 'build',
                     '-o', '<(grit_out_dir)',
                     '-D', 'SHARED_INTERMEDIATE_DIR=<(SHARED_INTERMEDIATE_DIR)',
                     '<@(grit_defines)' ],
          'message': 'Generating resources from <(grit_grd_file)',
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'libcef_static',
      'type': 'static_library',
      'msvs_guid': 'FA39524D-3067-4141-888D-28A86C66F2B9',
      'defines': [
        'BUILDING_CEF_SHARED',
      ],
      'include_dirs': [
        '.',
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/public',
        '<(grit_out_dir)',
      ],
      'dependencies': [
        'cef_extra_resources',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/printing/printing.gyp:printing',
        '<(DEPTH)/sdch/sdch.gyp:sdch',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/bzip2/bzip2.gyp:bzip2',
        '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '<(DEPTH)/third_party/libjpeg_turbo/libjpeg.gyp:libjpeg',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/libxslt/libxslt.gyp:libxslt',
        '<(DEPTH)/third_party/modp_b64/modp_b64.gyp:modp_b64',
        '<(DEPTH)/third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        '<(DEPTH)/ui/ui.gyp:ui_resources_standard',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/support/webkit_support.gyp:appcache',
        '<(DEPTH)/webkit/support/webkit_support.gyp:blob',
        '<(DEPTH)/webkit/support/webkit_support.gyp:database',
        '<(DEPTH)/webkit/support/webkit_support.gyp:dom_storage',
        '<(DEPTH)/webkit/support/webkit_support.gyp:fileapi',
        '<(DEPTH)/webkit/support/webkit_support.gyp:glue',
        '<(DEPTH)/webkit/support/webkit_support.gyp:quota',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_gpu',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_media',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_user_agent',
      ],
      'sources': [
        '<@(includes_common)',
        'libcef/browser_appcache_system.cc',
        'libcef/browser_appcache_system.h',
        'libcef/browser_database_system.cc',
        'libcef/browser_database_system.h',
        'libcef/browser_devtools_agent.cc',
        'libcef/browser_devtools_agent.h',
        'libcef/browser_devtools_callargs.cc',
        'libcef/browser_devtools_callargs.h',
        'libcef/browser_devtools_client.cc',
        'libcef/browser_devtools_client.h',
        'libcef/browser_devtools_scheme_handler.cc',
        'libcef/browser_devtools_scheme_handler.h',
        'libcef/browser_dom_storage_system.cc',
        'libcef/browser_dom_storage_system.h',
        'libcef/browser_file_system.cc',
        'libcef/browser_file_system.h',
        'libcef/browser_file_writer.cc',
        'libcef/browser_file_writer.h',
        'libcef/browser_impl.cc',
        'libcef/browser_impl.h',
        'libcef/browser_navigation_controller.cc',
        'libcef/browser_navigation_controller.h',
        'libcef/browser_network_delegate.cc',
        'libcef/browser_network_delegate.h',
        'libcef/browser_request_context.cc',
        'libcef/browser_request_context.h',
        'libcef/browser_request_context_proxy.cc',
        'libcef/browser_request_context_proxy.h',
        'libcef/browser_resource_loader_bridge.cc',
        'libcef/browser_resource_loader_bridge.h',
        'libcef/browser_settings.cc',
        'libcef/browser_settings.h',
        'libcef/browser_socket_stream_bridge.cc',
        'libcef/browser_socket_stream_bridge.h',
        'libcef/browser_thread_stub.cc',
        'libcef/browser_webcookiejar_impl.cc',
        'libcef/browser_webcookiejar_impl.h',
        'libcef/browser_webblobregistry_impl.cc',
        'libcef/browser_webblobregistry_impl.h',
        'libcef/browser_webkit_glue.cc',
        'libcef/browser_webkit_glue.h',
        'libcef/browser_webkit_init.cc',
        'libcef/browser_webkit_init.h',
        'libcef/browser_webview_delegate.cc',
        'libcef/browser_webview_delegate.h',
        'libcef/browser_zoom_map.cc',
        'libcef/browser_zoom_map.h',
        'libcef/cef_context.cc',
        'libcef/cef_context.h',
        'libcef/cef_process.cc',
        'libcef/cef_process.h',
        'libcef/cef_process_io_thread.cc',
        'libcef/cef_process_io_thread.h',
        'libcef/cef_process_sub_thread.cc',
        'libcef/cef_process_sub_thread.h',
        'libcef/cef_process_ui_thread.cc',
        'libcef/cef_process_ui_thread.h',
        'libcef/cef_string_list.cc',
        'libcef/cef_string_map.cc',
        'libcef/cef_string_multimap.cc',
        'libcef/cef_string_types.cc',
        'libcef/cef_thread.cc',
        'libcef/cef_thread.h',
        'libcef/cef_time.cc',
        'libcef/cef_time_util.h',
        'libcef/command_line_impl.cc',
        'libcef/cookie_manager_impl.cc',
        'libcef/cookie_manager_impl.h',
        'libcef/cookie_store_proxy.cc',
        'libcef/cookie_store_proxy.h',
        'libcef/drag_data_impl.cc',
        'libcef/drag_data_impl.h',
        'libcef/drag_download_file.cc',
        'libcef/drag_download_file.h',
        'libcef/drag_download_util.cc',
        'libcef/drag_download_util.h',
        'libcef/dom_document_impl.cc',
        'libcef/dom_document_impl.h',
        'libcef/dom_event_impl.cc',
        'libcef/dom_event_impl.h',
        'libcef/dom_node_impl.cc',
        'libcef/dom_node_impl.h',
        'libcef/download_util.cc',
        'libcef/download_util.h',
        'libcef/external_protocol_handler.h',
        'libcef/geolocation_client.cc',
        'libcef/geolocation_client.h',
        'libcef/http_header_utils.cc',
        'libcef/http_header_utils.h',
        'libcef/nplugin_impl.cc',
        'libcef/origin_whitelist_impl.cc',
        'libcef/request_impl.cc',
        'libcef/request_impl.h',
        'libcef/response_impl.cc',
        'libcef/response_impl.h',
        'libcef/scheme_impl.cc',
        'libcef/simple_clipboard_impl.cc',
        'libcef/simple_clipboard_impl.h',
        'libcef/sqlite_diagnostics_stub.cc',
        'libcef/stream_impl.cc',
        'libcef/stream_impl.h',
        'libcef/task_impl.cc',
        'libcef/tracker.h',
        'libcef/url_impl.cc',
        'libcef/v8_impl.cc',
        'libcef/v8_impl.h',
        'libcef/web_plugin_impl.cc',
        'libcef/web_urlrequest_impl.cc',
        'libcef/web_urlrequest_impl.h',
        'libcef/webview_host.cc',
        'libcef/webview_host.h',
        'libcef/webwidget_host.cc',
        'libcef/webwidget_host.h',
        'libcef/xml_reader_impl.cc',
        'libcef/xml_reader_impl.h',
        'libcef/zip_reader_impl.cc',
        'libcef/zip_reader_impl.h',
        '<(DEPTH)/chrome/browser/net/clear_on_exit_policy.cc',
        '<(DEPTH)/chrome/browser/net/clear_on_exit_policy.h',
        '<(DEPTH)/chrome/browser/net/sqlite_persistent_cookie_store.cc',
        '<(DEPTH)/chrome/browser/net/sqlite_persistent_cookie_store.h',
        # DevTools resource IDs generated by grit
        '<(grit_out_dir)/grit/devtools_resources_map.cc',
        # Geolocation implementation
        '<(DEPTH)/content/browser/geolocation/device_data_provider.cc',
        '<(DEPTH)/content/browser/geolocation/empty_device_data_provider.cc',
        '<(DEPTH)/content/browser/geolocation/geolocation_provider.cc',
        '<(DEPTH)/content/browser/geolocation/location_arbitrator.cc',
        '<(DEPTH)/content/browser/geolocation/location_provider.cc',
        '<(DEPTH)/content/browser/geolocation/network_location_provider.cc',
        '<(DEPTH)/content/browser/geolocation/network_location_request.cc',
        '<(DEPTH)/content/browser/geolocation/wifi_data_provider_common.cc',
        '<(DEPTH)/content/common/net/url_fetcher.cc',
        '<(DEPTH)/content/common/net/url_request_user_data.cc',
        '<(DEPTH)/content/public/common/geoposition.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/breakpad/breakpad.gyp:breakpad_handler',
            '<(DEPTH)/third_party/angle/src/build_angle.gyp:libEGL',
            '<(DEPTH)/third_party/angle/src/build_angle.gyp:libGLESv2',
          ],
          'sources': [
            '<@(includes_win)',
            'libcef/browser_drag_delegate_win.cc',
            'libcef/browser_drag_delegate_win.h',
            'libcef/browser_impl_win.cc',
            'libcef/browser_webkit_glue_win.cc',
            'libcef/browser_webview_delegate_win.cc',
            'libcef/cef_process_ui_thread_win.cc',
            'libcef/external_protocol_handler_win.cc',
            'libcef/printing/print_settings.cc',
            'libcef/printing/print_settings.h',
            'libcef/printing/win_printing_context.cc',
            'libcef/printing/win_printing_context.h',
            'libcef/web_drag_source_win.cc',
            'libcef/web_drag_source_win.h',
            'libcef/web_drag_utils_win.cc',
            'libcef/web_drag_utils_win.h',
            'libcef/web_drop_target_win.cc',
            'libcef/web_drop_target_win.h',
            'libcef/webview_host_win.cc',
            'libcef/webwidget_host_win.cc',
            # Geolocation implementation
            '<(DEPTH)/content/browser/geolocation/wifi_data_provider_common_win.cc',
            '<(DEPTH)/content/browser/geolocation/wifi_data_provider_win.cc',
            '<(DEPTH)/content/browser/geolocation/win7_location_api_win.cc',
            '<(DEPTH)/content/browser/geolocation/win7_location_provider_win.cc',
          ],
        }],
        [ 'OS=="mac"', {
          'sources': [
            '<@(includes_mac)',
            'libcef/browser_impl_mac.mm',
            'libcef/browser_webview_delegate_mac.mm',
            'libcef/browser_webview_mac.h',
            'libcef/browser_webview_mac.mm',
            'libcef/cef_process_ui_thread_mac.mm',
            'libcef/external_popup_menu_mac.h',
            'libcef/external_popup_menu_mac.mm',
            'libcef/external_protocol_handler_mac.mm',
            'libcef/webview_host_mac.mm',
            'libcef/webwidget_host_mac.mm',
            'libcef/web_drag_source_mac.h',
            'libcef/web_drag_source_mac.mm',
            'libcef/web_drag_utils_mac.h',
            'libcef/web_drag_utils_mac.mm',
            'libcef/web_drop_target_mac.h',
            'libcef/web_drop_target_mac.mm',
            # Include necessary Mozilla sources.
            '<(DEPTH)/third_party/mozilla/NSPasteboard+Utils.h',
            '<(DEPTH)/third_party/mozilla/NSPasteboard+Utils.mm',
            '<(DEPTH)/third_party/mozilla/NSString+Utils.h',
            '<(DEPTH)/third_party/mozilla/NSString+Utils.mm',
            '<(DEPTH)/third_party/mozilla/NSURL+Utils.h',
            '<(DEPTH)/third_party/mozilla/NSURL+Utils.m',
            # Geolocation implementation
            '<(DEPTH)/content/browser/geolocation/core_location_data_provider_mac.mm',
            '<(DEPTH)/content/browser/geolocation/core_location_provider_mac.mm',
            '<(DEPTH)/content/browser/geolocation/wifi_data_provider_corewlan_mac.mm',
            '<(DEPTH)/content/browser/geolocation/wifi_data_provider_mac.cc',
          ],
        }],
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:dbus',
            '<(DEPTH)/dbus/dbus.gyp:dbus',
          ],
          'sources': [
            '<@(includes_linux)',
            'libcef/browser_impl_gtk.cc',
            'libcef/browser_webview_delegate_gtk.cc',
            'libcef/cef_process_ui_thread_gtk.cc',
            'libcef/external_protocol_handler_gtk.cc',
            'libcef/webview_host_gtk.cc',
            'libcef/webwidget_host_gtk.cc',
            'libcef/web_drag_source_gtk.cc',
            'libcef/web_drag_source_gtk.h',
            'libcef/web_drop_target_gtk.cc',
            'libcef/web_drop_target_gtk.h',
            # Include the files needed for drag&drop that are otherwise excluded
            # by 'toolkit_views=0' in ui/ui.gyp.
            '<(DEPTH)/ui/base/dragdrop/drag_drop_types_gtk.cc',
            '<(DEPTH)/ui/base/dragdrop/os_exchange_data.cc',
            '<(DEPTH)/ui/base/dragdrop/os_exchange_data.h',
            '<(DEPTH)/ui/base/dragdrop/os_exchange_data_provider_gtk.cc',
            '<(DEPTH)/ui/base/dragdrop/os_exchange_data_provider_gtk.h',
            # Geolocation implementation
            '<(DEPTH)/content/browser/geolocation/gps_location_provider_linux.cc',
            '<(DEPTH)/content/browser/geolocation/libgps_wrapper_linux.cc',
            '<(DEPTH)/content/browser/geolocation/wifi_data_provider_linux.cc',
          ],
        }],
      ],
    },
  ]
}
