// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_STEALTH_CONFIG_H_
#define CEF_LIBCEF_BROWSER_STEALTH_CONFIG_H_
#pragma once

#include <string>
#include <vector>

// Stealth configuration for removing browser automation fingerprints.
// Applied during browser/context creation to make the automated browser
// indistinguishable from a regular user browser.
//
// These modifications only remove STRUCTURAL fingerprints (API artifacts,
// property differences). Timing-based detection is not addressed here
// as it's explicitly acceptable per project policy.
struct CefStealthConfig {
  // Remove navigator.webdriver property (set to false/undefined).
  // Default: true. This is the most commonly checked automation signal.
  bool hide_webdriver = true;

  // Remove Chrome DevTools Protocol artifacts from window object.
  // Removes: cdc_*, __webdriver_*, $cdc_* properties.
  // Default: true.
  bool hide_cdp_artifacts = true;

  // Mask the user-agent to remove headless/automation indicators.
  // Removes: "HeadlessChrome", "Headless" from UA string.
  // Default: true.
  bool mask_headless_ua = true;

  // Report standard Chrome plugin/mime types.
  // Default: true. Headless Chrome reports no plugins.
  bool fake_plugins = true;

  // Report standard screen dimensions and color depth.
  // Default: true. Headless often has unusual screen values.
  bool fake_screen = true;

  // Set standard WebGL vendor/renderer strings.
  // Default: true. Headless can expose different GL strings.
  bool fake_webgl = true;

  // Spoof the permissions API to report "prompt" instead of "denied".
  // Default: true. Headless denies all permissions by default.
  bool fake_permissions = true;

  // Override navigator.languages to include standard values.
  // Default: true.
  bool fake_languages = true;

  // Run the navigator.permissions.query patch.
  // Default: true.
  bool patch_permissions_query = true;

  // Custom user agent string. If empty, the default is used.
  std::string custom_user_agent;

  // Custom platform string. If empty, navigator.platform is unchanged.
  std::string custom_platform;

  // Build the JavaScript injection code that applies all stealth patches.
  // Returns JS code to be executed in every new document (via
  // Page.addScriptToEvaluateOnNewDocument).
  std::string BuildStealthScript() const;

  // Static factory for a default stealth config (everything enabled).
  static CefStealthConfig Default();

  // Static factory for no stealth (everything disabled).
  static CefStealthConfig None();
};

#endif  // CEF_LIBCEF_BROWSER_STEALTH_CONFIG_H_
