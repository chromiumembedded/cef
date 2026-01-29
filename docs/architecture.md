This page provides an overview of the CEF architecture.

**Contents**

- [Overview](#overview)
- [Dependencies](#dependencies)
- [Versions](#versions)
- [History](#history)
- [Modern Architecture](#modern-architecture)
- [Common API Usage](#common-api-usage)
- [C Wrapper API](#c-wrapper-api)
- [CEF1 (Discontinued)](#cef1-discontinued)
  - [API Usage](#api-usage)
  - [Threading Considerations](#threading-considerations)
  - [Implementation Details](#implementation-details)
- [CEF2 (Discontinued)](#cef2-discontinued)
- [CEF3](#cef3)
  - [API Usage](#api-usage)
  - [Process Considerations](#process-considerations)
  - [Threading Considerations](#threading-considerations)
  - [Implementation Details](#implementation-details)
    - [Alloy Runtime Classes](#alloy-runtime-classes)
    - [Chrome Runtime Classes](#chrome-runtime-classes)

---

# Overview

The Chromium Embedded Framework (CEF) is an open source project founded by Marshall Greenblatt in 2008 to develop a Web browser control based on the [Google Chromium](http://www.chromium.org/Home) project. CEF currently supports a range of programming languages and operating systems and can be easily integrated into both new and existing applications. It was designed from the ground up with both performance and ease of use in mind. The base framework includes C and C++ programming interfaces exposed via native libraries that insulate the host application from Chromium and Blink implementation details. It provides close integration between the browser control and the host application, which can optionally control resource loading, navigation, context menus, printing, JavaScript bindings and more, while taking advantage of the same performance and HTML5 technologies available in the Google Chrome Web browser.

See the project [README.md](https://github.com/chromiumembedded/cef/blob/master/README.md) for additional introductory material.

# Dependencies

The CEF project depends on a number of other projects maintained by third parties. The major projects that CEF depends on are:

  * [Chromium](http://www.chromium.org/Home) - Provides general functionality like network stack, threading, message loop, logging and process control needed to create a fully functional Web browser. Implements "platform" code that allows Blink to communicate with V8 and Skia. Many Chromium design documents can be found at http://dev.chromium.org/developers.
  * [Blink](https://www.chromium.org/blink) (formerly WebKit) - The rendering implementation used by Chromium. Provides DOM parsing, layout, event handling, rendering and HTML5 JavaScript APIs. Some HTML5 implementations are split between the Blink and Chromium code bases.
  * [V8](https://developers.google.com/v8/) - JavaScript engine.
  * [Skia](https://skia.org/) - 2D graphics library used for rendering non-accelerated content. More information on how Chromium integrates Skia is available [here](http://www.chromium.org/developers/design-documents/graphics-and-skia).
  * [Angle](https://chromium.googlesource.com/angle/angle/+/master/README.md) - 3D graphics translation layer for Windows that converts GLES calls to DirectX. More information about accelerated compositing is available [here](http://dev.chromium.org/developers/design-documents/gpu-accelerated-compositing-in-chrome).

# Versions

There have been three versions of CEF to date.

  * CEF1 - Single process implementation using the Chromium WebKit API (now discontinued).
  * CEF2 - Multi process implementation built on the Chromium browser (now discontinued).
  * CEF3 - Multi process implementation using either the [Chromium Content API](http://www.chromium.org/developers/content-module/content-api) (called the "Alloy runtime") or the complete Chrome UI (called the "Chrome runtime").

Only the CEF3 version is currently developed and supported. See the "History" and "Modern Architecture" sections below for background and the [Branches And Building](branches_and_building.md) page for current development information.

# History

In 2008, when the CEF project started, Chromium was a monolithic application built on top of Apple's [WebKit](https://webkit.org/) rendering engine. Google maintained an in-tree variant of WebKit (called the [Chromium WebKit API](https://www.youtube.com/watch?v=JFzC_Gx76E8)) that added additional functionality specific to Chromium. The first version of CEF (called CEF1) was implemented on top of this Chromium WebKit API and used the default single-process application model supported by WebKit1 at that time (for more details on CEF1 architecture see the "CEF1" section below).

Use of the Chromium WebKit API came with some known limitations. For example, that API layer did not support the multi-process model that was the basis for most of Google Chrome's [stability and security advantages](https://chromium.googlesource.com/playground/chromium-org-site/+/refs/heads/main/developers/design-documents/multi-process-architecture/index.md). It also lacked many of the UI features that were becoming necessary to support new web platform features (like device selection dialogs, permissions management, etc). We therefore began a new effort, in 2010, to reimplement CEF (called CEF2) on top of the fully-featured but monolithic Chromium browser UI. The lack of internal API boundaries in the Chromium code at that time made it necessary for us to take an "all or nothing" approach. If we could create a CEF browser window based on the complete Chrome UI we would effectively gain access to all of Chrome's capabilities "for free" (for more details on CEF2 architecture see the "CEF2" section below).

Google began acknowledging the validity of the embedded use case for Chromium in 2010 and started work on new internal API boundaries to make embedding easier. The [Chromium Content API](http://www.chromium.org/developers/content-module/content-api), which encapsulated the multi-process application model and provided stub implementations for most web platform features, reached a development stage in late 2011 where it was a viable option for Chromium embedders like CEF. It still didn't provide all of the features that we might like, but it was a "supported" API layer. We therefore abandoned the CEF2 effort in early 2012 and focused our efforts instead on the Content API as the more viable embedding option. The existing CEF1 API, which was strongly dependent on WebKit1 and the single-process model, could not be directly ported to the Content API. A new CEF API (called CEF3), with substantial breaking API changes and multi-process support, was released in early 2013 (for more details on CEF3 architecture see the "Modern Architecture" and "CEF3" sections below).

Also in 2013, Google decided to [fork the WebKit project](https://blog.chromium.org/2013/04/blink-rendering-engine-for-chromium.html). After renaming it to Blink they removed many of the API boundaries that had previously existed between Chromium and WebKit and effectively deleted the Chromium WebKit API. The multi-process architecture, which was optional in WebKit1, was now a requirement with Chromium/Blink. CEF1, which depended on the Chromium WebKit API, no longer had a viable future and CEF3 became the only path forward.

Fast forward another 7 years, to 2020, and the Chromium architecture has again changed substantially. A lot of effort has been spent to define clear architectural layers (sometimes called [components](https://www.chromium.org/developers/design-documents/cookbook/)) in the Chromium/Blink source code. New platforms and new embedders have entered the Chromium ecosystem. And yet, many features desired by CEF users remain stubbornly out of reach in the Chrome UI layer. With this new, more modern, version of Chromium, we decided it was time to revisit our goals from CEF2 and create a new CEF version based on the Chrome UI layer (now called the "Chrome runtime" in CEF3). Continue to the "Modern Architecture" section for details.

# Modern Architecture

The modern Chromium code base is composed of architectural layers, with the primary application layers being "content" (for the [Content API](http://www.chromium.org/developers/content-module/content-api)) and "chrome" (for the Chrome UI layer). The "content" layer includes [WebContents](https://source.chromium.org/chromium/chromium/src/+/master:content/public/browser/web_contents.h) as the primary browser object and provides implementations for most of the generic web platform and application/process management functionality. The "chrome" layer includes [Browser](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/ui/browser.h) as the primary browser object and provides implementations for many advanced features (including datalist/autofill, extensions, gamepad, webhid, webmidi, etc.), along with standard Chrome UI toolbars and dialogs for device selection, user permissions, settings, etc.

For historical reasons (outlined above), CEF browser windows have been based on the "content" layer WebContents object since the introduction of CEF3 in 2013. Adding support for additional "chrome" layer features in that architecture was a serious effort because it involved careful understanding and cherry-picking of the related functionality from "chrome" code into the CEF code base. Over time we have added dependencies on the "chrome" layer targets to support some features in CEF (such and extensions, PDF viewer and print preview) and to avoid code duplication. As a result, CEF3 binaries in modern times already contained most/all of the code that would be required to launch a stand-alone "chrome" Browser window, but simply lacked the mechanism to do so.

In 2020 we launched a [new effort](https://github.com/chromiumembedded/cef/issues/2969) to implement the CEF3 API on top of a fully-featured "chrome" Browser window. To be usable in existing CEF3 client applications the "chrome" window needed to support a large subset of CEF3's existing callbacks/intercepts (e.g. loading progress, network requests, etc.), and be inserted into the existing [Views framework](https://www.chromium.org/developers/design-documents/chromeviews/) structure. At the same time, it was necessary to substantially refactor the internal CEF (libcef) code to extract functionality that was common to both the old WebContents-based approach and the new Browser-based approach. As part of this refactoring we renamed the existing "content" layer implementation to the "Alloy runtime" (where Alloy = Chrome + other stuff) and named the new "chrome" layer implementation as the "Chrome runtime". Proceed to the "CEF3" section below for additional feature and implementation details.

# Common API Usage

All versions of CEF expose a simple, easy-to-use API designed to insulate users from the underlying Chromium and Blink code complexity. Common usage is as follows:

1. Initialize CEF by calling CefInitialize().

2. Perform work on the UI message loop by calling CefRunMessageLoop() or CefDoMessageLoopWork().

3. Create a browser window by calling CreateBrowser() or CreateBrowserSync() and passing in a CefClient instance.

4. Shut down CEF by calling CefShutdown() before the application exits.

# C Wrapper API

The libcef shared library exports a C API that isolates the user from the CEF runtime and code base. The libcef\_dll\_wrapper project, which is distributed in source code form as part of the binary release, wraps this exported C API in a C++ API that is then linked into the client application. The code for this C/C++ API translation layer is automatically generated by the [translator](https://github.com/chromiumembedded/cef/blob/master/tools/translator.README.txt) tool. Direct usage of the C API is described on the [Using The CAPI](using_the_capi.md) page.

# CEF1 (Discontinued)

CEF1 was the initial version of CEF first released in 2008. It was discontinued in 2013 when Google forked the WebKit project and subsequently deleted the Chromium WebKit API. See the "History" section above for additional details.

The single process architecture used by CEF1 integrates Chromium and WebKit directly into the client application. Advantages to the single process architecture included reduced memory usage and closer integration with the client application. For example, having the renderer running in the same process allowed JavaScript bindings to synchronously communicate between client code and renderer code. Disadvantages included [reduced performance](https://github.com/chromiumembedded/cef/issues/304) with certain types of accelerated content, [crashes](https://github.com/chromiumembedded/cef/issues/242) due to plugins like Flash running in the same process, and lack of support for application [security and stability features](https://chromium.googlesource.com/playground/chromium-org-site/+/refs/heads/main/developers/design-documents/multi-process-architecture/index.md) that required a multi-process architecture.

## API Usage

Below is an overview of the main CEF1 interfaces and their uses. Since CEF1 is discontinued we no longer provide detailed usage information.

  * [CefApp](http://magpcss.org/ceforum/apidocs/projects/(default)/CefApp.html) - This interface is passed to CefInitialize() and allows the application to customize global functionality like resource loading and proxy handling.
  * [CefClient](http://magpcss.org/ceforum/apidocs/projects/(default)/CefClient.html) - This interface is passed to CefCreateBrowser() or CefCreateBrowserSync() and acts as the connection between an individual CEF browser instance and the client application. Additional interfaces for request handling, display handling, etc. are exposed via this interface.
  * [CefBrowser](http://magpcss.org/ceforum/apidocs/projects/(default)/CefBrowser.html) - Exposes capabilities provided by the browser. This includes back/forward navigation, source retrieval, request loading, etc. A single CefBrowser may have one or more child [CefFrame](http://magpcss.org/ceforum/apidocs/projects/(default)/CefFrame.html) objects.

## Threading Considerations

CEF1 includes UI, IO and FILE threads. The UI thread creates browser windows and is used for all interaction with WebKit and V8. The IO thread is used for handling schema and network requests. The FILE thread is used for the application cache and other miscellaneous activities.

When using CEF1 you should keep the following threading considerations in mind:

  * Do not perform blocking operations on the UI thread. This can lead to serious performance issues.
  * The UI thread will be the same as the main application thread if CefInitialize() is called with a CefSettings.multi\_threaded\_message\_loop value of false.
  * All WebKit and V8 interation must take place on the UI thread. Consequently, some CEF API functions can only be called on the UI thread. Functions that have this limitation will be documented accordingly in the associated CEF header file.
  * The [CefPostTask](http://magpcss.org/ceforum/apidocs/projects/(default)/(_globals).html#CefPostTask(CefThreadId,CefRefPtr<CefTask>)) method can be used to post tasks asnychronously between the various threads.

## Implementation Details

CEF1 has the following major implementation classes:

  * CefContext - Represents the global CEF context. A single CefContext object is created by CefInitialize() and destroyed by CefShutdown().
  * CefProcess - Used by CefContext to create and manage the CEF threads.
  * BrowserWebKitInit - Manages the global WebKit environment as exposed by the Chromium WebKit API.
  * WebViewHost - Provides the native window "wrapper" implementation for a WebView. This class extends WebWidgetHost which provides functionality shared in common with popup widgets (like select menus) on some platforms.
  * CefBrowserImpl - Implements the CefBrowser interface, creates the WebViewHost and provides the glue code for a single browser instance.
  * BrowserWebViewDelegate - Implements the WebKit interfaces that provide communication between CefBrowserImpl and the underlying WebView.

# CEF2 (Discontinued)

CEF2 was an effort to reimplement the CEF1 API on top of the Chrome browser UI as it existed in 2010. CEF2 was discontinued when Chromium announced support for the Content API. Information about CEF2 can be found in the "History" section above and [here](http://magpcss.org/ceforum/viewtopic.php?f=10&t=122) on the CEF Forum.

# CEF3

CEF3 has been the recommended and supported version of CEF since January 2013. It uses the same multi process architecture as the Chromium Web browser via either the [Chromium Content API](http://www.chromium.org/developers/content-module/content-api) (called the "Alloy runtime") or the complete Chrome UI (called the "Chrome runtime"). This architecture provides a number of advantages over the single process architecture used by CEF1:

  * Support for multi-process run mode (with related [stability and security advantages](https://chromium.googlesource.com/playground/chromium-org-site/+/refs/heads/main/developers/design-documents/multi-process-architecture/index.md)).
  * More code sharing with the Chromium browser.
  * Improved performance and less breakage due to use of the "supported" code path.
  * Faster access to new features.

In most cases CEF3 will have the same performance and stability characteristics as the Chromium Web browser. Both the Alloy runtime and the Chrome runtime support modern web platform features and provide the safety/security expected from an up-to-date Chromium-based browser. The Chrome runtime, which was introduced in 2020, adds implementations for many advanced features (including datalist/autofill, extensions, gamepad, webhid, webmidi, etc.), along with standard Chrome UI toolbars and dialogs for device selection, user permissions, settings, etc. At the time of writing, the Chrome runtime requires use of the [Views framework](https://www.chromium.org/developers/design-documents/chromeviews/) in CEF client applications and does not support off-screen rendering. A current list of open issues specific to the Chrome runtime can be found [here](https://github.com/chromiumembedded/cef/issues?q=is%3Aissue+is%3Aopen+label%3Achrome).

Starting with the M125 release (mid-2024) the Alloy runtime has been [split](https://github.com/chromiumembedded/cef/issues/3681) into separate style and bootstrap components. Both Chrome style and Alloy style browsers/windows can now be created while using the Chrome bootstrap. The Alloy bootstrap has been [removed](https://github.com/chromiumembedded/cef/issues/3685) starting with the M128 release.

## API Usage

See the [General Usage](general_usage.md) page for detailed API usage instructions.

## Process Considerations

See the [Processes](general_usage.md#processes) section of the [General Usage](general_usage.md) page for details.

## Threading Considerations

See the [Threads](general_usage.md#threads) section of the [General Usage](general_usage.md) page for details.

## Implementation Details

For an implementation overview see the "Modern Architecture" section above. 

Shared CEF library/runtime classes use the Cef prefix. Classes that implement the Alloy or Chrome runtime use the Alloy or Chrome prefixes respectively. Classes that extend an existing Chrome-prefixed class add the Cef or Alloy suffix, thereby following the existing naming pattern of Chrome-derived classes.

### Alloy Runtime Classes

Alloy runtime bootstrap was [removed](https://github.com/chromiumembedded/cef/issues/3685) in M128 (mid-2024) and Alloy runtime style is now supported using the Chrome runtime bootstrap. Alloy runtime style is based on the "content" layer and has the following major implementation classes:

  * [AlloyBrowserHostImpl](https://github.com/chromiumembedded/cef/blob/master/libcef/browser/alloy/alloy_browser_host_impl.h) - Implements the CefBrowser and CefBrowserHost interfaces in the browser process. Provides the glue code and implements interfaces for communicating with the RenderViewHost.

### Chrome Runtime Classes

Chrome runtime bootstrap and style is based on the "chrome" layer and has the following major implementation classes:

  * [ChromeMainDelegateCef](https://github.com/chromiumembedded/cef/blob/master/libcef/common/chrome/chrome_main_delegate_cef.h) - Implements the common process bootstrap logic.
  * [ChromeContentClientCef](https://github.com/chromiumembedded/cef/blob/master/libcef/common/chrome/chrome_content_client_cef.h) - Implements the Content API callbacks that are common to all processes.
  * [CefContext](https://github.com/chromiumembedded/cef/blob/master/libcef/browser/context.h) - Represents the global CEF context in the browser process. A single CefContext object is created by CefInitialize() and destroyed by CefShutdown().
  * [ChromeBrowserMainExtraPartsCef](https://github.com/chromiumembedded/cef/blob/master/libcef/browser/chrome/chrome_browser_main_extra_parts_cef.h) - Implements the browser process bootstrap logic.
  * [ChromeContentBrowserClientCef](https://github.com/chromiumembedded/cef/blob/master/libcef/browser/chrome/chrome_content_browser_client_cef.h) - Implements the Content API callbacks for the browser process.
  * [ChromeBrowserHostImpl](https://github.com/chromiumembedded/cef/blob/master/libcef/browser/chrome/chrome_browser_host_impl.h) - Implements the CefBrowser and CefBrowserHost interfaces in the browser process. Provides the glue code and implements interfaces for communicating with the RenderViewHost.
  * [ChromeContentRendererClientCef](https://github.com/chromiumembedded/cef/blob/master/libcef/renderer/chrome/chrome_content_renderer_client_cef.h) - Implements the Content API callbacks for the render process.
  * [CefBrowserImpl](https://github.com/chromiumembedded/cef/blob/master/libcef/renderer/browser_impl.h) - Implements the CefBrowser interface in the render process. Provides the glue code and implements interfaces for communicating with the RenderView.
  * [ChromeBrowserDelegate](https://github.com/chromiumembedded/cef/blob/master/libcef/browser/chrome/chrome_browser_delegate.h) - Interface injected into Chrome's [Browser](https://source.chromium.org/chromium/chromium/src/+/master:chrome/browser/ui/browser.h) object (via [patch files](https://github.com/chromiumembedded/cef/blob/master/patch/README.txt)) to support callbacks into CEF code.
  * [ChromeBrowserFrame](https://github.com/chromiumembedded/cef/blob/ecdfd467f84da9bb6ef57eb4fcb2778fdef5e07a/libcef/browser/chrome/views/chrome_browser_frame.h) - Widget for a Views-hosted Chrome browser.

An overview of the Chrome Browser object model is also available [here](https://github.com/chromiumembedded/cef/blob/ecdfd467f84da9bb6ef57eb4fcb2778fdef5e07a/libcef/browser/chrome/views/chrome_browser_frame.h).