REDISTRIBUTION
--------------

This binary distribution contains the below components.

Required components:

The following components are required. CEF will not function without them.

* CEF core library.
  * libcef.dll

* Crash reporting library.
  * chrome_elf.dll

* Unicode support data.
  * icudtl.dat

* V8 snapshot data.
  * snapshot_blob.bin
  * v8_context_snapshot.bin

Optional components:

The following components are optional. If they are missing CEF will continue to
run but any related functionality may become broken or disabled.

* Localized resources.
  Locale file loading can be disabled completely using
  CefSettings.pack_loading_disabled. The locales directory path can be
  customized using CefSettings.locales_dir_path. 
 
  * locales/
    Directory containing localized resources used by CEF, Chromium and Blink. A
    .pak file is loaded from this directory based on the CefSettings.locale
    value. Only configured locales need to be distributed. If no locale is
    configured the default locale of "en-US" will be used. Without these files
    arbitrary Web components may display incorrectly.

* Other resources.
  Pack file loading can be disabled completely using
  CefSettings.pack_loading_disabled. The resources directory path can be
  customized using CefSettings.resources_dir_path.

  * chrome_100_percent.pak
  * chrome_200_percent.pak
  * resources.pak
    These files contain non-localized resources used by CEF, Chromium and Blink.
    Without these files arbitrary Web components may display incorrectly.

* Direct3D support.
  * d3dcompiler_47.dll
  Support for GPU accelerated rendering of HTML5 content like 2D canvas, 3D CSS
  and WebGL. Without this file the aforementioned capabilities may fail when GPU
  acceleration is enabled (default in most cases). Use of this bundled version
  is recommended instead of relying on the possibly old and untested system
  installed version.

* DirectX compiler support (x64 only).
  * dxil.dll
  * dxcompiler.dll
  Support for DirectX rendering of WebGPU. Without these files the
  aforementioned capabilities may fail.

* ANGLE support.
  * libEGL.dll
  * libGLESv2.dll
  Support for rendering of HTML5 content like 2D canvas, 3D CSS and WebGL.
  Without these files the aforementioned capabilities may fail.

* SwANGLE support.
  * vk_swiftshader.dll
  * vk_swiftshader_icd.json
  * vulkan-1.dll
  Support for software rendering of HTML5 content like 2D canvas, 3D CSS and
  WebGL using SwiftShader's Vulkan library as ANGLE's Vulkan backend. Without
  these files the aforementioned capabilities may fail when GPU acceleration is
  disabled or unavailable.
