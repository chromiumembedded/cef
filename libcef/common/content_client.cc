// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/content_client.h"

#include <stdint.h>

#include "include/cef_stream.h"
#include "include/cef_version.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/extensions/pdf_extension_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/scheme_registration.h"
#include "libcef/common/scheme_registrar_impl.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pepper_flash.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pepper_plugin_info.h"
#include "content/public/common/user_agent.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_LINUX)
#include "libcef/common/widevine_loader.h"
#endif

namespace {

CefContentClient* g_content_client = NULL;

// The following plugin-related methods are from
// chrome/common/chrome_content_client.cc

const char kPDFPluginExtension[] = "pdf";
const char kPDFPluginDescription[] = "Portable Document Format";
const char kPDFPluginOutOfProcessMimeType[] =
    "application/x-google-chrome-pdf";
const uint32_t kPDFPluginPermissions = ppapi::PERMISSION_PRIVATE |
                                       ppapi::PERMISSION_DEV;

content::PepperPluginInfo::GetInterfaceFunc g_pdf_get_interface;
content::PepperPluginInfo::PPP_InitializeModuleFunc g_pdf_initialize_module;
content::PepperPluginInfo::PPP_ShutdownModuleFunc g_pdf_shutdown_module;

// Appends the known built-in plugins to the given vector. Some built-in
// plugins are "internal" which means they are compiled into the Chrome binary,
// and some are extra shared libraries distributed with the browser (these are
// not marked internal, aside from being automatically registered, they're just
// regular plugins).
void ComputeBuiltInPlugins(std::vector<content::PepperPluginInfo>* plugins) {
  if (extensions::PdfExtensionEnabled()) {
    content::PepperPluginInfo pdf_info;
    pdf_info.is_internal = true;
    pdf_info.is_out_of_process = true;
    pdf_info.name = extensions::pdf_extension_util::kPdfPluginName;
    pdf_info.description = kPDFPluginDescription;
    pdf_info.path =
        base::FilePath::FromUTF8Unsafe(CefContentClient::kPDFPluginPath);
    content::WebPluginMimeType pdf_mime_type(
        kPDFPluginOutOfProcessMimeType,
        kPDFPluginExtension,
        kPDFPluginDescription);
    pdf_info.mime_types.push_back(pdf_mime_type);
    pdf_info.internal_entry_points.get_interface = g_pdf_get_interface;
    pdf_info.internal_entry_points.initialize_module = g_pdf_initialize_module;
    pdf_info.internal_entry_points.shutdown_module = g_pdf_shutdown_module;
    pdf_info.permissions = kPDFPluginPermissions;
    plugins->push_back(pdf_info);
  }
}

content::PepperPluginInfo CreatePepperFlashInfo(const base::FilePath& path,
                                                const std::string& version) {
  content::PepperPluginInfo plugin;

  plugin.is_out_of_process = true;
  plugin.name = content::kFlashPluginName;
  plugin.path = path;
  plugin.permissions = chrome::kPepperFlashPermissions;

  std::vector<std::string> flash_version_numbers = base::SplitString(
      version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (flash_version_numbers.size() < 1)
    flash_version_numbers.push_back("11");
  if (flash_version_numbers.size() < 2)
    flash_version_numbers.push_back("2");
  if (flash_version_numbers.size() < 3)
    flash_version_numbers.push_back("999");
  if (flash_version_numbers.size() < 4)
    flash_version_numbers.push_back("999");
  // E.g., "Shockwave Flash 10.2 r154":
  plugin.description = plugin.name + " " + flash_version_numbers[0] + "." +
      flash_version_numbers[1] + " r" + flash_version_numbers[2];
  plugin.version = base::JoinString(flash_version_numbers, ".");
  content::WebPluginMimeType swf_mime_type(content::kFlashPluginSwfMimeType,
                                           content::kFlashPluginSwfExtension,
                                           content::kFlashPluginSwfDescription);
  plugin.mime_types.push_back(swf_mime_type);
  content::WebPluginMimeType spl_mime_type(content::kFlashPluginSplMimeType,
                                           content::kFlashPluginSplExtension,
                                           content::kFlashPluginSplDescription);
  plugin.mime_types.push_back(spl_mime_type);

  return plugin;
}

void AddPepperFlashFromCommandLine(
    std::vector<content::PepperPluginInfo>* plugins) {
  const base::CommandLine::StringType flash_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kPpapiFlashPath);
  if (flash_path.empty())
    return;

  // Also get the version from the command-line. Should be something like 11.2
  // or 11.2.123.45.
  std::string flash_version =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPpapiFlashVersion);

  plugins->push_back(
      CreatePepperFlashInfo(base::FilePath(flash_path), flash_version));
}

bool GetSystemPepperFlash(content::PepperPluginInfo* plugin) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kEnableSystemFlash))
    return false;

  // Do not try and find System Pepper Flash if there is a specific path on
  // the commmand-line.
  if (command_line->HasSwitch(switches::kPpapiFlashPath))
    return false;

  base::FilePath flash_filename;
  if (!PathService::Get(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN,
                        &flash_filename)) {
    return false;
  }

  base::FilePath manifest_path(
      flash_filename.DirName().AppendASCII("manifest.json"));

  std::string manifest_data;
  if (!base::ReadFileToString(manifest_path, &manifest_data))
    return false;
  std::unique_ptr<base::Value> manifest_value(
      base::JSONReader::Read(manifest_data, base::JSON_ALLOW_TRAILING_COMMAS));
  if (!manifest_value.get())
    return false;
  base::DictionaryValue* manifest = NULL;
  if (!manifest_value->GetAsDictionary(&manifest))
    return false;

  base::Version version;
  if (!chrome::CheckPepperFlashManifest(*manifest, &version))
    return false;

  *plugin = CreatePepperFlashInfo(flash_filename, version.GetString());
  return true;
}

}  // namespace

const char CefContentClient::kPDFPluginPath[] = "internal-pdf-viewer";

CefContentClient::CefContentClient(CefRefPtr<CefApp> application)
    : application_(application),
      pack_loading_disabled_(false),
      allow_pack_file_load_(false),
      scheme_info_list_locked_(false) {
  DCHECK(!g_content_client);
  g_content_client = this;
}

CefContentClient::~CefContentClient() {
  g_content_client = NULL;
}

// static
CefContentClient* CefContentClient::Get() {
  return g_content_client;
}

void CefContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
  ComputeBuiltInPlugins(plugins);
  AddPepperFlashFromCommandLine(plugins);

#if defined(OS_LINUX)
#if defined(WIDEVINE_CDM_AVAILABLE) && BUILDFLAG(ENABLE_PEPPER_CDMS)
  CefWidevineLoader::AddPepperPlugins(plugins);
#endif
#endif

  content::PepperPluginInfo plugin;
  if (GetSystemPepperFlash(&plugin))
    plugins->push_back(plugin);
}

void CefContentClient::AddAdditionalSchemes(Schemes* schemes) {
  DCHECK(!scheme_info_list_locked_);

  if (application_.get()) {
    CefSchemeRegistrarImpl schemeRegistrar;
    application_->OnRegisterCustomSchemes(&schemeRegistrar);
    schemeRegistrar.GetSchemes(schemes);
  }

  scheme::AddInternalSchemes(schemes);

  scheme_info_list_locked_ = true;
}

std::string CefContentClient::GetUserAgent() const {
  std::string product_version;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent))
    return command_line->GetSwitchValueASCII(switches::kUserAgent);

  if (command_line->HasSwitch(switches::kProductVersion)) {
    product_version =
        command_line->GetSwitchValueASCII(switches::kProductVersion);
  } else {
    product_version = base::StringPrintf("Chrome/%d.%d.%d.%d",
        CHROME_VERSION_MAJOR, CHROME_VERSION_MINOR, CHROME_VERSION_BUILD,
        CHROME_VERSION_PATCH);
  }

  return content::BuildUserAgentFromProduct(product_version);
}

base::string16 CefContentClient::GetLocalizedString(int message_id) const {
  base::string16 value =
      ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
  if (value.empty())
    LOG(ERROR) << "No localized string available for id " << message_id;

  return value;
}

base::StringPiece CefContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  base::StringPiece value =
      ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          resource_id, scale_factor);
  if (value.empty())
    LOG(ERROR) << "No data resource available for id " << resource_id;

  return value;
}

base::RefCountedMemory* CefContentClient::GetDataResourceBytes(
    int resource_id) const {
  base::RefCountedMemory* value =
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id);
  if (!value)
    LOG(ERROR) << "No data resource bytes available for id " << resource_id;

  return value;
}

gfx::Image& CefContentClient::GetNativeImageNamed(int resource_id) const {
  gfx::Image& value =
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
  if (value.IsEmpty())
    LOG(ERROR) << "No native image available for id " << resource_id;

  return value;
}

void CefContentClient::AddCustomScheme(const SchemeInfo& scheme_info) {
  DCHECK(!scheme_info_list_locked_);
  scheme_info_list_.push_back(scheme_info);

  if (CefContentBrowserClient::Get()) {
    CefContentBrowserClient::Get()->RegisterCustomScheme(
        scheme_info.scheme_name);
  }
}

const CefContentClient::SchemeInfoList* CefContentClient::GetCustomSchemes() {
  DCHECK(scheme_info_list_locked_);
  return &scheme_info_list_;
}

bool CefContentClient::HasCustomScheme(const std::string& scheme_name) {
  DCHECK(scheme_info_list_locked_);
  if (scheme_info_list_.empty())
    return false;

  SchemeInfoList::const_iterator it = scheme_info_list_.begin();
  for (; it != scheme_info_list_.end(); ++it) {
    if (it->scheme_name == scheme_name)
      return true;
  }

  return false;
}

// static
void CefContentClient::SetPDFEntryFunctions(
    content::PepperPluginInfo::GetInterfaceFunc get_interface,
    content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
    content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module) {
  g_pdf_get_interface = get_interface;
  g_pdf_initialize_module = initialize_module;
  g_pdf_shutdown_module = shutdown_module;
}

base::FilePath CefContentClient::GetPathForResourcePack(
    const base::FilePath& pack_path,
    ui::ScaleFactor scale_factor) {
  // Only allow the cef pack file to load.
  if (!pack_loading_disabled_ && allow_pack_file_load_)
    return pack_path;
  return base::FilePath();
}

base::FilePath CefContentClient::GetPathForLocalePack(
    const base::FilePath& pack_path,
    const std::string& locale) {
  if (!pack_loading_disabled_)
    return pack_path;
  return base::FilePath();
}

gfx::Image CefContentClient::GetImageNamed(int resource_id) {
  return gfx::Image();
}

gfx::Image CefContentClient::GetNativeImageNamed(int resource_id) {
  return gfx::Image();
}

base::RefCountedStaticMemory* CefContentClient::LoadDataResourceBytes(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return NULL;
}

bool CefContentClient::GetRawDataResource(int resource_id,
                                          ui::ScaleFactor scale_factor,
                                          base::StringPiece* value) {
  if (application_.get()) {
    CefRefPtr<CefResourceBundleHandler> handler =
        application_->GetResourceBundleHandler();
    if (handler.get()) {
      void* data = NULL;
      size_t data_size = 0;
      if (scale_factor != ui::SCALE_FACTOR_NONE) {
        if (handler->GetDataResourceForScale(
              resource_id, static_cast<cef_scale_factor_t>(scale_factor), data,
              data_size)) {
          *value = base::StringPiece(static_cast<char*>(data), data_size);
        }
      } else if (handler->GetDataResource(resource_id, data, data_size)) {
        *value = base::StringPiece(static_cast<char*>(data), data_size);
      }
    }
  }

  return (pack_loading_disabled_ || !value->empty());
}

bool CefContentClient::GetLocalizedString(int message_id,
                                          base::string16* value) {
  if (application_.get()) {
    CefRefPtr<CefResourceBundleHandler> handler =
        application_->GetResourceBundleHandler();
    if (handler.get()) {
      CefString cef_str;
      if (handler->GetLocalizedString(message_id, cef_str))
        *value = cef_str;
    }
  }

  return (pack_loading_disabled_ || !value->empty());
}
