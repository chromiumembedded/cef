// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/extension_util.h"

#include <algorithm>
#include <memory>

#include "include/base/cef_callback.h"
#include "include/base/cef_cxx17_backports.h"
#include "include/cef_parser.h"
#include "include/cef_path_util.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/shared/browser/file_util.h"
#include "tests/shared/browser/resource_util.h"

namespace client {
namespace extension_util {

namespace {

std::string GetResourcesPath() {
  CefString resources_dir;
  if (CefGetPath(PK_DIR_RESOURCES, resources_dir) && !resources_dir.empty()) {
    return resources_dir.ToString() + file_util::kPathSep;
  }
  return std::string();
}

// Internal extension paths may be prefixed with PK_DIR_RESOURCES and always
// use forward slash as path separator.
std::string GetInternalPath(const std::string& extension_path) {
  std::string resources_path_lower = GetResourcesPath();
  std::string extension_path_lower = extension_path;

#if defined(OS_WIN)
  // Convert to lower-case, since Windows paths are case-insensitive.
  std::transform(resources_path_lower.begin(), resources_path_lower.end(),
                 resources_path_lower.begin(), ::tolower);
  std::transform(extension_path_lower.begin(), extension_path_lower.end(),
                 extension_path_lower.begin(), ::tolower);
#endif

  std::string internal_path;
  if (!resources_path_lower.empty() &&
      extension_path_lower.find(resources_path_lower) == 0U) {
    internal_path = extension_path.substr(resources_path_lower.size());
  } else {
    internal_path = extension_path;
  }

#if defined(OS_WIN)
  // Normalize path separators.
  std::replace(internal_path.begin(), internal_path.end(), '\\', '/');
#endif

  return internal_path;
}

using ManifestCallback =
    base::OnceCallback<void(CefRefPtr<CefDictionaryValue> /*manifest*/)>;

void RunManifestCallback(ManifestCallback callback,
                         CefRefPtr<CefDictionaryValue> manifest) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the browser UI thread.
    CefPostTask(TID_UI, base::BindOnce(std::move(callback), manifest));
    return;
  }
  std::move(callback).Run(manifest);
}

// Asynchronously reads the manifest and executes |callback| on the UI thread.
void GetInternalManifest(const std::string& extension_path,
                         ManifestCallback callback) {
  if (!CefCurrentlyOn(TID_FILE_USER_BLOCKING)) {
    // Execute on the browser FILE thread.
    CefPostTask(TID_FILE_USER_BLOCKING,
                base::BindOnce(GetInternalManifest, extension_path,
                               std::move(callback)));
    return;
  }

  const std::string& manifest_path = GetInternalExtensionResourcePath(
      file_util::JoinPath(extension_path, "manifest.json"));
  std::string manifest_contents;
  if (!LoadBinaryResource(manifest_path.c_str(), manifest_contents) ||
      manifest_contents.empty()) {
    LOG(ERROR) << "Failed to load manifest from " << manifest_path;
    RunManifestCallback(std::move(callback), nullptr);
    return;
  }

  CefString error_msg;
  CefRefPtr<CefValue> value =
      CefParseJSONAndReturnError(manifest_contents, JSON_PARSER_RFC, error_msg);
  if (!value || value->GetType() != VTYPE_DICTIONARY) {
    if (error_msg.empty())
      error_msg = "Incorrectly formatted dictionary contents.";
    LOG(ERROR) << "Failed to parse manifest from " << manifest_path << "; "
               << error_msg.ToString();
    RunManifestCallback(std::move(callback), nullptr);
    return;
  }

  RunManifestCallback(std::move(callback), value->GetDictionary());
}

void LoadExtensionWithManifest(CefRefPtr<CefRequestContext> request_context,
                               const std::string& extension_path,
                               CefRefPtr<CefExtensionHandler> handler,
                               CefRefPtr<CefDictionaryValue> manifest) {
  CEF_REQUIRE_UI_THREAD();

  // Load the extension internally. Resource requests will be handled via
  // AddInternalExtensionToResourceManager.
  request_context->LoadExtension(extension_path, manifest, handler);
}

}  // namespace

bool IsInternalExtension(const std::string& extension_path) {
  // List of internally handled extensions.
  static const char* extensions[] = {"set_page_color"};

  const std::string& internal_path = GetInternalPath(extension_path);
  for (size_t i = 0; i < base::size(extensions); ++i) {
    // Exact match or first directory component.
    const std::string& extension = extensions[i];
    if (internal_path == extension ||
        internal_path.find(extension + '/') == 0) {
      return true;
    }
  }

  return false;
}

std::string GetInternalExtensionResourcePath(
    const std::string& extension_path) {
  return "extensions/" + GetInternalPath(extension_path);
}

std::string GetExtensionResourcePath(const std::string& extension_path,
                                     bool* internal) {
  const bool is_internal = IsInternalExtension(extension_path);
  if (internal)
    *internal = is_internal;
  if (is_internal)
    return GetInternalExtensionResourcePath(extension_path);
  return extension_path;
}

bool GetExtensionResourceContents(const std::string& extension_path,
                                  std::string& contents) {
  CEF_REQUIRE_FILE_USER_BLOCKING_THREAD();

  if (IsInternalExtension(extension_path)) {
    const std::string& contents_path =
        GetInternalExtensionResourcePath(extension_path);
    return LoadBinaryResource(contents_path.c_str(), contents);
  }

  return file_util::ReadFileToString(extension_path, &contents);
}

void LoadExtension(CefRefPtr<CefRequestContext> request_context,
                   const std::string& extension_path,
                   CefRefPtr<CefExtensionHandler> handler) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute on the browser UI thread.
    CefPostTask(TID_UI, base::BindOnce(LoadExtension, request_context,
                                       extension_path, handler));
    return;
  }

  if (IsInternalExtension(extension_path)) {
    // Read the extension manifest and load asynchronously.
    GetInternalManifest(
        extension_path,
        base::BindOnce(LoadExtensionWithManifest, request_context,
                       extension_path, handler));
  } else {
    // Load the extension from disk.
    request_context->LoadExtension(extension_path, nullptr, handler);
  }
}

void AddInternalExtensionToResourceManager(
    CefRefPtr<CefExtension> extension,
    CefRefPtr<CefResourceManager> resource_manager) {
  DCHECK(IsInternalExtension(extension->GetPath()));

  if (!CefCurrentlyOn(TID_IO)) {
    // Execute on the browser IO thread.
    CefPostTask(TID_IO, base::BindOnce(AddInternalExtensionToResourceManager,
                                       extension, resource_manager));
    return;
  }

  const std::string& origin = GetExtensionOrigin(extension->GetIdentifier());
  const std::string& resource_path =
      GetInternalExtensionResourcePath(extension->GetPath());

// Add provider for bundled resource files.
#if defined(OS_WIN)
  // Read resources from the binary.
  resource_manager->AddProvider(
      CreateBinaryResourceProvider(origin, resource_path), 50, std::string());
#elif defined(OS_POSIX)
  // Read resources from a directory on disk.
  std::string resource_dir;
  if (GetResourceDir(resource_dir)) {
    resource_dir += "/" + resource_path;
    resource_manager->AddDirectoryProvider(origin, resource_dir, 50,
                                           std::string());
  }
#endif
}

std::string GetExtensionOrigin(const std::string& extension_id) {
  return "chrome-extension://" + extension_id + "/";
}

std::string GetExtensionURL(CefRefPtr<CefExtension> extension) {
  CefRefPtr<CefDictionaryValue> browser_action =
      extension->GetManifest()->GetDictionary("browser_action");
  if (browser_action) {
    const std::string& default_popup =
        browser_action->GetString("default_popup");
    if (!default_popup.empty())
      return GetExtensionOrigin(extension->GetIdentifier()) + default_popup;
  }

  return std::string();
}

std::string GetExtensionIconPath(CefRefPtr<CefExtension> extension,
                                 bool* internal) {
  CefRefPtr<CefDictionaryValue> browser_action =
      extension->GetManifest()->GetDictionary("browser_action");
  if (browser_action) {
    const std::string& default_icon = browser_action->GetString("default_icon");
    if (!default_icon.empty()) {
      return GetExtensionResourcePath(
          file_util::JoinPath(extension->GetPath(), default_icon), internal);
    }
  }

  return std::string();
}

}  // namespace extension_util
}  // namespace client
