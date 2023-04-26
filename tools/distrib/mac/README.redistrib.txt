REDISTRIBUTION
--------------

This binary distribution contains the below components. Components listed under
the "required" section must be redistributed with all applications using CEF.
Components listed under the "optional" section may be excluded if the related
features will not be used.

Applications using CEF on OS X must follow a specific app bundle structure.
Replace "cefclient" in the below example with your application name.

cefclient.app/
  Contents/
    Frameworks/
      Chromium Embedded Framework.framework/
        Chromium Embedded Framework <= main application library
        Libraries/
          libEGL.dylib <= ANGLE support libraries
          libGLESv2.dylib <=^
          libvk_swiftshader.dylib <= SwANGLE support libraries
          vk_swiftshader_icd.json <=^
        Resources/
          chrome_100_percent.pak <= non-localized resources and strings
          chrome_200_percent.pak <=^
          resources.pak          <=^
          gpu_shader_cache.bin <= ANGLE-Metal shader cache
          icudtl.dat <= unicode support
          snapshot_blob.bin, v8_context_snapshot.[x86_64|arm64].bin <= V8 initial snapshot
          en.lproj/, ... <= locale-specific resources and strings
          Info.plist
      cefclient Helper.app/
        Contents/
          Info.plist
          MacOS/
            cefclient Helper <= helper executable
          Pkginfo
      Info.plist
    MacOS/
      cefclient <= cefclient application executable
    Pkginfo
    Resources/
      binding.html, ... <= cefclient application resources

The "Chromium Embedded Framework.framework" is an unversioned framework that
contains CEF binaries and resources. Executables (cefclient, cefclient Helper,
etc) must load this framework dynamically at runtime instead of linking it
directly. See the documentation in include/wrapper/cef_library_loader.h for
more information.

The "cefclient Helper" app is used for executing separate processes (renderer,
plugin, etc) with different characteristics. It needs to have a separate app
bundle and Info.plist file so that, among other things, it doesn't show dock
icons.

Required components:

The following components are required. CEF will not function without them.

* CEF core library.
  * Chromium Embedded Framework.framework/Chromium Embedded Framework

* Unicode support data.
  * Chromium Embedded Framework.framework/Resources/icudtl.dat

* V8 snapshot data.
  * Chromium Embedded Framework.framework/Resources/snapshot_blob.bin
  * Chromium Embedded Framework.framework/Resources/v8_context_snapshot.bin

Optional components:

The following components are optional. If they are missing CEF will continue to
run but any related functionality may become broken or disabled.

* Localized resources.
  Locale file loading can be disabled completely using
  CefSettings.pack_loading_disabled.

  * Chromium Embedded Framework.framework/Resources/*.lproj/
    Directory containing localized resources used by CEF, Chromium and Blink. A
    .pak file is loaded from this directory based on the CefSettings.locale
    value. Only configured locales need to be distributed. If no locale is
    configured the default locale of "en" will be used. Without these files
    arbitrary Web components may display incorrectly.

* Other resources.
  Pack file loading can be disabled completely using
  CefSettings.pack_loading_disabled.

  * Chromium Embedded Framework.framework/Resources/chrome_100_percent.pak
  * Chromium Embedded Framework.framework/Resources/chrome_200_percent.pak
  * Chromium Embedded Framework.framework/Resources/resources.pak
    These files contain non-localized resources used by CEF, Chromium and Blink.
    Without these files arbitrary Web components may display incorrectly.

* ANGLE support.
  * Chromium Embedded Framework.framework/Libraries/libEGL.dylib
  * Chromium Embedded Framework.framework/Libraries/libGLESv2.dylib
  * Chromium Embedded Framework.framework/Resources/gpu_shader_cache.bin
  Support for rendering of HTML5 content like 2D canvas, 3D CSS and WebGL.
  Without these files the aforementioned capabilities may fail.

* SwANGLE support.
  * Chromium Embedded Framework.framework/Libraries/libvk_swiftshader.dylib
  * Chromium Embedded Framework.framework/Libraries/vk_swiftshader_icd.json
  Support for software rendering of HTML5 content like 2D canvas, 3D CSS and
  WebGL using SwiftShader's Vulkan library as ANGLE's Vulkan backend. Without
  these files the aforementioned capabilities may fail when GPU acceleration is
  disabled or unavailable.
