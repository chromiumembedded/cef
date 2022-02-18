// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppBrowser;
using client::ClientAppRenderer;

void CreateBrowserDelegates(ClientAppBrowser::DelegateSet& delegates) {
  // Bring in audio output tests.
  extern void CreateAudioOutputTests(ClientAppBrowser::DelegateSet & delegates);
  CreateAudioOutputTests(delegates);

  // Bring in the CORS tests.
  extern void CreateCorsBrowserTests(ClientAppBrowser::DelegateSet & delegates);
  CreateCorsBrowserTests(delegates);

  // Bring in the preference tests.
  extern void CreatePreferenceBrowserTests(ClientAppBrowser::DelegateSet &
                                           delegates);
  CreatePreferenceBrowserTests(delegates);

  // Bring in URLRequest tests.
  extern void CreateURLRequestBrowserTests(ClientAppBrowser::DelegateSet &
                                           delegates);
  CreateURLRequestBrowserTests(delegates);
}

void CreateRenderDelegates(ClientAppRenderer::DelegateSet& delegates) {
  // Bring in the Frame tests.
  extern void CreateFrameRendererTests(ClientAppRenderer::DelegateSet &
                                       delegates);
  CreateFrameRendererTests(delegates);

  // Bring in the DOM tests.
  extern void CreateDOMRendererTests(ClientAppRenderer::DelegateSet &
                                     delegates);
  CreateDOMRendererTests(delegates);

  // Bring in the message router tests.
  extern void CreateMessageRouterRendererTests(ClientAppRenderer::DelegateSet &
                                               delegates);
  CreateMessageRouterRendererTests(delegates);

  // Bring in the Navigation tests.
  extern void CreateNavigationRendererTests(ClientAppRenderer::DelegateSet &
                                            delegates);
  CreateNavigationRendererTests(delegates);

  // Bring in the process message tests.
  extern void CreateProcessMessageRendererTests(ClientAppRenderer::DelegateSet &
                                                delegates);
  CreateProcessMessageRendererTests(delegates);

  // Bring in the RequestHandler tests.
  extern void CreateRequestHandlerRendererTests(ClientAppRenderer::DelegateSet &
                                                delegates);
  CreateRequestHandlerRendererTests(delegates);

  // Bring in the routing test handler delegate.
  extern void CreateRoutingTestHandlerDelegate(ClientAppRenderer::DelegateSet &
                                               delegates);
  CreateRoutingTestHandlerDelegate(delegates);

  // Bring in the thread tests.
  extern void CreateThreadRendererTests(ClientAppRenderer::DelegateSet &
                                        delegates);
  CreateThreadRendererTests(delegates);

  // Bring in the URLRequest tests.
  extern void CreateURLRequestRendererTests(ClientAppRenderer::DelegateSet &
                                            delegates);
  CreateURLRequestRendererTests(delegates);

  // Bring in the V8 tests.
  extern void CreateV8RendererTests(ClientAppRenderer::DelegateSet & delegates);
  CreateV8RendererTests(delegates);
}

void RegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
  // Bring in the scheme handler tests.
  extern void RegisterSchemeHandlerCustomSchemes(
      CefRawPtr<CefSchemeRegistrar> registrar);
  RegisterSchemeHandlerCustomSchemes(registrar);

  // Bring in the cookie tests.
  extern void RegisterCookieCustomSchemes(
      CefRawPtr<CefSchemeRegistrar> registrar);
  RegisterCookieCustomSchemes(registrar);

  // Bring in the URLRequest tests.
  extern void RegisterURLRequestCustomSchemes(
      CefRawPtr<CefSchemeRegistrar> registrar);
  RegisterURLRequestCustomSchemes(registrar);

  // Bring in the resource request handler tests.
  extern void RegisterResourceRequestHandlerCustomSchemes(
      CefRawPtr<CefSchemeRegistrar> registrar);
  RegisterResourceRequestHandlerCustomSchemes(registrar);
}

void RegisterCookieableSchemes(std::vector<std::string>& cookieable_schemes) {
  // Bring in the scheme handler tests.
  extern void RegisterSchemeHandlerCookieableSchemes(std::vector<std::string> &
                                                     cookieable_schemes);
  RegisterSchemeHandlerCookieableSchemes(cookieable_schemes);

  // Bring in the cookie tests.
  extern void RegisterCookieCookieableSchemes(std::vector<std::string> &
                                              cookieable_schemes);
  RegisterCookieCookieableSchemes(cookieable_schemes);

  // Bring in the URLRequest tests.
  extern void RegisterURLRequestCookieableSchemes(std::vector<std::string> &
                                                  cookieable_schemes);
  RegisterURLRequestCookieableSchemes(cookieable_schemes);
}

namespace client {

// static
void ClientAppBrowser::CreateDelegates(DelegateSet& delegates) {
  ::CreateBrowserDelegates(delegates);
}

// static
void ClientAppRenderer::CreateDelegates(DelegateSet& delegates) {
  ::CreateRenderDelegates(delegates);
}

// static
void ClientApp::RegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
  ::RegisterCustomSchemes(registrar);
}

// static
void ClientAppBrowser::RegisterCookieableSchemes(
    std::vector<std::string>& cookieable_schemes) {
  ::RegisterCookieableSchemes(cookieable_schemes);
}

}  // namespace client
