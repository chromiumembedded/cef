# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

{
  'variables': {
    'pkg-config': 'pkg-config',
    'chromium_code': 1,
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/cef',
    'revision': '<!(python tools/revision.py)',
    # Need to be creative to match dylib version formatting requirements.
    'version_mac_dylib':
        '<!(python ../chrome/tools/build/version.py -f VERSION -f ../chrome/VERSION -t "@CEF_MAJOR@<(revision).@BUILD_HI@.@BUILD_LO@" -e "BUILD_HI=int(BUILD)/256" -e "BUILD_LO=int(BUILD)%256")',
  },
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
        'cef_pak',
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
      ],
      'mac_bundle_resources!': [
        # TODO(mark): Come up with a fancier way to do this (mac_info_plist?)
        # that automatically sets the correct INFOPLIST_FILE setting and adds
        # the file to a source group.
        'tests/cefclient/mac/Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'tests/cefclient/mac/Info.plist',
        # Necessary to avoid an "install_name_tool: changing install names or
        # rpaths can't be redone" error.
        'OTHER_LDFLAGS': ['-Wl,-headerpad_max_install_names'],
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
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
        ['OS == "win" or (toolkit_uses_gtk == 1 and selinux == 0)', {
          'dependencies': [
            '<(DEPTH)/sandbox/sandbox.gyp:sandbox',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
        }],
        [ 'OS=="mac"', {
          'product_name': 'cefclient',
          'dependencies': [
            'cefclient_helper_app',
            'interpose_dependency_shim',
          ],
          'copies': [
            {
              # Add library dependencies to the bundle.
              'destination': '<(PRODUCT_DIR)/cefclient.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/',
              'files': [
                '<(PRODUCT_DIR)/libcef.dylib',
                '<(PRODUCT_DIR)/ffmpegsumo.so',
              ],
            },
            {
              # Add localized resources to the bundle.
              'destination': '<(PRODUCT_DIR)/cefclient.app/Contents/Frameworks/Chromium Embedded Framework.framework/Resources/',
              'files': [
                '<!@pymod_do_main(repack_locales -o -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(INTERMEDIATE_DIR) <(locales))',
              ],
            },
            {
              # Add the helper app.
              'destination': '<(PRODUCT_DIR)/cefclient.app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/cefclient Helper.app',
                '<(PRODUCT_DIR)/libplugin_carbon_interpose.dylib',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/libcef.dylib',
                '@executable_path/../Frameworks/Chromium Embedded Framework.framework/Libraries/libcef.dylib',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              'postbuild_name': 'Copy cef.pak File',
              'action': [
                'cp',
                '-f',
                '${BUILT_PRODUCTS_DIR}/cef.pak',
                '${BUILT_PRODUCTS_DIR}/cefclient.app/Contents/Frameworks/Chromium Embedded Framework.framework/Resources/cef.pak'
              ],
            },
            {
              'postbuild_name': 'Copy devtools_resources.pak File',
              'action': [
                'cp',
                '-f',
                '${BUILT_PRODUCTS_DIR}/devtools_resources.pak',
                '${BUILT_PRODUCTS_DIR}/cefclient.app/Contents/Frameworks/Chromium Embedded Framework.framework/Resources/devtools_resources.pak'
              ],
            },
            {
              'postbuild_name': 'Copy WebCore Resources',
              'action': [
                'cp',
                '-Rf',
                '${BUILT_PRODUCTS_DIR}/../../third_party/WebKit/Source/WebCore/Resources/',
                '${BUILT_PRODUCTS_DIR}/cefclient.app/Contents/Frameworks/Chromium Embedded Framework.framework/Resources/'
              ],
            },
            {
              # Modify the Info.plist as needed.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--scm=1'],
            },
            {
              # This postbuid step is responsible for creating the following
              # helpers:
              #
              # cefclient Helper EH.app and cefclient Helper NP.app are created
              # from cefclient Helper.app.
              #
              # The EH helper is marked for an executable heap. The NP helper
              # is marked for no PIE (ASLR).
              'postbuild_name': 'Make More Helpers',
              'action': [
                '../build/mac/make_more_helpers.sh',
                'Frameworks',
                'cefclient',
              ],
            },
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
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
          'copies': [
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
        'cef_pak',
        'libcef',
        'libcef_dll_wrapper',
      ],
      'sources': [
        'tests/cefclient/client_app.cpp',
        'tests/cefclient/client_app.h',
        'tests/cefclient/client_switches.cpp',
        'tests/cefclient/client_switches.h',
        'tests/unittests/command_line_unittest.cc',
        'tests/unittests/cookie_unittest.cc',
        'tests/unittests/dialog_unittest.cc',
        'tests/unittests/dom_unittest.cc',
        'tests/unittests/geolocation_unittest.cc',
        'tests/unittests/jsdialog_unittest.cc',
        'tests/unittests/navigation_unittest.cc',
        'tests/unittests/process_message_unittest.cc',
        'tests/unittests/request_unittest.cc',
        'tests/unittests/run_all_unittests.cc',
        'tests/unittests/scheme_handler_unittest.cc',
        'tests/unittests/stream_unittest.cc',
        'tests/unittests/string_unittest.cc',
        'tests/unittests/client_app_delegates.cc',
        'tests/unittests/test_handler.cc',
        'tests/unittests/test_handler.h',
        'tests/unittests/test_suite.cc',
        'tests/unittests/test_suite.h',
        'tests/unittests/test_util.cc',
        'tests/unittests/test_util.h',
        'tests/unittests/tracing_unittest.cc',
        'tests/unittests/url_unittest.cc',
        'tests/unittests/urlrequest_unittest.cc',
        'tests/unittests/v8_unittest.cc',
        'tests/unittests/values_unittest.cc',
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
        # Necessary to avoid an "install_name_tool: changing install names or
        # rpaths can't be redone" error.
        'OTHER_LDFLAGS': ['-Wl,-headerpad_max_install_names'],
      },
      'include_dirs': [
        '.',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'sources': [
            'tests/unittests/os_rendering_unittest.cc',
          ],
        }],
        [ 'OS=="mac"', {
          'product_name': 'cef_unittests',
          'dependencies': [
            'cef_unittests_helper_app',
          ],
          'run_as': {
            'action': ['${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.app/Contents/MacOS/${PRODUCT_NAME}'],
          },
          'copies': [
            {
              # Add library dependencies to the bundle.
              'destination': '<(PRODUCT_DIR)/cef_unittests.app/Contents/Frameworks/Chromium Embedded Framework.framework/Libraries/',
              'files': [
                '<(PRODUCT_DIR)/libcef.dylib',
                '<(PRODUCT_DIR)/ffmpegsumo.so',
              ],
            },
            {
              # Add localized resources to the bundle.
              'destination': '<(PRODUCT_DIR)/cef_unittests.app/Contents/Frameworks/Chromium Embedded Framework.framework/Resources/',
              'files': [
                '<!@pymod_do_main(repack_locales -o -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(INTERMEDIATE_DIR) <(locales))',
              ],
            },
            {
              # Add the helper app.
              'destination': '<(PRODUCT_DIR)/cef_unittests.app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/cef_unittests Helper.app',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/libcef.dylib',
                '@executable_path/../Frameworks/Chromium Embedded Framework.framework/Libraries/libcef.dylib',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              'postbuild_name': 'Copy cef.pak File',
              'action': [
                'cp',
                '-f',
                '${BUILT_PRODUCTS_DIR}/cef.pak',
                '${BUILT_PRODUCTS_DIR}/cef_unittests.app/Contents/Frameworks/Chromium Embedded Framework.framework/Resources/cef.pak'
              ],
            },
            {
              'postbuild_name': 'Copy devtools_resources.pak File',
              'action': [
                'cp',
                '-f',
                '${BUILT_PRODUCTS_DIR}/devtools_resources.pak',
                '${BUILT_PRODUCTS_DIR}/cef_unittests.app/Contents/Frameworks/Chromium Embedded Framework.framework/Resources/devtools_resources.pak'
              ],
            },
            {
              'postbuild_name': 'Copy WebCore Resources',
              'action': [
                'cp',
                '-Rf',
                '${BUILT_PRODUCTS_DIR}/../../third_party/WebKit/Source/WebCore/Resources/',
                '${BUILT_PRODUCTS_DIR}/cef_unittests.app/Contents/Frameworks/Chromium Embedded Framework.framework/Resources/'
              ],
            },
            {
              # Modify the Info.plist as needed.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--scm=1'],
            },
            {
              # This postbuid step is responsible for creating the following
              # helpers:
              #
              # cefclient Helper EH.app and cefclient Helper NP.app are created
              # from cefclient Helper.app.
              #
              # The EH helper is marked for an executable heap. The NP helper
              # is marked for no PIE (ASLR).
              'postbuild_name': 'Make More Helpers',
              'action': [
                '../build/mac/make_more_helpers.sh',
                'Frameworks',
                'cef_unittests',
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
        '<(DEPTH)/content/content.gyp:content_app',
        '<(DEPTH)/content/content.gyp:content_browser',
        '<(DEPTH)/content/content.gyp:content_common',
        '<(DEPTH)/content/content.gyp:content_gpu',
        '<(DEPTH)/content/content.gyp:content_plugin',
        '<(DEPTH)/content/content.gyp:content_ppapi_plugin',
        '<(DEPTH)/content/content.gyp:content_renderer',
        '<(DEPTH)/content/content.gyp:content_utility',
        '<(DEPTH)/content/content.gyp:content_worker',
        '<(DEPTH)/content/content_resources.gyp:content_resources',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '<(DEPTH)/third_party/zlib/zlib.gyp:minizip',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/support/webkit_support.gyp:glue',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_storage',
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
        # The libcef_static target contains ObjC categories. Passing the -ObjC flag
        # is necessary to properly load them and avoid a "selector not recognized"
        # runtime error. See http://developer.apple.com/library/mac/#qa/qa1490/_index.html
        # for more information.
        'OTHER_LDFLAGS': ['-Wl,-ObjC'],
        'DYLIB_COMPATIBILITY_VERSION': '<(version_mac_dylib)',
        'DYLIB_CURRENT_VERSION': '<(version_mac_dylib)',
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
          'sources': [
            '<@(includes_win)',
            # TODO(cef): Remove webkit_resources.rc once custom cursor resources
            # can be loaded via ResourceBundle. See crbug.com/147663.
            '$(OutDir)/obj/global_intermediate/webkit/webkit_resources.rc',
            'libcef_dll/libcef_dll.rc',
          ],
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
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
      # Create the pack file for CEF strings.
      'target_name': 'cef_strings',
      'type': 'none',
      'actions': [
        {
          'action_name': 'cef_strings',
          'variables': {
            'grit_grd_file': 'libcef/resources/cef_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # Create the locale-specific pack files.
      'target_name': 'cef_locales',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/ui/base/strings/ui_strings.gyp:ui_strings',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
        'cef_strings',
      ],
      'variables': {
        'repack_locales_cmd': ['python', 'tools/repack_locales.py'],
      },
      'conditions': [
        ['OS=="win"', {
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
        }, { # OS!="win"
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
          ],
        }],
        ['OS != "mac"', {
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/locales',
              'files': [
                '<!@pymod_do_main(repack_locales -o -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(INTERMEDIATE_DIR) <(locales))'
              ],
            },
          ],
        }],
      ],
    },
    {
      # Create the pack file for CEF resources.
      'target_name': 'cef_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'cef_resources',
          'variables': {
            'grit_grd_file': 'libcef/resources/cef_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(grit_out_dir)/cef_resources.pak'
          ],
        },
      ],
    },
    {
      # Combine all non-localized pack file resources into a single CEF pack file.
      'target_name': 'cef_pak',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/content/browser/debugger/devtools_resources.gyp:devtools_resources',
        '<(DEPTH)/content/content_resources.gyp:content_resources',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
        'cef_locales',
        'cef_resources',
      ],
      'variables': {
        'repack_path': '<(DEPTH)/tools/grit/grit/format/repack.py',
        'make_pack_header_path': 'tools/make_pack_header.py',
      },
      'actions': [
        {
          'action_name': 'repack_cef_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources_100_percent.pak',
              '<(grit_out_dir)/cef_resources.pak',
            ],
          },
          'inputs': [
            '<(repack_path)',
            '<@(pak_inputs)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/cef.pak',
          ],
          'action': ['python', '<(repack_path)', '<@(_outputs)',
                     '<@(pak_inputs)'],
        },
        {
          'action_name': 'make_pack_resources_header',
          'variables': {
            'header_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/content/grit/content_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/net/grit/net_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/grit/ui_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/grit/devtools_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/grit/webkit_chromium_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/grit/webkit_resources.h',
              '<(grit_out_dir)/grit/cef_resources.h',
            ],
          },
          'inputs': [
            '<(make_pack_header_path)',
            '<@(header_inputs)',
          ],
          'outputs': [
            'include/cef_pack_resources.h',
          ],
          'action': ['python', '<(make_pack_header_path)', '<@(_outputs)',
                     '<@(header_inputs)'],
        },
        {
          'action_name': 'make_pack_strings_header',
          'variables': {
            'header_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_strings/grit/ui_strings.h',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/grit/webkit_strings.h',
              '<(grit_out_dir)/grit/cef_strings.h',
            ],
          },
          'inputs': [
            '<(make_pack_header_path)',
            '<@(header_inputs)',
          ],
          'outputs': [
            'include/cef_pack_strings.h',
          ],
          'action': ['python', '<(make_pack_header_path)', '<@(_outputs)',
                     '<@(header_inputs)'],
        },
      ],
      'copies': [
        {
          # Keep the devtools_resources.pak file separate.
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/devtools_resources.pak',
          ],
        },
      ],
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
        # CEF grit resource includes
        '<(grit_out_dir)',
        '<(SHARED_INTERMEDIATE_DIR)/ui/ui_strings',
      ],
      'dependencies': [
        '<(DEPTH)/content/content.gyp:content_app',
        '<(DEPTH)/content/content.gyp:content_browser',
        '<(DEPTH)/content/content.gyp:content_common',
        '<(DEPTH)/content/content.gyp:content_gpu',
        '<(DEPTH)/content/content.gyp:content_plugin',
        '<(DEPTH)/content/content.gyp:content_ppapi_plugin',
        '<(DEPTH)/content/content.gyp:content_renderer',
        '<(DEPTH)/content/content.gyp:content_utility',
        '<(DEPTH)/content/content.gyp:content_worker',
        '<(DEPTH)/content/content_resources.gyp:content_resources',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/WebKit/Source/WebCore/WebCore.gyp/WebCore.gyp:webcore',
        '<(DEPTH)/third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '<(DEPTH)/third_party/zlib/zlib.gyp:minizip',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/support/webkit_support.gyp:glue',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_storage',
        # Necessary to generate the grit include files.
        'cef_pak',
      ],
      'sources': [
        '<@(includes_common)',
        'libcef/browser/browser_context.cc',
        'libcef/browser/browser_context.h',
        'libcef/browser/browser_host_impl.cc',
        'libcef/browser/browser_host_impl.h',
        'libcef/browser/browser_main.cc',
        'libcef/browser/browser_main.h',
        'libcef/browser/browser_message_filter.cc',
        'libcef/browser/browser_message_filter.h',
        'libcef/browser/browser_message_loop.cc',
        'libcef/browser/browser_message_loop.h',
        'libcef/browser/browser_settings.cc',
        'libcef/browser/browser_settings.h',
        'libcef/browser/browser_urlrequest_impl.cc',
        'libcef/browser/browser_urlrequest_impl.h',
        'libcef/browser/chrome_scheme_handler.cc',
        'libcef/browser/chrome_scheme_handler.h',
        'libcef/browser/content_browser_client.cc',
        'libcef/browser/content_browser_client.h',
        'libcef/browser/context.cc',
        'libcef/browser/context.h',
        'libcef/browser/context_menu_params_impl.cc',
        'libcef/browser/context_menu_params_impl.h',
        'libcef/browser/cookie_manager_impl.cc',
        'libcef/browser/cookie_manager_impl.h',
        'libcef/browser/devtools_delegate.cc',
        'libcef/browser/devtools_delegate.h',
        'libcef/browser/devtools_scheme_handler.cc',
        'libcef/browser/devtools_scheme_handler.h',
        'libcef/browser/download_item_impl.cc',
        'libcef/browser/download_item_impl.h',
        'libcef/browser/download_manager_delegate.cc',
        'libcef/browser/download_manager_delegate.h',
        'libcef/browser/frame_host_impl.cc',
        'libcef/browser/frame_host_impl.h',
        'libcef/browser/geolocation_impl.cc',
        'libcef/browser/internal_scheme_handler.cc',
        'libcef/browser/internal_scheme_handler.h',
        'libcef/browser/javascript_dialog.h',
        'libcef/browser/javascript_dialog_creator.cc',
        'libcef/browser/javascript_dialog_creator.h',
        'libcef/browser/menu_creator.cc',
        'libcef/browser/menu_creator.h',
        'libcef/browser/menu_model_impl.cc',
        'libcef/browser/menu_model_impl.h',
        'libcef/browser/navigate_params.cc',
        'libcef/browser/navigate_params.h',
        'libcef/browser/origin_whitelist_impl.cc',
        'libcef/browser/origin_whitelist_impl.h',
        'libcef/browser/path_util_impl.cc',
        'libcef/browser/process_util_impl.cc',
        'libcef/browser/resource_context.cc',
        'libcef/browser/resource_context.h',
        'libcef/browser/resource_dispatcher_host_delegate.cc',
        'libcef/browser/resource_dispatcher_host_delegate.h',
        'libcef/browser/resource_request_job.cc',
        'libcef/browser/resource_request_job.h',
        'libcef/browser/scheme_impl.cc',
        'libcef/browser/scheme_registration.cc',
        'libcef/browser/scheme_registration.h',
        'libcef/browser/sqlite_diagnostics_stub.cc',
        'libcef/browser/stream_impl.cc',
        'libcef/browser/stream_impl.h',
        'libcef/browser/trace_impl.cc',
        'libcef/browser/trace_subscriber.cc',
        'libcef/browser/trace_subscriber.h',
        'libcef/browser/thread_util.h',
        'libcef/browser/url_network_delegate.cc',
        'libcef/browser/url_network_delegate.h',
        'libcef/browser/url_request_context_getter.cc',
        'libcef/browser/url_request_context_getter.h',
        'libcef/browser/url_request_context_getter_proxy.cc',
        'libcef/browser/url_request_context_getter_proxy.h',
        'libcef/browser/url_request_context_proxy.cc',
        'libcef/browser/url_request_context_proxy.h',
        'libcef/browser/url_request_interceptor.cc',
        'libcef/browser/url_request_interceptor.h',
        'libcef/browser/web_plugin_impl.cc',
        'libcef/browser/web_plugin_impl.h',
        'libcef/browser/xml_reader_impl.cc',
        'libcef/browser/xml_reader_impl.h',
        'libcef/browser/zip_reader_impl.cc',
        'libcef/browser/zip_reader_impl.h',
        'libcef/common/cef_message_generator.cc',
        'libcef/common/cef_message_generator.h',
        'libcef/common/cef_messages.cc',
        'libcef/common/cef_messages.h',
        'libcef/common/cef_switches.cc',
        'libcef/common/cef_switches.h',
        'libcef/common/command_line_impl.cc',
        'libcef/common/command_line_impl.h',
        'libcef/common/content_client.cc',
        'libcef/common/content_client.h',
        'libcef/common/http_header_utils.cc',
        'libcef/common/http_header_utils.h',
        'libcef/common/main_delegate.cc',
        'libcef/common/main_delegate.h',
        'libcef/common/process_message_impl.cc',
        'libcef/common/process_message_impl.h',
        'libcef/common/request_impl.cc',
        'libcef/common/request_impl.h',
        'libcef/common/response_impl.cc',
        'libcef/common/response_impl.h',
        'libcef/common/response_manager.cc',
        'libcef/common/response_manager.h',
        'libcef/common/scheme_registrar_impl.cc',
        'libcef/common/scheme_registrar_impl.h',
        'libcef/common/string_list_impl.cc',
        'libcef/common/string_map_impl.cc',
        'libcef/common/string_multimap_impl.cc',
        'libcef/common/string_types_impl.cc',
        'libcef/common/task_impl.cc',
        'libcef/common/time_impl.cc',
        'libcef/common/time_util.h',
        'libcef/common/tracker.cc',
        'libcef/common/tracker.h',
        'libcef/common/url_impl.cc',
        'libcef/common/urlrequest_impl.cc',
        'libcef/common/value_base.cc',
        'libcef/common/value_base.h',
        'libcef/common/values_impl.cc',
        'libcef/common/values_impl.h',
        'libcef/renderer/browser_impl.cc',
        'libcef/renderer/browser_impl.h',
        'libcef/renderer/chrome_bindings.cc',
        'libcef/renderer/chrome_bindings.h',
        'libcef/renderer/content_renderer_client.cc',
        'libcef/renderer/content_renderer_client.h',
        'libcef/renderer/dom_document_impl.cc',
        'libcef/renderer/dom_document_impl.h',
        'libcef/renderer/dom_event_impl.cc',
        'libcef/renderer/dom_event_impl.h',
        'libcef/renderer/dom_node_impl.cc',
        'libcef/renderer/dom_node_impl.h',
        'libcef/renderer/frame_impl.cc',
        'libcef/renderer/frame_impl.h',
        'libcef/renderer/render_message_filter.cc',
        'libcef/renderer/render_message_filter.h',
        'libcef/renderer/render_process_observer.cc',
        'libcef/renderer/render_process_observer.h',
        'libcef/renderer/render_urlrequest_impl.cc',
        'libcef/renderer/render_urlrequest_impl.h',
        'libcef/renderer/thread_util.h',
        'libcef/renderer/v8_impl.cc',
        'libcef/renderer/v8_impl.h',
        'libcef/renderer/webkit_glue.cc',
        'libcef/renderer/webkit_glue.h',
        # Include sources for persistent cookie storage.
        '<(DEPTH)/chrome/browser/net/clear_on_exit_policy.cc',
        '<(DEPTH)/chrome/browser/net/clear_on_exit_policy.h',
        '<(DEPTH)/chrome/browser/net/sqlite_persistent_cookie_store.cc',
        '<(DEPTH)/chrome/browser/net/sqlite_persistent_cookie_store.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            '<@(includes_win)',
            'libcef/browser/backing_store_osr.cc',
            'libcef/browser/backing_store_osr.h',
            'libcef/browser/backing_store_osr_win.cc',
            'libcef/browser/browser_host_impl_win.cc',
            'libcef/browser/browser_main_win.cc',
            'libcef/browser/javascript_dialog_win.cc',
            'libcef/browser/menu_creator_runner_win.cc',
            'libcef/browser/menu_creator_runner_win.h',
            'libcef/browser/render_widget_host_view_osr.h',
            'libcef/browser/render_widget_host_view_osr.cc',
            'libcef/browser/web_contents_view_osr.cc',
            'libcef/browser/web_contents_view_osr.h',
            # Include sources for context menu implementation.
            '<(DEPTH)/ui/views/controls/menu/menu_2.cc',
            '<(DEPTH)/ui/views/controls/menu/menu_2.h',
            '<(DEPTH)/ui/views/controls/menu/menu_config.cc',
            '<(DEPTH)/ui/views/controls/menu/menu_config.h',
            '<(DEPTH)/ui/views/controls/menu/menu_config_win.cc',
            '<(DEPTH)/ui/views/controls/menu/menu_listener.cc',
            '<(DEPTH)/ui/views/controls/menu/menu_listener.h',
            '<(DEPTH)/ui/views/controls/menu/native_menu_win.cc',
            '<(DEPTH)/ui/views/controls/menu/native_menu_win.h',
          ],
        }],
        [ 'OS=="mac"', {
          'sources': [
            '<@(includes_mac)',
            'libcef/browser/application_mac.h',
            'libcef/browser/application_mac.mm',
            'libcef/browser/browser_host_impl_mac.mm',
            'libcef/browser/browser_main_mac.mm',
            'libcef/browser/javascript_dialog_mac.mm',
            'libcef/browser/menu_creator_runner_mac.h',
            'libcef/browser/menu_creator_runner_mac.mm',
            # Include sources for context menu implementation.
            '<(DEPTH)/chrome/browser/disposition_utils.cc',
            '<(DEPTH)/chrome/browser/disposition_utils.h',
            '<(DEPTH)/chrome/browser/event_disposition.cc',
            '<(DEPTH)/chrome/browser/event_disposition.h',
            '<(DEPTH)/chrome/browser/ui/cocoa/event_utils.mm',
            '<(DEPTH)/chrome/browser/ui/cocoa/event_utils.h',
            '<(DEPTH)/chrome/browser/ui/cocoa/menu_controller.mm',
            '<(DEPTH)/chrome/browser/ui/cocoa/menu_controller.h',
          ],
        }],
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'sources': [
            '<@(includes_linux)',
            'libcef/browser/browser_host_impl_gtk.cc',
            'libcef/browser/browser_main_gtk.cc',
            'libcef/browser/gtk_util_stub.cc',
            'libcef/browser/javascript_dialog_gtk.cc',
            'libcef/browser/menu_creator_runner_gtk.cc',
            'libcef/browser/menu_creator_runner_gtk.h',
            # Include sources for context menu implementation.
            '<(DEPTH)/chrome/browser/ui/gtk/gtk_custom_menu.cc',
            '<(DEPTH)/chrome/browser/ui/gtk/gtk_custom_menu.h',
            '<(DEPTH)/chrome/browser/ui/gtk/gtk_custom_menu_item.cc',
            '<(DEPTH)/chrome/browser/ui/gtk/gtk_custom_menu_item.h',
            '<(DEPTH)/chrome/browser/ui/gtk/menu_gtk.cc',
            '<(DEPTH)/chrome/browser/ui/gtk/menu_gtk.h',
            '<(DEPTH)/chrome/browser/ui/views/event_utils.cc',
            '<(DEPTH)/chrome/browser/ui/views/event_utils.h',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['os_posix==1 and OS!="mac" and OS!="android" and gcc_version>=46', {
      'target_defaults': {
        # Disable warnings about c++0x compatibility, as some names (such
        # as nullptr) conflict with upcoming c++0x types.
        'cflags_cc': ['-Wno-c++0x-compat'],
      },
    }],
    ['OS=="mac"', {
      'targets': [
        {
          # Dummy target to allow cefclient to require plugin_carbon_interpose
          # to build without actually linking to the resulting library.
          'target_name': 'interpose_dependency_shim',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'plugin_carbon_interpose',
          ],
          # In release, we end up with a strip step that is unhappy if there is
          # no binary. Rather than check in a new file for this temporary hack,
          # just generate a source file on the fly.
          'actions': [
            {
              'action_name': 'generate_stub_main',
              'process_outputs_as_sources': 1,
              'inputs': [],
              'outputs': [ '<(INTERMEDIATE_DIR)/dummy_main.c' ],
              'action': [
                'bash', '-c',
                'echo "int main() { return 0; }" > <(INTERMEDIATE_DIR)/dummy_main.c'
              ],
            },
          ],
        },
        {
          # dylib for interposing Carbon calls in the plugin process.
          'target_name': 'plugin_carbon_interpose',
          'type': 'shared_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          # This target must not depend on static libraries, else the code in
          # those libraries would appear twice in plugin processes: Once from
          # Chromium Framework, and once from this dylib.
          'dependencies': [
            'libcef',
          ],
          'conditions': [
            ['component=="shared_library"', {
              'dependencies': [
                '<(DEPTH)/webkit/support/webkit_support.gyp:glue',
                '<(DEPTH)/content/content.gyp:content_plugin',
              ],
            }],
          ],
          'sources': [
            '<(DEPTH)/content/plugin/plugin_carbon_interpose_mac.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            ],
          },
          'xcode_settings': {
            'DYLIB_COMPATIBILITY_VERSION': '<(version_mac_dylib)',
            'DYLIB_CURRENT_VERSION': '<(version_mac_dylib)',
          },
          'postbuilds': [
            {
              # The framework defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # libplugin_carbon_interpose.dylib.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/libcef.dylib',
                '@executable_path/../../../../Frameworks/Chromium Embedded Framework.framework/Libraries/libcef.dylib',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
          ],
        },
        {
          'target_name': 'cefclient_helper_app',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': 'cefclient Helper',
          'mac_bundle': 1,
          'dependencies': [
            'cef_pak',
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
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
            ],
          },
          'sources': [
            '<@(cefclient_sources_mac_helper)',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list helper-Info.plist once, not the three times it
          # is listed here.
          'mac_bundle_resources!': [
            'tests/cefclient/mac/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'tests/cefclient/mac/helper-Info.plist',
            # Necessary to avoid an "install_name_tool: changing install names or
            # rpaths can't be redone" error.
            'OTHER_LDFLAGS': ['-Wl,-headerpad_max_install_names'],
          },
          'postbuilds': [
            {
              # The framework defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # cefclient_helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/libcef.dylib',
                '@executable_path/../../../../Frameworks/Chromium Embedded Framework.framework/Libraries/libcef.dylib',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, --breakpad=0, --keystone=0, and --scm=0
              # are used because Breakpad, Keystone, and SCM keys are
              # never placed into the helper.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--breakpad=0',
                         '--keystone=0',
                         '--scm=0'],
            },
          ],
        },  # target cefclient_helper_app
        {
          'target_name': 'cef_unittests_helper_app',
          'type': 'executable',
          'product_name': 'cef_unittests Helper',
          'mac_bundle': 1,
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base',
            '<(DEPTH)/base/base.gyp:base_i18n',
            '<(DEPTH)/base/base.gyp:test_support_base',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
            '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
            'cef_pak',
            'libcef',
            'libcef_dll_wrapper',
          ],
          'defines': [
            'USING_CEF_SHARED',
          ],
          'include_dirs': [
            '.',
          ],
          'sources': [
            'tests/cefclient/client_app.cpp',
            'tests/cefclient/client_app.h',
            'tests/cefclient/client_switches.cpp',
            'tests/cefclient/client_switches.h',
            'tests/cefclient/process_helper_mac.cpp',
            'tests/unittests/client_app_delegates.cc',
            'tests/unittests/cookie_unittest.cc',
            'tests/unittests/dom_unittest.cc',
            'tests/unittests/navigation_unittest.cc',
            'tests/unittests/process_message_unittest.cc',
            'tests/unittests/scheme_handler_unittest.cc',
            'tests/unittests/urlrequest_unittest.cc',
            'tests/unittests/test_handler.cc',
            'tests/unittests/test_handler.h',
            'tests/unittests/test_suite.cc',
            'tests/unittests/test_suite.h',
            'tests/unittests/test_util.cc',
            'tests/unittests/test_util.h',
            'tests/unittests/tracing_unittest.cc',
            'tests/unittests/v8_unittest.cc',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list helper-Info.plist once, not the three times it
          # is listed here.
          'mac_bundle_resources!': [
            'tests/cefclient/mac/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'tests/cefclient/mac/helper-Info.plist',
            # Necessary to avoid an "install_name_tool: changing install names or
            # rpaths can't be redone" error.
            'OTHER_LDFLAGS': ['-Wl,-headerpad_max_install_names'],
          },
          'postbuilds': [
            {
              # The framework defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # cefclient_helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/libcef.dylib',
                '@executable_path/../../../../Frameworks/Chromium Embedded Framework.framework/Libraries/libcef.dylib',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, --breakpad=0, --keystone=0, and --scm=0
              # are used because Breakpad, Keystone, and SCM keys are
              # never placed into the helper.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--breakpad=0',
                         '--keystone=0',
                         '--scm=0'],
            },
          ],
        },  # target cef_unittests_helper_app
      ],
    }],  # OS=="mac"
  ],
}
