// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/client_app.h"

// static
void ClientApp::CreateBrowserDelegates(BrowserDelegateSet& delegates) {
  // Bring in the Frame tests.
  extern void CreateFrameBrowserTests(BrowserDelegateSet& delegates);
  CreateFrameBrowserTests(delegates);

  // Bring in the Navigation tests.
  extern void CreateNavigationBrowserTests(BrowserDelegateSet& delegates);
  CreateNavigationBrowserTests(delegates);

  // Bring in the RequestHandler tests.
  extern void CreateRequestHandlerBrowserTests(BrowserDelegateSet& delegates);
  CreateRequestHandlerBrowserTests(delegates);

  // Bring in the V8 tests.
  extern void CreateV8BrowserTests(BrowserDelegateSet& delegates);
  CreateV8BrowserTests(delegates);
}

// static
void ClientApp::CreateRenderDelegates(RenderDelegateSet& delegates) {
  // Bring in the Frame tests.
  extern void CreateFrameRendererTests(RenderDelegateSet& delegates);
  CreateFrameRendererTests(delegates);

  // Bring in the DOM tests.
  extern void CreateDOMRendererTests(RenderDelegateSet& delegates);
  CreateDOMRendererTests(delegates);

  // Bring in the message router tests.
  extern void CreateMessageRouterRendererTests(
      ClientApp::RenderDelegateSet& delegates);
  CreateMessageRouterRendererTests(delegates);

  // Bring in the Navigation tests.
  extern void CreateNavigationRendererTests(RenderDelegateSet& delegates);
  CreateNavigationRendererTests(delegates);

  // Bring in the process message tests.
  extern void CreateProcessMessageRendererTests(
      ClientApp::RenderDelegateSet& delegates);
  CreateProcessMessageRendererTests(delegates);

  // Bring in the RequestHandler tests.
  extern void CreateRequestHandlerRendererTests(RenderDelegateSet& delegates);
  CreateRequestHandlerRendererTests(delegates);

  // Bring in the Request tests.
  extern void CreateRequestRendererTests(RenderDelegateSet& delegates);
  CreateRequestRendererTests(delegates);

  // Bring in the routing test handler delegate.
  extern void CreateRoutingTestHandlerDelegate(
      ClientApp::RenderDelegateSet& delegates);
  CreateRoutingTestHandlerDelegate(delegates);

  // Bring in the URLRequest tests.
  extern void CreateURLRequestRendererTests(RenderDelegateSet& delegates);
  CreateURLRequestRendererTests(delegates);

  // Bring in the V8 tests.
  extern void CreateV8RendererTests(RenderDelegateSet& delegates);
  CreateV8RendererTests(delegates);
}

// static
void ClientApp::RegisterCustomSchemes(
    CefRefPtr<CefSchemeRegistrar> registrar,
    std::vector<CefString>& cookiable_schemes) {
  // Bring in the scheme handler tests.
  extern void RegisterSchemeHandlerCustomSchemes(
      CefRefPtr<CefSchemeRegistrar> registrar,
      std::vector<CefString>& cookiable_schemes);
  RegisterSchemeHandlerCustomSchemes(registrar, cookiable_schemes);

  // Bring in the cookie tests.
  extern void RegisterCookieCustomSchemes(
      CefRefPtr<CefSchemeRegistrar> registrar,
      std::vector<CefString>& cookiable_schemes);
  RegisterCookieCustomSchemes(registrar, cookiable_schemes);

  // Bring in the URLRequest tests.
  extern void RegisterURLRequestCustomSchemes(
      CefRefPtr<CefSchemeRegistrar> registrar,
      std::vector<CefString>& cookiable_schemes);
  RegisterURLRequestCustomSchemes(registrar, cookiable_schemes);
}

// static
CefRefPtr<CefPrintHandler> ClientApp::CreatePrintHandler() {
  return NULL;
}

