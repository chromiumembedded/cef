#include "cef/libcef/common/resource_bundle_delegate.h"

#include "cef/libcef/common/app_manager.h"

CefResourceBundleDelegate::CefResourceBundleDelegate() = default;

base::FilePath CefResourceBundleDelegate::GetPathForResourcePack(
    const base::FilePath& pack_path,
    ui::ResourceScaleFactor scale_factor) {
  return pack_path;
}

base::FilePath CefResourceBundleDelegate::GetPathForLocalePack(
    const base::FilePath& pack_path,
    const std::string& locale) {
  return pack_path;
}

gfx::Image CefResourceBundleDelegate::GetImageNamed(int resource_id) {
  return gfx::Image();
}

gfx::Image CefResourceBundleDelegate::GetNativeImageNamed(int resource_id) {
  return gfx::Image();
}

bool CefResourceBundleDelegate::HasDataResource(int resource_id) const {
  // This has no impact on the loading of resources in ResourceBundle.
  return false;
}

base::RefCountedMemory* CefResourceBundleDelegate::LoadDataResourceBytes(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return nullptr;
}

std::optional<std::string> CefResourceBundleDelegate::LoadDataResourceString(
    int resource_id) {
  return std::nullopt;
}

bool CefResourceBundleDelegate::GetRawDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor,
    std::string_view* value) const {
  auto application = CefAppManager::Get()->GetApplication();
  if (application) {
    CefRefPtr<CefResourceBundleHandler> handler =
        application->GetResourceBundleHandler();
    if (handler.get()) {
      void* data = nullptr;
      size_t data_size = 0;
      if (scale_factor != ui::kScaleFactorNone) {
        if (handler->GetDataResourceForScale(
                resource_id, static_cast<cef_scale_factor_t>(scale_factor),
                data, data_size)) {
          *value = std::string_view(static_cast<char*>(data), data_size);
        }
      } else if (handler->GetDataResource(resource_id, data, data_size)) {
        *value = std::string_view(static_cast<char*>(data), data_size);
      }
    }
  }

  return !value->empty();
}

bool CefResourceBundleDelegate::GetLocalizedString(
    int message_id,
    std::u16string* value) const {
  auto application = CefAppManager::Get()->GetApplication();
  if (application) {
    CefRefPtr<CefResourceBundleHandler> handler =
        application->GetResourceBundleHandler();
    if (handler.get()) {
      CefString cef_str;
      if (handler->GetLocalizedString(message_id, cef_str)) {
        *value = cef_str;
      }
    }
  }

  return !value->empty();
}
