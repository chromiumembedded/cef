# Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'linux_use_gold_binary': 0,
    'linux_use_gold_flags': 0,
    'conditions': [
      [ 'OS=="mac"', {
        # Don't use clang with CEF binary releases due to Chromium tree structure dependency.
        'clang': 0,
      }],
      ['sysroot!=""', {
        'pkg-config': './pkg-config-wrapper "<(sysroot)" "<(target_arch)"',
      }, {
        'pkg-config': 'pkg-config'
      }],
    ]
  },
  'includes': [
    # Bring in the source file lists for cefclient.
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
        'cefclient/mac/Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'cefclient/mac/Info.plist',
        # Target build path.
        'SYMROOT': 'xcodebuild',
      },
      'conditions': [
        ['OS=="win"', {
          'actions': [
            {
              'action_name': 'copy_resources',
              'msvs_cygwin_shell': 0,
              'inputs': [],
              'outputs': [
                '<(PRODUCT_DIR)/copy_resources.stamp',
              ],
              'action': [
                'xcopy /efy',
                'Resources\*',
                '$(OutDir)',
              ],
            },
            {
              'action_name': 'copy_libraries',
              'msvs_cygwin_shell': 0,
              'inputs': [],
              'outputs': [
                '<(PRODUCT_DIR)/copy_resources.stamp',
              ],
              'action': [
                'xcopy /efy',
                '$(ConfigurationName)\*.dll',
                '$(OutDir)',
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
              '-l$(ConfigurationName)/libcef.lib'
            ],
          },
          'sources': [
            '<@(includes_win)',
            '<@(cefclient_sources_win)',
          ],
        }],
        [ 'OS=="mac"', {
          'product_name': 'cefclient',
          'copies': [
            {
              # Add library dependencies to the bundle.
              'destination': '<(PRODUCT_DIR)/cefclient.app/Contents/MacOS/',
              'files': [
                '$(CONFIGURATION)/libcef.dylib',
              ],
            },
            {
              # Add other resources to the bundle.
              'destination': '<(PRODUCT_DIR)/cefclient.app/Contents/',
              'files': [
                'Resources/',
              ],
            },
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
              '$(CONFIGURATION)/libcef.dylib'
            ],
          },
          'sources': [
            '<@(includes_mac)',
            '<@(cefclient_sources_mac)',
          ],
        }],
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/files',
              'files': [
                '<@(cefclient_bundle_resources_linux)',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/',
              'files': [
                'Resources/chrome.pak',
                'Resources/devtools_resources.pak',
                'Resources/locales/',
                '$(BUILDTYPE)/libcef.so',
              ],
            },
          ],
          'dependencies': [
            'gtk',
          ],
          'link_settings': {
            'ldflags': [
              # Look for libcef.so in the current directory. Path can also be
              # specified using the LD_LIBRARY_PATH environment variable.
              '-Wl,-rpath,.',
            ],
            'libraries': [
              "$(BUILDTYPE)/libcef.so",
            ],
          },
          'sources': [
            '<@(includes_linux)',
            '<@(cefclient_sources_linux)',
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
      'xcode_settings': {
        # Target build path.
        'SYMROOT': 'xcodebuild',
      },
      'conditions': [
        [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            'gtk',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
      'targets': [
        {
          'target_name': 'gtk',
          'type': 'none',
          'variables': {
            # gtk requires gmodule, but it does not list it as a dependency
            # in some misconfigured systems.
            'gtk_packages': 'gmodule-2.0 gtk+-2.0 gthread-2.0',
          },
          'direct_dependent_settings': {
            'cflags': [
              '$(shell <(pkg-config) --cflags <(gtk_packages))',
            ],
          },
          'link_settings': {
            'ldflags': [
              '$(shell <(pkg-config) --libs-only-L --libs-only-other <(gtk_packages))',
            ],
            'libraries': [
              '$(shell <(pkg-config) --libs-only-l <(gtk_packages))',
            ],
          },
        },
      ],
    }],  # OS=="linux" or OS=="freebsd" or OS=="openbsd"
  ],
}
