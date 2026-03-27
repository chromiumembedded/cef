// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/stealth_config.h"

std::string CefStealthConfig::BuildStealthScript() const {
  std::string script;

  if (hide_webdriver) {
    script += R"JS(
// Hide navigator.webdriver
Object.defineProperty(navigator, 'webdriver', {
  get: () => undefined,
  configurable: true,
});
)JS";
  }

  if (hide_cdp_artifacts) {
    script += R"JS(
// Remove CDP artifacts from window
(function() {
  const props = Object.getOwnPropertyNames(window);
  for (const prop of props) {
    if (prop.match(/^(cdc_|__webdriver_|\$cdc_)/)) {
      try { delete window[prop]; } catch(e) {}
    }
  }
})();
)JS";
  }

  if (fake_plugins) {
    script += R"JS(
// Fake navigator.plugins
Object.defineProperty(navigator, 'plugins', {
  get: () => {
    const plugins = [
      { name: 'Chrome PDF Plugin', filename: 'internal-pdf-viewer',
        description: 'Portable Document Format' },
      { name: 'Chrome PDF Viewer', filename: 'mhjfbmdgcfjbbpaeojofohoefgiehjai',
        description: '' },
      { name: 'Native Client', filename: 'internal-nacl-plugin',
        description: '' },
    ];
    plugins.length = 3;
    plugins.item = idx => plugins[idx] || null;
    plugins.namedItem = name => plugins.find(p => p.name === name) || null;
    plugins.refresh = () => {};
    return plugins;
  },
  configurable: true,
});
)JS";
  }

  if (fake_languages) {
    script += R"JS(
// Ensure navigator.languages is populated
if (!navigator.languages || navigator.languages.length === 0) {
  Object.defineProperty(navigator, 'languages', {
    get: () => ['en-US', 'en'],
    configurable: true,
  });
}
)JS";
  }

  if (fake_screen) {
    script += R"JS(
// Ensure screen has standard values
if (screen.width === 0 || screen.height === 0) {
  Object.defineProperty(screen, 'width', { get: () => 1920, configurable: true });
  Object.defineProperty(screen, 'height', { get: () => 1080, configurable: true });
  Object.defineProperty(screen, 'availWidth', { get: () => 1920, configurable: true });
  Object.defineProperty(screen, 'availHeight', { get: () => 1040, configurable: true });
  Object.defineProperty(screen, 'colorDepth', { get: () => 24, configurable: true });
  Object.defineProperty(screen, 'pixelDepth', { get: () => 24, configurable: true });
}
)JS";
  }

  if (fake_webgl) {
    script += R"JS(
// Override WebGL debug info
(function() {
  const getParam = WebGLRenderingContext.prototype.getParameter;
  WebGLRenderingContext.prototype.getParameter = function(param) {
    if (param === 0x9245) return 'Google Inc. (NVIDIA)';  // UNMASKED_VENDOR
    if (param === 0x9246) return 'ANGLE (NVIDIA, NVIDIA GeForce GTX 1080 Direct3D11 vs_5_0 ps_5_0, D3D11)';  // UNMASKED_RENDERER
    return getParam.call(this, param);
  };
  if (typeof WebGL2RenderingContext !== 'undefined') {
    const getParam2 = WebGL2RenderingContext.prototype.getParameter;
    WebGL2RenderingContext.prototype.getParameter = function(param) {
      if (param === 0x9245) return 'Google Inc. (NVIDIA)';
      if (param === 0x9246) return 'ANGLE (NVIDIA, NVIDIA GeForce GTX 1080 Direct3D11 vs_5_0 ps_5_0, D3D11)';
      return getParam2.call(this, param);
    };
  }
})();
)JS";
  }

  if (patch_permissions_query) {
    script += R"JS(
// Patch Permissions API
(function() {
  const origQuery = Permissions.prototype.query;
  Permissions.prototype.query = function(desc) {
    if (desc.name === 'notifications') {
      return Promise.resolve({ state: 'prompt', onchange: null });
    }
    return origQuery.call(this, desc);
  };
})();
)JS";
  }

  if (mask_headless_ua) {
    script += R"JS(
// Mask headless indicators in user-agent data
if (navigator.userAgentData) {
  const origBrands = navigator.userAgentData.brands;
  if (origBrands) {
    Object.defineProperty(navigator.userAgentData, 'brands', {
      get: () => origBrands.filter(b => !b.brand.includes('Headless')),
      configurable: true,
    });
  }
}
)JS";
  }

  if (!custom_user_agent.empty()) {
    script += "\n// Override navigator.userAgent\n";
    script += "Object.defineProperty(navigator, 'userAgent', {\n";
    script += "  get: () => '" + custom_user_agent + "',\n";
    script += "  configurable: true,\n";
    script += "});\n";
  }

  if (!custom_platform.empty()) {
    script += "\n// Override navigator.platform\n";
    script += "Object.defineProperty(navigator, 'platform', {\n";
    script += "  get: () => '" + custom_platform + "',\n";
    script += "  configurable: true,\n";
    script += "});\n";
  }

  return script;
}

CefStealthConfig CefStealthConfig::Default() {
  return CefStealthConfig();  // All defaults are true
}

CefStealthConfig CefStealthConfig::None() {
  CefStealthConfig config;
  config.hide_webdriver = false;
  config.hide_cdp_artifacts = false;
  config.mask_headless_ua = false;
  config.fake_plugins = false;
  config.fake_screen = false;
  config.fake_webgl = false;
  config.fake_permissions = false;
  config.fake_languages = false;
  config.patch_permissions_query = false;
  return config;
}
