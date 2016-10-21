// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/pepper/browser_pepper_host_factory.h"

#include "build/build_config.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/renderer_host/pepper/pepper_flash_browser_host.h"
#include "chrome/browser/renderer_host/pepper/pepper_flash_clipboard_message_filter.h"
#include "chrome/browser/renderer_host/pepper/pepper_flash_drm_host.h"
#include "chrome/browser/renderer_host/pepper/pepper_isolated_file_system_message_filter.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/host/message_filter_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

using ppapi::host::MessageFilterHost;
using ppapi::host::ResourceHost;
using ppapi::host::ResourceMessageFilter;

CefBrowserPepperHostFactory::CefBrowserPepperHostFactory(
    content::BrowserPpapiHost* host)
    : host_(host) {}

CefBrowserPepperHostFactory::~CefBrowserPepperHostFactory() {}

std::unique_ptr<ResourceHost> CefBrowserPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    PP_Resource resource,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK(host == host_->GetPpapiHost());

  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return nullptr;

  // Flash interfaces.
  if (host_->GetPpapiHost()->permissions().HasPermission(
          ppapi::PERMISSION_FLASH)) {
    switch (message.type()) {
      case PpapiHostMsg_Flash_Create::ID:
        return base::MakeUnique<chrome::PepperFlashBrowserHost>(host_, instance,
                                                                resource);
      case PpapiHostMsg_FlashClipboard_Create::ID: {
        scoped_refptr<ResourceMessageFilter> clipboard_filter(
            new chrome::PepperFlashClipboardMessageFilter);
        return base::MakeUnique<MessageFilterHost>(
            host_->GetPpapiHost(), instance, resource, clipboard_filter);
      }
      case PpapiHostMsg_FlashDRM_Create::ID:
        return base::MakeUnique<chrome::PepperFlashDRMHost>(host_, instance,
                                                            resource);
    }
  }

  // Permissions for the following interfaces will be checked at the
  // time of the corresponding instance's methods calls (because
  // permission check can be performed only on the UI
  // thread). Currently these interfaces are available only for
  // whitelisted apps which may not have access to the other private
  // interfaces.
  if (message.type() == PpapiHostMsg_IsolatedFileSystem_Create::ID) {
    chrome::PepperIsolatedFileSystemMessageFilter* isolated_fs_filter =
        chrome::PepperIsolatedFileSystemMessageFilter::Create(instance, host_);
    if (!isolated_fs_filter)
      return nullptr;
    return base::MakeUnique<MessageFilterHost>(host, instance, resource,
                                               isolated_fs_filter);
  }

  NOTREACHED() << "Unhandled message type: " << message.type();
  return nullptr;
}
