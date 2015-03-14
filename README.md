**Note: This project is currently in the process of moving from Google Code to Bitbucket. The original project page is [here](https://code.google.com/p/chromiumembedded/). Additional details about the move are available [here](http://magpcss.org/ceforum/viewtopic.php?f=10&t=12759).**

# Introduction #

The Chromium Embedded Framework (CEF) is a simple framework for embedding Chromium-based browsers in other applications. It is a BSD-licensed open source project founded by Marshall Greenblatt in 2008 and based on the [Google Chromium](http://www.chromium.org/Home) project. Unlike the Chromium project itself, which focuses mainly on Google Chrome application development, CEF focuses on facilitating embedded browser use cases in third-party applications. CEF insulates the user from the underlying Chromium and Blink code complexity by offering production-quality stable APIs, release branches tracking specific Chromium releases, and binary distributions. Most features in CEF have default implementations that provide rich functionality while requiring little or no integration work from the user. There are currently over 100 million installed instances of CEF around the world embedded in products from a wide range of companies and industries. A partial list of companies and products using CEF is available on the [CEF Wikipedia page](http://en.wikipedia.org/wiki/Chromium_Embedded_Framework#Applications_using_CEF). Some use cases for CEF include:

* Embedding an HTML5-compliant Web browser control in an existing native application.
* Creating a light-weight native “shell” application that hosts a user interface developed primarily using Web technologies.
* Rendering Web content “off-screen” in applications that have their own custom drawing frameworks.
* Acting as a host for automated testing of existing Web properties and applications.

CEF supports a wide range of programming languages and operating systems and can be easily integrated into both new and existing applications. It was designed from the ground up with both performance and ease of use in mind. The base framework includes C and C++ programming interfaces exposed via native libraries that insulate the host application from Chromium and Blink implementation details. It provides close integration between the browser and the host application including support for custom plugins, protocols, JavaScript objects and JavaScript extensions. The host application can optionally control resource loading, navigation, context menus, printing and more, while taking advantage of the same performance and HTML5 technologies available in the Google Chrome Web browser.

Users new to CEF development should start by reading the [Tutorial](http://code.google.com/p/chromiumembedded/wiki/Tutorial) Wiki page for an overview of CEF usage and then proceed to the [GeneralUsage](http://code.google.com/p/chromiumembedded/wiki/GeneralUsage) Wiki page for a more in-depth discussion or architectural and usage issues. Complete API documentation is available [here](http://magpcss.org/ceforum/apidocs3/). CEF support and related discussion is available on the [CEF Forum](http://www.magpcss.org/ceforum/).

Numerous individuals and organizations contribute time and resources to support CEF development, but more involvement from the community is always welcome. This includes support for both the core CEF project and external projects that integrate CEF with additional programming languages and frameworks (see the "External Projects" section below). If you are interested in donating time to help with CEF development please see the "Helping Out" section below. If you are interested in donating money to support general CEF development and infrastructure efforts please visit the [CEF Donations](http://www.magpcss.org/ceforum/donate.php) page.

# Binary Distributions #

Binary distributions, which include all files necessary to build a CEF-based application, are available on the [Downloads](http://www.magpcss.net/cef_downloads/) page. Automated nightly builds, available from http://cefbuilds.com, include the newest changes but may not be fully tested. Binary distributions are stand-alone and do not require the download of CEF or Chromium source code. Symbol files for debugging binary distributions of libcef can also be downloaded from the above links.

Release notes for past and current CEF binary distributions are available [here](http://code.google.com/p/chromiumembedded/wiki/ReleaseNotes).

# Source Distributions #

The CEF project is an extension of the Chromium project. CEF maintains development and release branches that track Chromium branches. CEF source code can be downloaded, built and packaged manually or with automated tools. Visit the [BranchesAndBuilding](http://code.google.com/p/chromiumembedded/wiki/BranchesAndBuilding) Wiki page for more information.

# External Projects #

The base CEF framework includes support for the C and C++ programming languages. Thanks to the hard work of external maintainers CEF can integrate with a number of other programming languages and frameworks. These external projects are not maintained by CEF so please contact the respective project maintainer if you have any questions or issues.

* .Net - https://github.com/chillitom/CefSharp
* .Net (CEF1) - https://bitbucket.org/fddima/cefglue
* .Net/Mono (CEF3) - https://bitbucket.org/xilium/xilium.cefglue
* .Net (CEF3) - https://bitbucket.org/wborgsm/chromiumfx
* Delphi (CEF1) - http://code.google.com/p/delphichromiumembedded/
* Delphi (CEF3) - http://code.google.com/p/dcef3/
* Go - https://github.com/CzarekTomczak/cef2go
* Java - http://code.google.com/p/javachromiumembedded/
* Java - http://code.google.com/p/javacef/
* Python - http://code.google.com/p/cefpython/

If you're the maintainer of a project not listed above and would like your project listed here please either post to the [CEF Forum](http://www.magpcss.org/ceforum/) or contact Marshall directly.