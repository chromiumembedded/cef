#include "libcef/common/resource_bundle_delegate.h"

#include "libcef/common/content_client.h"

base::FilePath CefResourceBundleDelegate::GetPathForResourcePack(
    const base::FilePath& pack_path,
    ui::ScaleFactor scale_factor) {
  // Only allow the cef pack file to load.
  if (!content_client_->pack_loading_disabled() &&
      content_client_->allow_pack_file_load()) {
    return pack_path;
  }
  return base::FilePath();
}

base::FilePath CefResourceBundleDelegate::GetPathForLocalePack(
    const base::FilePath& pack_path,
    const std::string& locale) {
  if (!content_client_->pack_loading_disabled())
    return pack_path;
  return base::FilePath();
}

gfx::Image CefResourceBundleDelegate::GetImageNamed(int resource_id) {
  return gfx::Image();
}

gfx::Image CefResourceBundleDelegate::GetNativeImageNamed(int resource_id) {
  return gfx::Image();
}

base::RefCountedStaticMemory* CefResourceBundleDelegate::LoadDataResourceBytes(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return NULL;
}

bool CefResourceBundleDelegate::GetRawDataResource(int resource_id,
                                                   ui::ScaleFactor scale_factor,
                                                   base::StringPiece* value) {
  if (content_client_->application().get()) {
    CefRefPtr<CefResourceBundleHandler> handler =
        content_client_->application()->GetResourceBundleHandler();
    if (handler.get()) {
      void* data = NULL;
      size_t data_size = 0;
      if (scale_factor != ui::SCALE_FACTOR_NONE) {
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

  return (content_client_->pack_loading_disabled() || !value->empty());
}

bool CefResourceBundleDelegate::GetLocalizedString(int message_id,
                                                   base::string16* value) {
  if (content_client_->application().get()) {
    CefRefPtr<CefResourceBundleHandler> handler =
        content_client_->application()->GetResourceBundleHandler();
    if (handler.get()) {
      CefString cef_str;
      if (handler->GetLocalizedString(message_id, cef_str))
        *value = cef_str;
    }
  }

  return (content_client_->pack_loading_disabled() || !value->empty());
}
