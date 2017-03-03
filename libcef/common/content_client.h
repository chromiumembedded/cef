// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CONTENT_CLIENT_H_
#define CEF_LIBCEF_COMMON_CONTENT_CLIENT_H_
#pragma once

#include <list>
#include <string>
#include <vector>

#include "include/cef_app.h"

#include "base/compiler_specific.h"
#include "content/public/common/content_client.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/url_util.h"

class CefContentClient : public content::ContentClient,
                         public ui::ResourceBundle::Delegate {
 public:
  static const char kPDFPluginPath[];

  explicit CefContentClient(CefRefPtr<CefApp> application);
  ~CefContentClient() override;

  // Returns the singleton CefContentClient instance.
  static CefContentClient* Get();

  // content::ContentClient methods.
  void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) override;
  void AddAdditionalSchemes(Schemes* schemes) override;
  std::string GetUserAgent() const override;
  base::string16 GetLocalizedString(int message_id) const override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const override;
   base::RefCountedMemory* GetDataResourceBytes(
      int resource_id) const override;
  gfx::Image& GetNativeImageNamed(int resource_id) const override;

  // Values are registered with all processes (url/url_util.h) and with Blink
  // (SchemeRegistry) unless otherwise indicated.
  struct SchemeInfo {
    // Lower-case ASCII scheme name.
    std::string scheme_name;

    // A scheme that is subject to URL canonicalization and parsing rules as
    // defined in the Common Internet Scheme Syntax RFC 1738 Section 3.1
    // available at http://www.ietf.org/rfc/rfc1738.txt.
    // This value is not registered with Blink.
    bool is_standard;

    // A scheme that will be treated the same as "file". For example, normal
    // pages cannot link to or access URLs of this scheme.
    bool is_local;

    // A scheme that can only be displayed from other content hosted with the
    // same scheme. For example, pages in other origins cannot create iframes or
    // hyperlinks to URLs with the scheme. For schemes that must be accessible
    // from other schemes set this value to false, set |is_cors_enabled| to
    // true, and use CORS "Access-Control-Allow-Origin" headers to further
    // restrict access.
    // This value is registered with Blink only.
    bool is_display_isolated;
    
    // A scheme that will be treated the same as "https". For example, loading
    // this scheme from other secure schemes will not trigger mixed content
    // warnings.
    bool is_secure;

    // A scheme that can be sent CORS requests. This value should be true in
    // most cases where |is_standard| is true.
    bool is_cors_enabled;

    // A scheme that can bypass Content-Security-Policy (CSP) checks. This value
    // should be false in most cases where |is_standard| is true.
    bool is_csp_bypassing;
  };
  typedef std::list<SchemeInfo> SchemeInfoList;

  // Custom scheme information will be registered first with all processes
  // (url/url_util.h) via CefContentClient::AddAdditionalSchemes which calls
  // AddCustomScheme, and second with Blink (SchemeRegistry) via
  // CefContentRendererClient::WebKitInitialized which calls GetCustomSchemes.
  void AddCustomScheme(const SchemeInfo& scheme_info);
  const SchemeInfoList* GetCustomSchemes();
  bool HasCustomScheme(const std::string& scheme_name);

  CefRefPtr<CefApp> application() const { return application_; }

  void set_pack_loading_disabled(bool val) { pack_loading_disabled_ = val; }
  bool pack_loading_disabled() const { return pack_loading_disabled_; }
  void set_allow_pack_file_load(bool val) { allow_pack_file_load_ = val; }

  static void SetPDFEntryFunctions(
      content::PepperPluginInfo::GetInterfaceFunc get_interface,
      content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
      content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module);

 private:
  // ui::ResourceBundle::Delegate methods.
  base::FilePath GetPathForResourcePack(
      const base::FilePath& pack_path,
      ui::ScaleFactor scale_factor) override;
  base::FilePath GetPathForLocalePack(
      const base::FilePath& pack_path,
      const std::string& locale) override;
  gfx::Image GetImageNamed(int resource_id) override;
  gfx::Image GetNativeImageNamed(int resource_id) override;
  base::RefCountedStaticMemory* LoadDataResourceBytes(
      int resource_id,
      ui::ScaleFactor scale_factor) override;
  bool GetRawDataResource(int resource_id,
                          ui::ScaleFactor scale_factor,
                          base::StringPiece* value) override;
  bool GetLocalizedString(int message_id,
                          base::string16* value) override;

  CefRefPtr<CefApp> application_;
  bool pack_loading_disabled_;
  bool allow_pack_file_load_;

  // Custom schemes handled by the client.
  SchemeInfoList scheme_info_list_;
  bool scheme_info_list_locked_;
};

#endif  // CEF_LIBCEF_COMMON_CONTENT_CLIENT_H_
