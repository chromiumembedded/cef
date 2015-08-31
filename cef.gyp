# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

{
  'variables': {
    'pkg-config': 'pkg-config',
    'chromium_code': 1,
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/cef',
    'about_credits_file': '<(SHARED_INTERMEDIATE_DIR)/about_credits.html',
    'framework_name': 'Chromium Embedded Framework',
    'commit_number': '<!(python tools/commit_number.py)',
    'chrome_version': '<!(python ../build/util/version.py -f ../chrome/VERSION -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")',
    # Need to be creative to match dylib version formatting requirements.
    'version_mac_dylib':
        '<!(python ../build/util/version.py -f VERSION -f ../chrome/VERSION -t "@CEF_MAJOR@<(commit_number).@BUILD_HI@.@BUILD_LO@" -e "BUILD_HI=int(BUILD)/256" -e "BUILD_LO=int(BUILD)%256")',
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
      ],
      'mac_bundle_resources': [
        '<@(cefclient_bundle_resources_mac)',
      ],
      'mac_bundle_resources!': [
        # TODO(mark): Come up with a fancier way to do this (mac_info_plist?)
        # that automatically sets the correct INFOPLIST_FILE setting and adds
        # the file to a source group.
        'tests/cefclient/resources/mac/Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'tests/cefclient/resources/mac/Info.plist',
        # Necessary to avoid an "install_name_tool: changing install names or
        # rpaths can't be redone" error.
        'OTHER_LDFLAGS': ['-Wl,-headerpad_max_install_names'],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(DEPTH)/content/content_shell_and_tests.gyp:content_shell_crash_service',
            'cef_sandbox',
            'libcef',
          ],
          'defines': [
            'CEF_USE_ATL',
            'CEF_USE_SANDBOX',
          ],
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
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                'tests/cefclient/resources/win/cefclient.exe.manifest',
              ],
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
          'dependencies': [
            'cef_framework',
            'cefclient_helper_app',
          ],
          'variables': {
            'PRODUCT_NAME': 'cefclient',
          },
          'copies': [
            {
              # Add the helper app.
              'destination': '<(PRODUCT_DIR)/<(PRODUCT_NAME).app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/<(PRODUCT_NAME) Helper.app',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(framework_name).framework',
              'action': [
                '../build/mac/copy_framework_unversioned.sh',
                '${BUILT_PRODUCTS_DIR}/<(framework_name).framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
              ],
            },
            {
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/<(framework_name)',
                '@executable_path/../Frameworks/<(framework_name).framework/<(framework_name)',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              'postbuild_name': 'Copy locale Resources',
              'action': [
                'cp',
                '-Rf',
                '${BUILT_PRODUCTS_DIR}/locales/',
                '${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.app/Contents/Frameworks/<(framework_name).framework/Resources/'
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
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
          'sources': [
            '<@(includes_mac)',
            '<@(cefclient_sources_mac)',
          ],
        }],
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            'gtk',
            'gtkglext',
            'libcef',
          ],
          'link_settings': {
            'libraries': [
              '-lX11',
            ],
          },
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
      'target_name': 'cefsimple',
      'type': 'executable',
      'mac_bundle': 1,
      'msvs_guid': 'A5DCDE19-F4B1-4E3A-BD4F-BFE688B24D34',
      'dependencies': [
        'libcef_dll_wrapper',
      ],
      'defines': [
        'USING_CEF_SHARED',
      ],
      'include_dirs': [
        '.',
        # cefsimple includes are relative to the tests directory to make
        # creation of binary releases easier.
        'tests'
      ],
      'sources': [
        '<@(includes_common)',
        '<@(includes_wrapper)',
        '<@(cefsimple_sources_common)',
      ],
      'mac_bundle_resources': [
        '<@(cefsimple_bundle_resources_mac)',
      ],
      'mac_bundle_resources!': [
        # TODO(mark): Come up with a fancier way to do this (mac_info_plist?)
        # that automatically sets the correct INFOPLIST_FILE setting and adds
        # the file to a source group.
        'tests/cefsimple/mac/Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'tests/cefsimple/mac/Info.plist',
        # Necessary to avoid an "install_name_tool: changing install names or
        # rpaths can't be redone" error.
        'OTHER_LDFLAGS': ['-Wl,-headerpad_max_install_names'],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'cef_sandbox',
            'libcef',
          ],
          'defines': [
            'CEF_USE_SANDBOX',
          ],
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
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                'tests/cefsimple/cefsimple.exe.manifest',
              ],
            },
          },
          'link_settings': {
            'libraries': [
              '-lcomctl32.lib',
              '-lshlwapi.lib',
              '-lrpcrt4.lib',
            ],
          },
          'sources': [
            '<@(includes_win)',
            '<@(cefsimple_sources_win)',
          ],
        }],
        [ 'OS=="mac"', {
          'product_name': 'cefsimple',
          'dependencies': [
            'cef_framework',
            'cefsimple_helper_app',
          ],
          'variables': {
            'PRODUCT_NAME': 'cefsimple',
          },
          'copies': [
            {
              # Add the helper app.
              'destination': '<(PRODUCT_DIR)/<(PRODUCT_NAME).app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/<(PRODUCT_NAME) Helper.app',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(framework_name).framework',
              'action': [
                '../build/mac/copy_framework_unversioned.sh',
                '${BUILT_PRODUCTS_DIR}/<(framework_name).framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
              ],
            },
            {
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/<(framework_name)',
                '@executable_path/../Frameworks/<(framework_name).framework/<(framework_name)',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              'postbuild_name': 'Copy locale Resources',
              'action': [
                'cp',
                '-Rf',
                '${BUILT_PRODUCTS_DIR}/locales/',
                '${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.app/Contents/Frameworks/<(framework_name).framework/Resources/'
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
              # cefsimple Helper EH.app and cefsimple Helper NP.app are created
              # from cefsimple Helper.app.
              #
              # The EH helper is marked for an executable heap. The NP helper
              # is marked for no PIE (ASLR).
              'postbuild_name': 'Make More Helpers',
              'action': [
                '../build/mac/make_more_helpers.sh',
                'Frameworks',
                'cefsimple',
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
            '<@(cefsimple_sources_mac)',
          ],
        }],
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            'libcef',
          ],
          'link_settings': {
            'libraries': [
              '-lX11',
            ],
          },
          'sources': [
            '<@(includes_linux)',
            '<@(cefsimple_sources_linux)',
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
        '<(DEPTH)/third_party/zlib/google/zip.gyp:zip',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
        'libcef_dll_wrapper',
      ],
      'sources': [
        'tests/cefclient/browser/client_app_browser.cc',
        'tests/cefclient/browser/client_app_browser.h',
        'tests/cefclient/browser/resource_util.h',
        'tests/cefclient/common/client_app.cc',
        'tests/cefclient/common/client_app.h',
        'tests/cefclient/common/client_app_other.cc',
        'tests/cefclient/common/client_app_other.h',
        'tests/cefclient/common/client_switches.cc',
        'tests/cefclient/common/client_switches.h',
        'tests/cefclient/renderer/client_app_renderer.cc',
        'tests/cefclient/renderer/client_app_renderer.h',
        'tests/cefclient/resources/osr_test.html',
        'tests/unittests/browser_info_map_unittest.cc',
        'tests/unittests/command_line_unittest.cc',
        'tests/unittests/cookie_unittest.cc',
        'tests/unittests/dialog_unittest.cc',
        'tests/unittests/display_unittest.cc',
        'tests/unittests/dom_unittest.cc',
        'tests/unittests/download_unittest.cc',
        'tests/unittests/draggable_regions_unittest.cc',
        'tests/unittests/frame_unittest.cc',
        'tests/unittests/geolocation_unittest.cc',
        'tests/unittests/jsdialog_unittest.cc',
        'tests/unittests/life_span_unittest.cc',
        'tests/unittests/message_router_unittest.cc',
        'tests/unittests/navigation_unittest.cc',
        'tests/unittests/os_rendering_unittest.cc',
        'tests/unittests/parser_unittest.cc',
        'tests/unittests/print_unittest.cc',
        'tests/unittests/process_message_unittest.cc',
        'tests/unittests/request_context_unittest.cc',
        'tests/unittests/request_handler_unittest.cc',
        'tests/unittests/request_unittest.cc',
        'tests/unittests/resource_manager_unittest.cc',
        'tests/unittests/routing_test_handler.cc',
        'tests/unittests/routing_test_handler.h',
        'tests/unittests/run_all_unittests.cc',
        'tests/unittests/scheme_handler_unittest.cc',
        'tests/unittests/stream_unittest.cc',
        'tests/unittests/stream_resource_handler_unittest.cc',
        'tests/unittests/string_unittest.cc',
        'tests/unittests/client_app_delegates.cc',
        'tests/unittests/task_unittest.cc',
        'tests/unittests/test_handler.cc',
        'tests/unittests/test_handler.h',
        'tests/unittests/test_suite.cc',
        'tests/unittests/test_suite.h',
        'tests/unittests/test_util.cc',
        'tests/unittests/test_util.h',
        'tests/unittests/tracing_unittest.cc',
        'tests/unittests/translator_unittest.cc',
        'tests/unittests/urlrequest_unittest.cc',
        'tests/unittests/v8_unittest.cc',
        'tests/unittests/values_unittest.cc',
        'tests/unittests/version_unittest.cc',
        'tests/unittests/xml_reader_unittest.cc',
        'tests/unittests/zip_reader_unittest.cc',
      ],
      'mac_bundle_resources': [
        'tests/cefclient/resources/osr_test.html',
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
        # Necessary to allow unittest files to access cefclient files.
        'tests',
      ],
      'conditions': [
        [ 'OS=="win"', {
          'dependencies': [
            'cef_sandbox',
            'libcef',
          ],
          'sources': [
            'tests/cefclient/browser/resource_util_win.cc',
            'tests/cefclient/resources/win/cefclient.rc',
          ],
          'msvs_settings': {
            'VCManifestTool': {
              'AdditionalManifestFiles': [
                'tests/cefclient/resources/win/cefclient.exe.manifest',
              ],
            },
          },
        }],
        [ 'OS=="mac"', {
          'product_name': 'cef_unittests',
          'dependencies': [
            'cef_framework',
            'cef_unittests_helper_app',
          ],
          'variables': {
            'PRODUCT_NAME': 'cef_unittests',
          },
          'run_as': {
            'action': ['${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.app/Contents/MacOS/${PRODUCT_NAME}'],
          },
          'copies': [
            {
              # Add the helper app.
              'destination': '<(PRODUCT_DIR)/<(PRODUCT_NAME).app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/<(PRODUCT_NAME) Helper.app',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(framework_name).framework',
              'action': [
                '../build/mac/copy_framework_unversioned.sh',
                '${BUILT_PRODUCTS_DIR}/<(framework_name).framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
              ],
            },
            {
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/<(framework_name)',
                '@executable_path/../Frameworks/<(framework_name).framework/<(framework_name)',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              'postbuild_name': 'Copy locale Resources',
              'action': [
                'cp',
                '-Rf',
                '${BUILT_PRODUCTS_DIR}/locales/',
                '${BUILT_PRODUCTS_DIR}/${PRODUCT_NAME}.app/Contents/Frameworks/<(framework_name).framework/Resources/'
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
            'tests/cefclient/browser/resource_util_mac.mm',
            'tests/cefclient/browser/resource_util_posix.cc',
            'tests/unittests/os_rendering_unittest_mac.h',
            'tests/unittests/os_rendering_unittest_mac.mm',
            'tests/unittests/run_all_unittests_mac.mm',
          ],
        }],
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            'libcef',
          ],
          'sources': [
            'tests/cefclient/browser/resource_util_linux.cc',
            'tests/cefclient/browser/resource_util_posix.cc',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/files',
              'files': [
                'tests/cefclient/resources/osr_test.html',
              ],
            },
          ],
          # Need to fix their ODR violations. See issue #1604.
          'ldflags!': [
            '-Wl,--detect-odr-violations',
          ],
        }],
      ],
    },
    {
      'target_name': 'libcef_dll_wrapper',
      'type': 'static_library',
      'msvs_guid': 'A9D6DC71-C0DC-4549-AEA0-3B15B44E86A9',
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
      'conditions': [
        [ 'OS=="mac"', {
          'dependencies': [
            'cef_framework',
          ],
        }, {  # OS!="mac"
          'dependencies': [
            'libcef',
          ],
        }],
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
        '<(DEPTH)/ui/strings/ui_strings.gyp:ui_strings',
        '<(DEPTH)/components/components_strings.gyp:components_strings',
        '<(DEPTH)/content/app/strings/content_strings.gyp:content_strings',
        '<(DEPTH)/extensions/extensions_strings.gyp:extensions_strings',
        'cef_strings',
      ],
      'actions': [
        {
          'action_name': 'repack_locales',
          'inputs': [
            'tools/repack_locales.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/cef_repack_locales.stamp',
          ],
          'action': [
            'python',
            'tools/repack_locales.py',
            '-g', '<(grit_out_dir)',
            '-s', '<(SHARED_INTERMEDIATE_DIR)',
            '-x', '<(PRODUCT_DIR)/locales',
            '<@(locales)',
          ],
        },
      ],
    },
    {
      'target_name': 'about_credits',
      'type': 'none',
      'actions': [
        {
          'variables': {
            'generator_path': '../tools/licenses.py',
          },
          'action_name': 'generate_about_credits',
          'inputs': [
            # TODO(phajdan.jr): make licenses.py print inputs too.
            '<(generator_path)',
          ],
          'outputs': [
            '<(about_credits_file)',
          ],
          'hard_dependency': 1,
          'action': ['python',
                     '<(generator_path)',
                     'credits',
                     '<(about_credits_file)',
          ],
          'message': 'Generating about:credits.',
        },
      ],
    },
    {
      # Create the pack file for CEF resources.
      'target_name': 'cef_resources',
      'type': 'none',
      'dependencies': [
        'about_credits',
      ],
      'actions': [
        {
          'action_name': 'cef_resources',
          'variables': {
            'grit_grd_file': 'libcef/resources/cef_resources.grd',
            'grit_additional_defines': [
              '-E', 'about_credits_file=<(about_credits_file)',
            ],
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
      # Create the pack file for component extension resources.
      'target_name': 'component_extension_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'component_extension_resources',
          'variables': {
            'grit_grd_file': '../chrome/browser/resources/component_extension_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(grit_out_dir)/component_extension_resources.pak'
          ],
        },
      ],
    },
    {
      # Combine all extensions-related non-localized pack file resources into a
      # single CEF pack file. Scaled resources are still in cef_pak.
      'target_name': 'cef_extensions_pak',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/extensions/extensions_resources.gyp:extensions_resources',
        '<(DEPTH)/extensions/extensions_strings.gyp:extensions_strings',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
        'component_extension_resources',
      ],
      'variables': {
        'make_pack_header_path': 'tools/make_pack_header.py',
      },
      'actions': [
        {
          'action_name': 'repack_cef_extensions_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_renderer_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
              '<(grit_out_dir)/component_extension_resources.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/cef_extensions.pak',
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
      ],
    },
    {
      # Combine all non-localized pack file resources into a single CEF pack file.
      'target_name': 'cef_pak',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/components/components_resources.gyp:components_resources',
        '<(DEPTH)/components/components_strings.gyp:components_strings',
        '<(DEPTH)/content/app/resources/content_resources.gyp:content_resources',
        '<(DEPTH)/content/app/strings/content_strings.gyp:content_strings',
        '<(DEPTH)/content/browser/devtools/devtools_resources.gyp:devtools_resources',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
        'cef_extensions_pak',
        'cef_locales',
        'cef_resources',
      ],
      'variables': {
        'make_pack_header_path': 'tools/make_pack_header.py',
      },
      'actions': [
        {
          'action_name': 'repack_cef_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(grit_out_dir)/cef_resources.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/cef.pak',
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
        {
          'action_name': 'repack_cef_100_percent_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_image_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_browser_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/cef_100_percent.pak',
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
        {
          'action_name': 'repack_cef_200_percent_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_image_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/extensions_browser_resources_200_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_200_percent.pak',
            ],
            'pak_output': '<(PRODUCT_DIR)/cef_200_percent.pak',
          },
          'includes': [ '../build/repack_action.gypi' ],
        },
        {
          'action_name': 'make_pack_resources_header',
          'variables': {
            'header_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/blink/grit/devtools_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/grit/blink_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/content/grit/content_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/grit/extensions_browser_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/grit/extensions_renderer_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/grit/extensions_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/net/grit/net_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/grit/ui_resources.h',
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/grit/webui_resources.h',
              '<(grit_out_dir)/grit/cef_resources.h',
              '<(grit_out_dir)/grit/component_extension_resources.h',
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
              '<(SHARED_INTERMEDIATE_DIR)/components/strings/grit/components_strings.h',
              '<(SHARED_INTERMEDIATE_DIR)/content/app/strings/grit/content_strings.h',
              '<(SHARED_INTERMEDIATE_DIR)/extensions/strings/grit/extensions_strings.h',
              '<(SHARED_INTERMEDIATE_DIR)/ui/strings/grit/ui_strings.h',
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
            '<(SHARED_INTERMEDIATE_DIR)/blink/devtools_resources.pak',
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
        '<(DEPTH)/third_party/WebKit/public/platform',
        '<(DEPTH)/third_party/WebKit/public/web',
        # CEF grit resource includes
        '<(DEPTH)/cef/libcef/resources/grit_stub',
        '<(grit_out_dir)',
        '<(SHARED_INTERMEDIATE_DIR)/ui/resources',
        '<(SHARED_INTERMEDIATE_DIR)/ui/strings',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_prefs',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/cc/blink/cc_blink.gyp:cc_blink',
        '<(DEPTH)/cc/cc.gyp:cc',
        # Generate chrome/common/safe_browsing/csd.pb.h required by
        # zip_analyzer_results.h via chrome_utility_messages.h
        '<(DEPTH)/chrome/chrome.gyp:safe_browsing_proto',
        '<(DEPTH)/components/components.gyp:cdm_renderer',
        '<(DEPTH)/components/components.gyp:component_updater',
        '<(DEPTH)/components/components.gyp:crash_component_breakpad_mac_to_be_deleted',
        '<(DEPTH)/components/components.gyp:crx_file',
        '<(DEPTH)/components/components.gyp:devtools_discovery',
        '<(DEPTH)/components/components.gyp:devtools_http_handler',
        '<(DEPTH)/components/components.gyp:keyed_service_content',
        '<(DEPTH)/components/components.gyp:keyed_service_core',
        '<(DEPTH)/components/components.gyp:navigation_interception',
        '<(DEPTH)/components/components.gyp:pdf_browser',
        '<(DEPTH)/components/components.gyp:pdf_renderer',
        '<(DEPTH)/components/components.gyp:plugins_renderer',
        '<(DEPTH)/components/components.gyp:pref_registry',
        '<(DEPTH)/components/components.gyp:printing_browser',
        '<(DEPTH)/components/components.gyp:printing_common',
        '<(DEPTH)/components/components.gyp:printing_renderer',
        '<(DEPTH)/components/components.gyp:proxy_config',
        '<(DEPTH)/components/components.gyp:update_client',
        '<(DEPTH)/components/components.gyp:user_prefs',
        '<(DEPTH)/components/components.gyp:version_info',
        '<(DEPTH)/components/components.gyp:web_cache_renderer',
        '<(DEPTH)/components/url_formatter/url_formatter.gyp:url_formatter',
        '<(DEPTH)/content/content.gyp:content_app_both',
        '<(DEPTH)/content/content.gyp:content_browser',
        '<(DEPTH)/content/content.gyp:content_common',
        '<(DEPTH)/content/content.gyp:content_gpu',
        '<(DEPTH)/content/content.gyp:content_plugin',
        '<(DEPTH)/content/content.gyp:content_ppapi_plugin',
        '<(DEPTH)/content/content.gyp:content_renderer',
        '<(DEPTH)/content/content.gyp:content_resources',
        '<(DEPTH)/content/content.gyp:content_utility',
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/device/core/core.gyp:device_core',
        '<(DEPTH)/device/hid/hid.gyp:device_hid',
        '<(DEPTH)/extensions/browser/api/api_registration.gyp:extensions_api_registration',
        '<(DEPTH)/extensions/common/api/api.gyp:extensions_api',
        '<(DEPTH)/extensions/extensions.gyp:extensions_browser',
        '<(DEPTH)/extensions/extensions.gyp:extensions_renderer',
        '<(DEPTH)/extensions/extensions.gyp:extensions_utility',
        '<(DEPTH)/extensions/extensions_resources.gyp:extensions_resources',
        '<(DEPTH)/gpu/gpu.gyp:gpu',
        '<(DEPTH)/ipc/ipc.gyp:ipc',
        '<(DEPTH)/media/blink/media_blink.gyp:media_blink',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/net/net.gyp:net_browser_services',
        '<(DEPTH)/net/net.gyp:net_with_v8',
        '<(DEPTH)/pdf/pdf.gyp:pdf',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/storage/storage_browser.gyp:storage',
        '<(DEPTH)/sync/sync.gyp:sync',
        '<(DEPTH)/third_party/hunspell/hunspell.gyp:hunspell',
        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/third_party/WebKit/Source/core/core.gyp:webcore',
        '<(DEPTH)/third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
        '<(DEPTH)/third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
        '<(DEPTH)/third_party/zlib/zlib.gyp:minizip',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
        '<(DEPTH)/ui/base/ime/ui_base_ime.gyp:ui_base_ime',
        '<(DEPTH)/ui/base/ui_base.gyp:ui_base',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        # Necessary to generate the grit include files.
        'cef_pak',
        # Necessary to generate API bindings for extensions.
        'libcef/browser/extensions/api/api_registration.gyp:cef_api_registration',
        'libcef/common/extensions/api/api.gyp:cef_api',
      ],
      'sources': [
        '<@(includes_common)',
        'libcef/browser/browser_context.cc',
        'libcef/browser/browser_context.h',
        'libcef/browser/browser_context_impl.cc',
        'libcef/browser/browser_context_impl.h',
        'libcef/browser/browser_context_proxy.cc',
        'libcef/browser/browser_context_proxy.h',
        'libcef/browser/browser_host_impl.cc',
        'libcef/browser/browser_host_impl.h',
        'libcef/browser/browser_info.cc',
        'libcef/browser/browser_info.h',
        'libcef/browser/browser_main.cc',
        'libcef/browser/browser_main.h',
        'libcef/browser/browser_message_filter.cc',
        'libcef/browser/browser_message_filter.h',
        'libcef/browser/browser_message_loop.cc',
        'libcef/browser/browser_message_loop.h',
        'libcef/browser/browser_pref_store.cc',
        'libcef/browser/browser_pref_store.h',
        'libcef/browser/browser_settings.cc',
        'libcef/browser/browser_settings.h',
        'libcef/browser/browser_urlrequest_impl.cc',
        'libcef/browser/browser_urlrequest_impl.h',
        'libcef/browser/chrome_browser_process_stub.cc',
        'libcef/browser/chrome_browser_process_stub.h',
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
        'libcef/browser/cookie_store_proxy.cc',
        'libcef/browser/cookie_store_proxy.h',
        'libcef/browser/devtools_delegate.cc',
        'libcef/browser/devtools_delegate.h',
        'libcef/browser/devtools_frontend.cc',
        'libcef/browser/devtools_frontend.h',
        'libcef/browser/devtools_scheme_handler.cc',
        'libcef/browser/devtools_scheme_handler.h',
        'libcef/browser/download_item_impl.cc',
        'libcef/browser/download_item_impl.h',
        'libcef/browser/download_manager_delegate.cc',
        'libcef/browser/download_manager_delegate.h',
        'libcef/browser/component_updater/cef_component_updater_configurator.cc',
        'libcef/browser/component_updater/cef_component_updater_configurator.h',
        'libcef/browser/extensions/api/streams_private/streams_private_api.cc',
        'libcef/browser/extensions/api/streams_private/streams_private_api.h',
        'libcef/browser/extensions/browser_context_keyed_service_factories.cc',
        'libcef/browser/extensions/browser_context_keyed_service_factories.h',
        'libcef/browser/extensions/browser_extensions_util.cc',
        'libcef/browser/extensions/browser_extensions_util.h',
        'libcef/browser/extensions/component_extension_resource_manager.cc',
        'libcef/browser/extensions/component_extension_resource_manager.h',
        'libcef/browser/extensions/event_router_forwarder.cc',
        'libcef/browser/extensions/event_router_forwarder.h',
        'libcef/browser/extensions/extensions_api_client.cc',
        'libcef/browser/extensions/extensions_api_client.h',
        'libcef/browser/extensions/extensions_browser_client.cc',
        'libcef/browser/extensions/extensions_browser_client.h',
        'libcef/browser/extensions/extension_system.cc',
        'libcef/browser/extensions/extension_system.h',
        'libcef/browser/extensions/extension_system_factory.cc',
        'libcef/browser/extensions/extension_system_factory.h',
        'libcef/browser/extensions/extension_web_contents_observer.cc',
        'libcef/browser/extensions/extension_web_contents_observer.h',
        'libcef/browser/extensions/mime_handler_view_guest_delegate.cc',
        'libcef/browser/extensions/mime_handler_view_guest_delegate.h',
        'libcef/browser/extensions/pdf_extension_util.cc',
        'libcef/browser/extensions/pdf_extension_util.h',
        'libcef/browser/extensions/pdf_web_contents_helper_client.cc',
        'libcef/browser/extensions/pdf_web_contents_helper_client.h',
        'libcef/browser/extensions/plugin_info_message_filter.cc',
        'libcef/browser/extensions/plugin_info_message_filter.h',
        'libcef/browser/extensions/url_request_util.cc',
        'libcef/browser/extensions/url_request_util.h',
        'libcef/browser/frame_host_impl.cc',
        'libcef/browser/frame_host_impl.h',
        'libcef/browser/geolocation_impl.cc',
        'libcef/browser/internal_scheme_handler.cc',
        'libcef/browser/internal_scheme_handler.h',
        'libcef/browser/javascript_dialog.h',
        'libcef/browser/javascript_dialog_manager.cc',
        'libcef/browser/javascript_dialog_manager.h',
        'libcef/browser/media_capture_devices_dispatcher.cc',
        'libcef/browser/media_capture_devices_dispatcher.h',
        'libcef/browser/menu_creator.cc',
        'libcef/browser/menu_creator.h',
        'libcef/browser/menu_model_impl.cc',
        'libcef/browser/menu_model_impl.h',
        'libcef/browser/navigate_params.cc',
        'libcef/browser/navigate_params.h',
        'libcef/browser/navigation_entry_impl.cc',
        'libcef/browser/navigation_entry_impl.h',
        'libcef/browser/origin_whitelist_impl.cc',
        'libcef/browser/origin_whitelist_impl.h',
        'libcef/browser/path_util_impl.cc',
        'libcef/browser/pepper/browser_pepper_host_factory.cc',
        'libcef/browser/pepper/browser_pepper_host_factory.h',
        'libcef/browser/pepper/pepper_flash_browser_host.cc',
        'libcef/browser/pepper/pepper_flash_browser_host.h',
        'libcef/browser/pepper/pepper_isolated_file_system_message_filter.cc',
        'libcef/browser/pepper/pepper_isolated_file_system_message_filter.h',
        'libcef/browser/pepper/device_id_fetcher.cc',
        'libcef/browser/permission_manager.cc',
        'libcef/browser/permission_manager.h',
        'libcef/browser/print_settings_impl.cc',
        'libcef/browser/print_settings_impl.h',
        'libcef/browser/printing/printing_message_filter.cc',
        'libcef/browser/printing/printing_message_filter.h',
        'libcef/browser/printing/print_view_manager.cc',
        'libcef/browser/printing/print_view_manager.h',
        'libcef/browser/printing/print_view_manager_base.cc',
        'libcef/browser/printing/print_view_manager_base.h',
        'libcef/browser/process_util_impl.cc',
        'libcef/browser/proxy_stubs.cc',
        'libcef/browser/render_widget_host_view_osr.cc',
        'libcef/browser/render_widget_host_view_osr.h',
        'libcef/browser/resource_context.cc',
        'libcef/browser/resource_context.h',
        'libcef/browser/resource_dispatcher_host_delegate.cc',
        'libcef/browser/resource_dispatcher_host_delegate.h',
        'libcef/browser/resource_request_job.cc',
        'libcef/browser/resource_request_job.h',
        'libcef/browser/request_context_impl.cc',
        'libcef/browser/request_context_impl.h',
        'libcef/browser/scheme_handler.cc',
        'libcef/browser/scheme_handler.h',
        'libcef/browser/scheme_impl.cc',
        'libcef/browser/software_output_device_osr.cc',
        'libcef/browser/software_output_device_osr.h',
        'libcef/browser/speech_recognition_manager_delegate.cc',
        'libcef/browser/speech_recognition_manager_delegate.h',
        'libcef/browser/ssl_cert_principal_impl.cc',
        'libcef/browser/ssl_cert_principal_impl.h',
        'libcef/browser/ssl_host_state_delegate.cc',
        'libcef/browser/ssl_host_state_delegate.h',
        'libcef/browser/ssl_info_impl.cc',
        'libcef/browser/ssl_info_impl.h',
        'libcef/browser/stream_impl.cc',
        'libcef/browser/stream_impl.h',
        'libcef/browser/trace_impl.cc',
        'libcef/browser/trace_subscriber.cc',
        'libcef/browser/trace_subscriber.h',
        'libcef/browser/thread_util.h',
        'libcef/browser/url_network_delegate.cc',
        'libcef/browser/url_network_delegate.h',
        'libcef/browser/url_request_context.cc',
        'libcef/browser/url_request_context.h',
        'libcef/browser/url_request_context_getter.h',
        'libcef/browser/url_request_context_getter_impl.cc',
        'libcef/browser/url_request_context_getter_impl.h',
        'libcef/browser/url_request_context_getter_proxy.cc',
        'libcef/browser/url_request_context_getter_proxy.h',
        'libcef/browser/url_request_context_impl.h',
        'libcef/browser/url_request_context_proxy.cc',
        'libcef/browser/url_request_context_proxy.h',
        'libcef/browser/url_request_manager.cc',
        'libcef/browser/url_request_manager.h',
        'libcef/browser/url_request_interceptor.cc',
        'libcef/browser/url_request_interceptor.h',
        'libcef/browser/url_request_user_data.cc',
        'libcef/browser/url_request_user_data.h',
        'libcef/browser/web_contents_view_osr.cc',
        'libcef/browser/web_contents_view_osr.h',
        'libcef/browser/web_plugin_impl.cc',
        'libcef/browser/web_plugin_impl.h',
        'libcef/browser/xml_reader_impl.cc',
        'libcef/browser/xml_reader_impl.h',
        'libcef/browser/zip_reader_impl.cc',
        'libcef/browser/zip_reader_impl.h',
        'libcef/common/base_impl.cc',
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
        'libcef/common/crash_reporter_client.cc',
        'libcef/common/crash_reporter_client.h',
        'libcef/common/drag_data_impl.cc',
        'libcef/common/drag_data_impl.h',
        'libcef/common/extensions/extensions_client.cc',
        'libcef/common/extensions/extensions_client.h',
        'libcef/common/extensions/extensions_util.cc',
        'libcef/common/extensions/extensions_util.h',
        'libcef/common/http_header_utils.cc',
        'libcef/common/http_header_utils.h',
        'libcef/common/json_impl.cc',
        'libcef/common/main_delegate.cc',
        'libcef/common/main_delegate.h',
        'libcef/common/net_resource_provider.cc',
        'libcef/common/net_resource_provider.h',
        'libcef/common/parser_impl.cc',
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
        'libcef/common/scheme_registration.cc',
        'libcef/common/scheme_registration.h',
        'libcef/common/string_list_impl.cc',
        'libcef/common/string_map_impl.cc',
        'libcef/common/string_multimap_impl.cc',
        'libcef/common/string_types_impl.cc',
        'libcef/common/task_impl.cc',
        'libcef/common/task_runner_impl.cc',
        'libcef/common/task_runner_impl.h',
        'libcef/common/test/translator_test_impl.cc',
        'libcef/common/time_impl.cc',
        'libcef/common/time_util.h',
        'libcef/common/tracker.cc',
        'libcef/common/tracker.h',
        'libcef/common/urlrequest_impl.cc',
        'libcef/common/value_base.cc',
        'libcef/common/value_base.h',
        'libcef/common/values_impl.cc',
        'libcef/common/values_impl.h',
        'libcef/common/upload_data.cc',
        'libcef/common/upload_data.h',
        'libcef/common/upload_element.cc',
        'libcef/common/upload_element.h',
        'libcef/renderer/browser_impl.cc',
        'libcef/renderer/browser_impl.h',
        'libcef/renderer/content_renderer_client.cc',
        'libcef/renderer/content_renderer_client.h',
        'libcef/renderer/dom_document_impl.cc',
        'libcef/renderer/dom_document_impl.h',
        'libcef/renderer/dom_node_impl.cc',
        'libcef/renderer/dom_node_impl.h',
        'libcef/renderer/extensions/extensions_dispatcher_delegate.cc',
        'libcef/renderer/extensions/extensions_dispatcher_delegate.h',
        'libcef/renderer/extensions/extensions_renderer_client.cc',
        'libcef/renderer/extensions/extensions_renderer_client.h',
        'libcef/renderer/extensions/print_web_view_helper_delegate.cc',
        'libcef/renderer/extensions/print_web_view_helper_delegate.h',
        'libcef/renderer/frame_impl.cc',
        'libcef/renderer/frame_impl.h',
        'libcef/renderer/media/cef_key_systems.cc',
        'libcef/renderer/media/cef_key_systems.h',
        'libcef/renderer/pepper/pepper_helper.cc',
        'libcef/renderer/pepper/pepper_helper.h',
        'libcef/renderer/pepper/pepper_uma_host.cc',
        'libcef/renderer/pepper/renderer_pepper_host_factory.cc',
        'libcef/renderer/pepper/renderer_pepper_host_factory.h',
        'libcef/renderer/render_frame_observer.cc',
        'libcef/renderer/render_frame_observer.h',
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
        'libcef/utility/content_utility_client.cc',
        'libcef/utility/content_utility_client.h',
        '<(DEPTH)/chrome/common/chrome_switches.cc',
        '<(DEPTH)/chrome/common/chrome_switches.h',
        # Include sources for proxy support.
        '<(DEPTH)/base/prefs/testing_pref_store.cc',
        '<(DEPTH)/base/prefs/testing_pref_store.h',
        '<(DEPTH)/chrome/browser/net/pref_proxy_config_tracker.cc',
        '<(DEPTH)/chrome/browser/net/pref_proxy_config_tracker.h',
        '<(DEPTH)/chrome/browser/net/pref_proxy_config_tracker_impl.cc',
        '<(DEPTH)/chrome/browser/net/pref_proxy_config_tracker_impl.h',
        '<(DEPTH)/chrome/browser/net/proxy_service_factory.cc',
        '<(DEPTH)/chrome/browser/net/proxy_service_factory.h',
        '<(DEPTH)/chrome/browser/net/utility_process_mojo_proxy_resolver_factory.cc',
        '<(DEPTH)/chrome/browser/net/utility_process_mojo_proxy_resolver_factory.h',
        '<(DEPTH)/chrome/browser/prefs/command_line_pref_store.cc',
        '<(DEPTH)/chrome/browser/prefs/command_line_pref_store.h',
        '<(DEPTH)/chrome/common/pref_names.cc',
        '<(DEPTH)/chrome/common/pref_names.h',
        '<(DEPTH)/components/data_reduction_proxy/core/common/data_reduction_proxy_switches.cc',
        '<(DEPTH)/components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h',
        '<(DEPTH)/components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.cc',
        '<(DEPTH)/components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h',
        # Include sources for the loadtimes V8 extension.
        '<(DEPTH)/chrome/renderer/loadtimes_extension_bindings.h',
        '<(DEPTH)/chrome/renderer/loadtimes_extension_bindings.cc',
        # Include sources for printing.
        '<(DEPTH)/chrome/browser/printing/print_job.cc',
        '<(DEPTH)/chrome/browser/printing/print_job.h',
        '<(DEPTH)/chrome/browser/printing/print_job_manager.cc',
        '<(DEPTH)/chrome/browser/printing/print_job_manager.h',
        '<(DEPTH)/chrome/browser/printing/print_job_worker.cc',
        '<(DEPTH)/chrome/browser/printing/print_job_worker.h',
        '<(DEPTH)/chrome/browser/printing/print_job_worker_owner.cc',
        '<(DEPTH)/chrome/browser/printing/print_job_worker_owner.h',
        '<(DEPTH)/chrome/browser/printing/printer_query.cc',
        '<(DEPTH)/chrome/browser/printing/printer_query.h',
        '<(DEPTH)/chrome/common/extensions/extension_constants.cc',
        '<(DEPTH)/chrome/common/extensions/extension_constants.h',
        # Include header for stub creation (BrowserProcess) so print_job_worker can
        # determine the current locale.
        '<(DEPTH)/chrome/browser/browser_process.cc',
        '<(DEPTH)/chrome/browser/browser_process.h',
        # Include sources for spell checking support.
        '<(DEPTH)/chrome/browser/spellchecker/feedback.cc',
        '<(DEPTH)/chrome/browser/spellchecker/feedback.h',
        '<(DEPTH)/chrome/browser/spellchecker/feedback_sender.cc',
        '<(DEPTH)/chrome/browser/spellchecker/feedback_sender.h',
        '<(DEPTH)/chrome/browser/spellchecker/misspelling.cc',
        '<(DEPTH)/chrome/browser/spellchecker/misspelling.h',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_action.cc',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_action.h',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_custom_dictionary.cc',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_custom_dictionary.h',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_factory.cc',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_factory.h',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_host_metrics.cc',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_host_metrics.h',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_hunspell_dictionary.cc',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter.cc',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter.h',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_service.cc',
        '<(DEPTH)/chrome/browser/spellchecker/spellcheck_service.h',
        '<(DEPTH)/chrome/browser/spellchecker/spelling_service_client.cc',
        '<(DEPTH)/chrome/browser/spellchecker/spelling_service_client.h',
        '<(DEPTH)/chrome/browser/spellchecker/word_trimmer.cc',
        '<(DEPTH)/chrome/browser/spellchecker/word_trimmer.h',
        '<(DEPTH)/chrome/common/chrome_constants.cc',
        '<(DEPTH)/chrome/common/chrome_constants.h',
        '<(DEPTH)/chrome/common/spellcheck_common.cc',
        '<(DEPTH)/chrome/common/spellcheck_common.h',
        '<(DEPTH)/chrome/common/spellcheck_marker.h',
        '<(DEPTH)/chrome/common/spellcheck_messages.h',
        '<(DEPTH)/chrome/common/spellcheck_result.h',
        '<(DEPTH)/chrome/renderer/spellchecker/custom_dictionary_engine.cc',
        '<(DEPTH)/chrome/renderer/spellchecker/custom_dictionary_engine.h',
        '<(DEPTH)/chrome/renderer/spellchecker/hunspell_engine.cc',
        '<(DEPTH)/chrome/renderer/spellchecker/hunspell_engine.h',
        '<(DEPTH)/chrome/renderer/spellchecker/spellcheck.cc',
        '<(DEPTH)/chrome/renderer/spellchecker/spellcheck.h',
        '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_language.cc',
        '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_language.h',
        '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_provider.cc',
        '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_provider.h',
        '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_worditerator.cc',
        '<(DEPTH)/chrome/renderer/spellchecker/spellcheck_worditerator.h',
        '<(DEPTH)/chrome/renderer/spellchecker/spelling_engine.h',
        # Include sources for pepper flash support.
        '<(DEPTH)/chrome/common/pepper_flash.cc',
        '<(DEPTH)/chrome/common/pepper_flash.h',
        '<(DEPTH)/chrome/common/ppapi_utils.cc',
        '<(DEPTH)/chrome/common/ppapi_utils.h',
        '<(DEPTH)/chrome/browser/renderer_host/pepper/pepper_flash_clipboard_message_filter.cc',
        '<(DEPTH)/chrome/browser/renderer_host/pepper/pepper_flash_clipboard_message_filter.h',
        '<(DEPTH)/chrome/browser/renderer_host/pepper/pepper_flash_drm_host.cc',
        '<(DEPTH)/chrome/browser/renderer_host/pepper/pepper_flash_drm_host.h',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_drm_renderer_host.cc',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_drm_renderer_host.h',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_font_file_host.cc',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_font_file_host.h',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_fullscreen_host.cc',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_fullscreen_host.h',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_menu_host.cc',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_menu_host.h',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_renderer_host.cc',
        '<(DEPTH)/chrome/renderer/pepper/pepper_flash_renderer_host.h',
        # Include sources required by chrome_utility_messages.h.
        '<(DEPTH)/chrome/common/safe_browsing/zip_analyzer_results.h',
        '<(DEPTH)/chrome/common/safe_browsing/zip_analyzer_results.cc',
        # Include sources for pepper PDF support.
        '<(DEPTH)/chrome/child/pdf_child_init.cc',
        '<(DEPTH)/chrome/child/pdf_child_init.h',
        '<(DEPTH)/chrome/renderer/pepper/chrome_pdf_print_client.cc',
        '<(DEPTH)/chrome/renderer/pepper/chrome_pdf_print_client.h',
        # Include sources for extensions support.
        '<(DEPTH)/chrome/common/pepper_permission_util.cc',
        '<(DEPTH)/chrome/common/pepper_permission_util.h',
        '<(DEPTH)/extensions/shell/browser/shell_display_info_provider.cc',
        '<(DEPTH)/extensions/shell/browser/shell_display_info_provider.h',
        '<(DEPTH)/extensions/shell/browser/shell_web_contents_modal_dialog_manager.cc',
        '<(grit_out_dir)/grit/component_extension_resources_map.cc',
        # Include sources for component-updater support.
        '<(DEPTH)/chrome/browser/component_updater/widevine_cdm_component_installer.cc',
        '<(DEPTH)/chrome/browser/component_updater/widevine_cdm_component_installer.h',
        # Include sources for widevine support.
        '<(DEPTH)/chrome/common/widevine_cdm_constants.cc',
        '<(DEPTH)/chrome/common/widevine_cdm_constants.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            '<@(includes_win)',
            'libcef/browser/browser_host_impl_win.cc',
            'libcef/browser/browser_main_win.cc',
            'libcef/browser/javascript_dialog_win.cc',
            'libcef/browser/menu_creator_runner_win.cc',
            'libcef/browser/menu_creator_runner_win.h',
            'libcef/browser/render_widget_host_view_osr_win.cc',
            # Include sources for printing using PDF.
            'libcef/utility/printing_handler.cc',
            'libcef/utility/printing_handler.h',
            '<(DEPTH)/chrome/browser/printing/pdf_to_emf_converter.cc',
            '<(DEPTH)/chrome/browser/printing/pdf_to_emf_converter.h',
            '<(DEPTH)/chrome/common/chrome_utility_printing_messages.h',
            # Include sources for font cache.
            '<(DEPTH)/chrome/utility/font_cache_handler_win.cc',
            '<(DEPTH)/chrome/utility/font_cache_handler_win.h',
          ],
        }],
        [ 'OS=="mac"', {
          'sources': [
            '<@(includes_mac)',
            'libcef/browser/browser_host_impl_mac.mm',
            'libcef/browser/javascript_dialog_mac.mm',
            'libcef/browser/menu_creator_runner_mac.h',
            'libcef/browser/menu_creator_runner_mac.mm',
            'libcef/browser/render_widget_host_view_osr_mac.mm',
            'libcef/browser/text_input_client_osr_mac.mm',
            'libcef/browser/text_input_client_osr_mac.h',
            'libcef/common/util_mac.h',
            'libcef/common/util_mac.mm',
            # Include sources for spell checking support.
            '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter_platform.h',
            '<(DEPTH)/chrome/browser/spellchecker/spellcheck_message_filter_platform_mac.cc',
            '<(DEPTH)/chrome/browser/spellchecker/spellcheck_platform.h',
            '<(DEPTH)/chrome/browser/spellchecker/spellcheck_platform_mac.mm',
            '<(DEPTH)/chrome/renderer/spellchecker/platform_spelling_engine.cc',
            '<(DEPTH)/chrome/renderer/spellchecker/platform_spelling_engine.h',
            # Include sources for pepper flash support.
            '<(DEPTH)/chrome/browser/renderer_host/pepper/monitor_finder_mac.h',
            '<(DEPTH)/chrome/browser/renderer_host/pepper/monitor_finder_mac.mm',
          ],
        }],
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'sources': [
            '<@(includes_linux)',
            'libcef/browser/browser_host_impl_linux.cc',
            'libcef/browser/javascript_dialog_linux.cc',
            'libcef/browser/menu_creator_runner_linux.cc',
            'libcef/browser/menu_creator_runner_linux.h',
            'libcef/browser/printing/print_dialog_linux.cc',
            'libcef/browser/printing/print_dialog_linux.h',
            'libcef/browser/render_widget_host_view_osr_linux.cc',
            'libcef/browser/window_x11.cc',
            'libcef/browser/window_x11.h',
          ],
        }],
        ['os_posix == 1 and OS != "mac"', {
          'dependencies': [
            '<(DEPTH)/components/components.gyp:breakpad_host',
          ],
        }],
        ['cld_version==2', {
          'dependencies': [
            '<(DEPTH)/third_party/cld_2/cld_2.gyp:cld2_platform_impl',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '<(DEPTH)/ui/views/controls/webview/webview.gyp:webview',
            '<(DEPTH)/ui/views/views.gyp:views',
          ],
          'sources': [
            'libcef/browser/window_delegate_view.cc',
            'libcef/browser/window_delegate_view.h',
            '<(DEPTH)/ui/views/test/desktop_test_views_delegate.h',
            '<(DEPTH)/ui/views/test/desktop_test_views_delegate_aura.cc',
            '<(DEPTH)/ui/views/test/test_views_delegate.h',
            '<(DEPTH)/ui/views/test/test_views_delegate_aura.cc',
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
          'target_name': 'cef_framework',
          'type': 'shared_library',
          'product_name': '<(framework_name)',
          'mac_bundle': 1,
          'mac_bundle_resources': [
            '<(PRODUCT_DIR)/cef.pak',
            '<(PRODUCT_DIR)/cef_100_percent.pak',
            '<(PRODUCT_DIR)/cef_200_percent.pak',
            '<(PRODUCT_DIR)/cef_extensions.pak',
            '<(PRODUCT_DIR)/devtools_resources.pak',
            '<(PRODUCT_DIR)/icudtl.dat',
            '<(PRODUCT_DIR)/natives_blob.bin',
            '<(PRODUCT_DIR)/snapshot_blob.bin',
            '<(PRODUCT_DIR)/widevinecdmadapter.plugin',
            'libcef/resources/framework-Info.plist',
          ],
          'mac_bundle_resources!': [
            'libcef/resources/framework-Info.plist',
          ],
          'xcode_settings': {
            # Default path that will be changed by install_name_tool in dependent targets.
            'INSTALL_PATH': '@executable_path',
            'DYLIB_INSTALL_NAME_BASE': '@executable_path',
            'LD_DYLIB_INSTALL_NAME': '@executable_path/<(framework_name)',

            # The libcef_static target contains ObjC categories. Passing the -ObjC flag
            # is necessary to properly load them and avoid a "selector not recognized"
            # runtime error. See http://developer.apple.com/library/mac/#qa/qa1490/_index.html
            # for more information.
            'OTHER_LDFLAGS': ['-Wl,-ObjC'],

            'DYLIB_COMPATIBILITY_VERSION': '<(version_mac_dylib)',
            'DYLIB_CURRENT_VERSION': '<(version_mac_dylib)',
            'INFOPLIST_FILE': 'libcef/resources/framework-Info.plist',
          },
          'dependencies': [
            'cef_pak',
            'libcef_static',
          ],
          'defines': [
            'BUILDING_CEF_SHARED',
          ],
          'include_dirs': [
            '.',
          ],
          'sources': [
            '<@(includes_common)',
            '<@(includes_capi)',
            '<@(libcef_sources_common)',
          ],
          'postbuilds': [
            {
              # Modify the Info.plist as needed.  The script explains why
              # this is needed.  This is also done in the chrome target.
              # The framework needs the Breakpad keys if this feature is
              # enabled.  It does not need the Keystone keys; these always
              # come from the outer application bundle.  The framework
              # doesn't currently use the SCM keys for anything,
              # but this seems like a really good place to store them.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--breakpad=1',
                         '--keystone=0',
                         '--scm=1',
                         '--version=<(chrome_version)',
                         '--branding=<(framework_name)'],
            },
          ],
          'copies': [
            {
              # Copy binaries for breakpad support.
              'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
              'files': [
                '<(PRODUCT_DIR)/crash_inspector',
                '<(PRODUCT_DIR)/crash_report_sender.app',
              ],
            },
          ],
        },  # target cef_framework
        {
          'target_name': 'cefclient_helper_app',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': 'cefclient Helper',
          'mac_bundle': 1,
          'dependencies': [
            'cef_framework',
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
            'tests/cefclient/resources/mac/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'tests/cefclient/resources/mac/helper-Info.plist',
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
                '@executable_path/<(framework_name)',
                '@executable_path/../../../../Frameworks/<(framework_name).framework/<(framework_name)',
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
          'target_name': 'cefsimple_helper_app',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': 'cefsimple Helper',
          'mac_bundle': 1,
          'dependencies': [
            'cef_framework',
            'libcef_dll_wrapper',
          ],
          'defines': [
            'USING_CEF_SHARED',
          ],
          'include_dirs': [
            '.',
            # cefsimple includes are relative to the tests directory to make
            # creation of binary releases easier.
            'tests'
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
            ],
          },
          'sources': [
            '<@(cefsimple_sources_mac_helper)',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list helper-Info.plist once, not the three times it
          # is listed here.
          'mac_bundle_resources!': [
            'tests/cefsimple/mac/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'tests/cefsimple/mac/helper-Info.plist',
            # Necessary to avoid an "install_name_tool: changing install names or
            # rpaths can't be redone" error.
            'OTHER_LDFLAGS': ['-Wl,-headerpad_max_install_names'],
          },
          'postbuilds': [
            {
              # The framework defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # cefsimple_helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/<(framework_name)',
                '@executable_path/../../../../Frameworks/<(framework_name).framework/<(framework_name)',
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
        },  # target cefsimple_helper_app
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
            'cef_framework',
            'libcef_dll_wrapper',
          ],
          'defines': [
            'USING_CEF_SHARED',
          ],
          'include_dirs': [
            '.',
            # Necessary to allow unittest files to access cefclient files.
            'tests',
          ],
          'sources': [
            'tests/cefclient/browser/client_app_browser.cc',
            'tests/cefclient/browser/client_app_browser.h',
            'tests/cefclient/common/client_app.cc',
            'tests/cefclient/common/client_app.h',
            'tests/cefclient/common/client_app_other.cc',
            'tests/cefclient/common/client_app_other.h',
            'tests/cefclient/common/client_switches.cc',
            'tests/cefclient/common/client_switches.h',
            'tests/cefclient/process_helper_mac.cc',
            'tests/cefclient/renderer/client_app_renderer.cc',
            'tests/cefclient/renderer/client_app_renderer.h',
            'tests/unittests/client_app_delegates.cc',
            'tests/unittests/cookie_unittest.cc',
            'tests/unittests/dom_unittest.cc',
            'tests/unittests/frame_unittest.cc',
            'tests/unittests/message_router_unittest.cc',
            'tests/unittests/navigation_unittest.cc',
            'tests/unittests/process_message_unittest.cc',
            'tests/unittests/request_handler_unittest.cc',
            'tests/unittests/request_unittest.cc',
            'tests/unittests/routing_test_handler.cc',
            'tests/unittests/routing_test_handler.h',
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
            'tests/cefclient/resources/mac/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'tests/cefclient/resources/mac/helper-Info.plist',
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
                '@executable_path/<(framework_name)',
                '@executable_path/../../../../Frameworks/<(framework_name).framework/<(framework_name)',
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
    }, {  # OS!="mac"
      'targets': [
      {
        'target_name': 'libcef',
        'type': 'shared_library',
        'msvs_guid': 'C13650D5-CF1A-4259-BE45-B1EBA6280E47',
        'dependencies': [
          'libcef_static',
        ],
        'defines': [
          'BUILDING_CEF_SHARED',
        ],
        'include_dirs': [
          '.',
        ],
        'sources': [
          '<@(includes_common)',
          '<@(includes_capi)',
          '<@(libcef_sources_common)',
        ],
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
              # TODO(cef): Remove ui_unscaled_resources.rc once custom cursor
              # resources can be loaded via ResourceBundle. See crbug.com/147663.
              '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_unscaled_resources.rc',
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
              'VCManifestTool': {
                'AdditionalManifestFiles': [
                  'libcef_dll/libcef.dll.manifest',
                ],
              },
            },
          }],
          [ '(OS=="linux" or OS=="freebsd" or OS=="openbsd") and use_allocator!="none"', {
            'dependencies':[
              '<(DEPTH)/base/allocator/allocator.gyp:allocator',
            ],
          }],
        ],
      }],
    }],  # OS!="mac"
    [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
      'targets': [
        {
          'target_name': 'gtk',
          'type': 'none',
          'variables': {
            # gtk requires gmodule, but it does not list it as a dependency
            # in some misconfigured systems.
            'gtk_packages': 'gmodule-2.0 gtk+-2.0 gthread-2.0 gtk+-unix-print-2.0',
          },
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags <(gtk_packages))',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other <(gtk_packages))',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l <(gtk_packages))',
            ],
          },
        },
        {
          'target_name': 'gtkglext',
          'type': 'none',
          'variables': {
            # gtkglext is required by the cefclient OSR example.
            'gtk_packages': 'gtkglext-1.0',
          },
          'direct_dependent_settings': {
            'cflags': [
              '<!@(pkg-config --cflags <(gtk_packages))',
            ],
          },
          'link_settings': {
            'ldflags': [
              '<!@(pkg-config --libs-only-L --libs-only-other <(gtk_packages))',
            ],
            'libraries': [
              '<!@(pkg-config --libs-only-l <(gtk_packages))',
            ],
          },
        },
      ],
    }],  # OS=="linux" or OS=="freebsd" or OS=="openbsd"
    [ 'OS=="win"', {
      'targets': [
        {
          'target_name': 'cef_sandbox',
          'type': 'static_library',
          'msvs_guid': 'C90B9CA2-2BD2-4140-9DB8-4474785FF360',
          'dependencies': [
            '<(DEPTH)/sandbox/sandbox.gyp:sandbox',
          ],
          'include_dirs': [
            '.',
          ],
          'sources': [
            'libcef_dll/sandbox/sandbox_win.cc',
          ],
        },
      ],
    }],  # OS=="win"
  ],
}
