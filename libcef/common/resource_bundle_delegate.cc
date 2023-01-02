#include "libcef/common/resource_bundle_delegate.h"

#include "libcef/common/app_manager.h"

base::FilePath CefResourceBundleDelegate::GetPathForResourcePack(
    const base::FilePath& pack_path,
    ui::ResourceScaleFactor scale_factor) {
  // Only allow the cef pack file to load.
  if (!pack_loading_disabled_ && allow_pack_file_load_) {
    return pack_path;
  }
  return base::FilePath();
}

base::FilePath CefResourceBundleDelegate::GetPathForLocalePack(
    const base::FilePath& pack_path,
    const std::string& locale) {
  if (!pack_loading_disabled_) {
    return pack_path;
  }
  return base::FilePath();
}

gfx::Image CefResourceBundleDelegate::GetImageNamed(int resource_id) {
  return gfx::Image();
}

gfx::Image CefResourceBundleDelegate::GetNativeImageNamed(int resource_id) {
  return gfx::Image();
}

base::RefCountedMemory* CefResourceBundleDelegate::LoadDataResourceBytes(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return nullptr;
}

absl::optional<std::string> CefResourceBundleDelegate::LoadDataResourceString(
    int resource_id) {
  return absl::nullopt;
}

bool CefResourceBundleDelegate::GetRawDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor,
    base::StringPiece* value) const {
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
          *value = base::StringPiece(static_cast<char*>(data), data_size);
        }
      } else if (handler->GetDataResource(resource_id, data, data_size)) {
        *value = base::StringPiece(static_cast<char*>(data), data_size);
      }
    }
  }

  return (pack_loading_disabled_ || !value->empty());
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

  return (pack_loading_disabled_ || !value->empty());
}
