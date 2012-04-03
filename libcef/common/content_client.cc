// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/content_client.h"
#include "include/cef_stream.h"
#include "include/cef_version.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "content/public/common/content_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/user_agent.h"

CefContentClient::CefContentClient(CefRefPtr<CefApp> application)
    : application_(application),
      pack_loading_disabled_(false) {
}

CefContentClient::~CefContentClient() {
}

// static
CefContentClient* CefContentClient::Get() {
  return static_cast<CefContentClient*>(content::GetContentClient());
}

void CefContentClient::SetActiveURL(const GURL& url) {
}

void CefContentClient::SetGpuInfo(const content::GPUInfo& gpu_info) {
}

void CefContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
}

void CefContentClient::AddNPAPIPlugins(
    webkit::npapi::PluginList* plugin_list) {
}

bool CefContentClient::HasWebUIScheme(const GURL& url) const {
  // There are no WebUI URLs in CEF.
  return false;
}

bool CefContentClient::CanHandleWhileSwappedOut(const IPC::Message& msg) {
  return false;
}

std::string CefContentClient::GetUserAgent(bool* overriding) const {
  static CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUserAgent)) {
    *overriding = true;
    return command_line.GetSwitchValueASCII(switches::kUserAgent);
  } else {
    std::string product_version;

    if (command_line.HasSwitch(switches::kProductVersion)) {
      *overriding = true;
      product_version =
          command_line.GetSwitchValueASCII(switches::kProductVersion);
    } else {
      *overriding = false;
      product_version = base::StringPrintf("Chrome/%d.%d.%d.%d",
          CHROME_VERSION_MAJOR, CHROME_VERSION_MINOR, CHROME_VERSION_BUILD,
          CHROME_VERSION_PATCH);
    }

    return webkit_glue::BuildUserAgentFromProduct(product_version);
  }
}

string16 CefContentClient::GetLocalizedString(int message_id) const {
  string16 value;

  if (application_.get()) {
    CefRefPtr<CefResourceBundleHandler> handler =
        application_->GetResourceBundleHandler();
    if (handler.get()) {
      CefString cef_str;
      if (handler->GetLocalizedString(message_id, cef_str))
        value = cef_str;
    }
  }

  if (value.empty() && !pack_loading_disabled_)
    value = ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);

  if (value.empty())
    LOG(ERROR) << "No localized string available for id " << message_id;

  return value;
}

base::StringPiece CefContentClient::GetDataResource(int resource_id) const {
  base::StringPiece value;

  if (application_.get()) {
    CefRefPtr<CefResourceBundleHandler> handler =
        application_->GetResourceBundleHandler();
    if (handler.get()) {
      void* data = NULL;
      size_t data_size = 0;
      if (handler->GetDataResource(resource_id, data, data_size))
        value = base::StringPiece(static_cast<char*>(data), data_size);
    }
  }

  if (value.empty() && !pack_loading_disabled_)
    value = ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);

  if (value.empty())
    LOG(ERROR) << "No data resource available for id " << resource_id;

  return value;
}

#if defined(OS_WIN)
bool CefContentClient::SandboxPlugin(CommandLine* command_line,
                                       sandbox::TargetPolicy* policy) {
  return false;
}
#endif

#if defined(OS_MACOSX)
bool CefContentClient::GetSandboxProfileForSandboxType(
    int sandbox_type,
    int* sandbox_profile_resource_id) const {
  return false;
}
#endif
