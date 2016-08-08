// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/image_impl.h"

#include <algorithm>

#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"

namespace {

SkColorType GetSkColorType(cef_color_type_t color_type) {
  switch (color_type) {
    case CEF_COLOR_TYPE_RGBA_8888:
      return kRGBA_8888_SkColorType;
    case CEF_COLOR_TYPE_BGRA_8888:
      return kBGRA_8888_SkColorType;
    default:
      break;
  }

  NOTREACHED();
  return kUnknown_SkColorType;
}

SkAlphaType GetSkAlphaType(cef_alpha_type_t alpha_type) {
  switch (alpha_type) {
    case CEF_ALPHA_TYPE_OPAQUE:
      return kOpaque_SkAlphaType;
    case CEF_ALPHA_TYPE_PREMULTIPLIED:
      return kPremul_SkAlphaType;
    case CEF_ALPHA_TYPE_POSTMULTIPLIED:
      return kUnpremul_SkAlphaType;
    default:
      break;
  }

  NOTREACHED();
  return kUnknown_SkAlphaType;
}

// Compress as PNG. Requires post-multiplied alpha.
bool PNGMethod(bool with_transparency,
               const SkBitmap& bitmap,
               std::vector<unsigned char>* compressed) {
  return gfx::PNGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getPixels()),
      bitmap.colorType() == kBGRA_8888_SkColorType ?
          gfx::PNGCodec::FORMAT_BGRA : gfx::PNGCodec::FORMAT_RGBA,
      gfx::Size(bitmap.width(), bitmap.height()),
      static_cast<int>(bitmap.rowBytes()),
      bitmap.alphaType() == kOpaque_SkAlphaType || !with_transparency,
      std::vector<gfx::PNGCodec::Comment>(),
      compressed);
}

// Compress as JPEG. This internally uses JCS_EXT_RGBX or JCS_EXT_BGRX which
// causes the alpha channel to be ignored. Requires post-multiplied alpha.
bool JPEGMethod(int quality,
                const SkBitmap& bitmap,
                std::vector<unsigned char>* compressed) {
  return gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getPixels()),
      bitmap.colorType() == kBGRA_8888_SkColorType ?
          gfx::JPEGCodec::FORMAT_BGRA : gfx::JPEGCodec::FORMAT_RGBA,
      bitmap.width(),
      bitmap.height(),
      static_cast<int>(bitmap.rowBytes()),
      quality,
      compressed);
}

}  // namespace

// static
CefRefPtr<CefImage> CefImage::CreateImage() {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  return new CefImageImpl();
}

CefImageImpl::CefImageImpl() {
  CEF_REQUIRE_UIT();
}

CefImageImpl::CefImageImpl(const gfx::ImageSkia& image_skia)
    : image_(image_skia) {
  CEF_REQUIRE_UIT();
}

CefImageImpl::~CefImageImpl() {
  CEF_REQUIRE_UIT();
}

bool CefImageImpl::IsEmpty() {
  CEF_REQUIRE_UIT_RETURN(false);
  return image_.IsEmpty();
}

bool CefImageImpl::IsSame(CefRefPtr<CefImage> that) {
  CEF_REQUIRE_UIT_RETURN(false);
  CefImageImpl* that_impl = static_cast<CefImageImpl*>(that.get());
  if (!that_impl)
    return false;

  // Quick check for the same object.
  if (this == that_impl)
    return true;

  return image_.AsImageSkia().BackedBySameObjectAs(
      that_impl->image_.AsImageSkia());
}

bool CefImageImpl::AddBitmap(float scale_factor,
                             int pixel_width,
                             int pixel_height,
                             cef_color_type_t color_type,
                             cef_alpha_type_t alpha_type,
                             const void* pixel_data,
                             size_t pixel_data_size) {
  CEF_REQUIRE_UIT_RETURN(false);
  const SkColorType ct = GetSkColorType(color_type);
  const SkAlphaType at = GetSkAlphaType(alpha_type);
 
  // Make sure the client passed in the expected values.
  if (ct != kBGRA_8888_SkColorType && ct != kRGBA_8888_SkColorType)
    return false;
  if (pixel_data_size != pixel_width * pixel_height * 4U)
    return false;

  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(
          SkImageInfo::Make(pixel_width, pixel_height, ct, at))) {
    return false;
  }

  DCHECK_EQ(pixel_data_size, bitmap.getSize());

  {
    SkAutoLockPixels bitmap_lock(bitmap);
    memcpy(bitmap.getPixels(), pixel_data, pixel_data_size);
  }

  return AddBitmap(scale_factor, bitmap);
}

bool CefImageImpl::AddPNG(float scale_factor,
                          const void* png_data,
                          size_t png_data_size) {
  CEF_REQUIRE_UIT_RETURN(false);

  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(static_cast<const unsigned char*>(png_data),
                             png_data_size, &bitmap)) {
    return false;
  }

  return AddBitmap(scale_factor, bitmap);
}

bool CefImageImpl::AddJPEG(float scale_factor,
                           const void* jpeg_data,
                           size_t jpeg_data_size) {
  CEF_REQUIRE_UIT_RETURN(false);

  std::unique_ptr<SkBitmap> bitmap(
      gfx::JPEGCodec::Decode(static_cast<const unsigned char*>(jpeg_data),
                             jpeg_data_size));
  if (!bitmap.get())
    return false;

  return AddBitmap(scale_factor, *bitmap);
}

size_t CefImageImpl::GetWidth() {
  CEF_REQUIRE_UIT_RETURN(false);
  return image_.Width();
}

size_t CefImageImpl::GetHeight() {
  CEF_REQUIRE_UIT_RETURN(false);
  return image_.Height();
}

bool CefImageImpl::HasRepresentation(float scale_factor) {
  CEF_REQUIRE_UIT_RETURN(false);
  return image_.AsImageSkia().HasRepresentation(scale_factor);
}

bool CefImageImpl::RemoveRepresentation(float scale_factor) {
  CEF_REQUIRE_UIT_RETURN(false);
  gfx::ImageSkia image_skia = image_.AsImageSkia();
  if (image_skia.HasRepresentation(scale_factor)) {
    image_skia.RemoveRepresentation(scale_factor);
    return true;
  }
  return false;
}

bool CefImageImpl::GetRepresentationInfo(float scale_factor,
                                         float& actual_scale_factor,
                                         int& pixel_width,
                                         int& pixel_height) {
  CEF_REQUIRE_UIT_RETURN(false);
  gfx::ImageSkia image_skia = image_.AsImageSkia();
  if (image_skia.isNull())
    return false;

  const gfx::ImageSkiaRep& rep = image_skia.GetRepresentation(scale_factor);
  if (rep.is_null())
    return false;

  actual_scale_factor = rep.scale();
  pixel_width = rep.sk_bitmap().width();
  pixel_height = rep.sk_bitmap().height();
  return true;
}

CefRefPtr<CefBinaryValue> CefImageImpl::GetAsBitmap(
    float scale_factor,
    cef_color_type_t color_type,
    cef_alpha_type_t alpha_type,
    int& pixel_width,
    int& pixel_height) {
  CEF_REQUIRE_UIT_RETURN(nullptr);

  const SkColorType desired_ct = GetSkColorType(color_type);
  const SkAlphaType desired_at = GetSkAlphaType(alpha_type);

  const SkBitmap* bitmap = GetBitmap(scale_factor);
  if (!bitmap)
    return nullptr;

  SkAutoLockPixels bitmap_lock(*bitmap);
  DCHECK(bitmap->readyToDraw());

  pixel_width = bitmap->width();
  pixel_height = bitmap->height();

  if (bitmap->colorType() == desired_ct && bitmap->alphaType() == desired_at) {
    // No conversion necessary.
    return CefBinaryValue::Create(bitmap->getPixels(), bitmap->getSize());
  } else {
    SkBitmap desired_bitmap;
    if (!ConvertBitmap(*bitmap, &desired_bitmap, desired_ct, desired_at))
      return nullptr;
    SkAutoLockPixels bitmap_lock(desired_bitmap);
    DCHECK(desired_bitmap.readyToDraw());
    return CefBinaryValue::Create(desired_bitmap.getPixels(),
                                  desired_bitmap.getSize());
  }
}

CefRefPtr<CefBinaryValue> CefImageImpl::GetAsPNG(float scale_factor,
                                                 bool with_transparency,
                                                 int& pixel_width,
                                                 int& pixel_height) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  const SkBitmap* bitmap = GetBitmap(scale_factor);
  if (!bitmap)
    return nullptr;

  std::vector<unsigned char> compressed;
  if (!WritePNG(*bitmap, &compressed, with_transparency))
    return nullptr;

  pixel_width = bitmap->width();
  pixel_height = bitmap->height();

  return CefBinaryValue::Create(&compressed.front(), compressed.size());
}

CefRefPtr<CefBinaryValue> CefImageImpl::GetAsJPEG(float scale_factor,
                                                  int quality,
                                                  int& pixel_width,
                                                  int& pixel_height) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  const SkBitmap* bitmap = GetBitmap(scale_factor);
  if (!bitmap)
    return nullptr;

  std::vector<unsigned char> compressed;
  if (!WriteJPEG(*bitmap, &compressed, quality))
    return nullptr;

  pixel_width = bitmap->width();
  pixel_height = bitmap->height();

  return CefBinaryValue::Create(&compressed.front(), compressed.size());
}

void CefImageImpl::AddBitmaps(int32_t scale_1x_size,
                              const std::vector<SkBitmap>& bitmaps) {
  if (scale_1x_size == 0) {
    // Set the scale 1x size to the smallest bitmap pixel size.
    int32_t min_size = std::numeric_limits<int32_t>::max();
    for (const SkBitmap& bitmap : bitmaps) {
      const int32_t size = std::max(bitmap.width(), bitmap.height());
      if (size < min_size)
        min_size = size;
    }
    scale_1x_size = min_size;
  }

  for (const SkBitmap& bitmap : bitmaps) {
    const int32_t size = std::max(bitmap.width(), bitmap.height());
    const float scale_factor = static_cast<float>(size) /
                               static_cast<float>(scale_1x_size);
    AddBitmap(scale_factor, bitmap);
  }
}

gfx::ImageSkia CefImageImpl::GetForced1xScaleRepresentation(
    float scale_factor) const {
  if (scale_factor == 1.0f) {
    // We can use the existing image without modification.
    return image_.AsImageSkia();
  }

  const SkBitmap* bitmap = GetBitmap(scale_factor);
  gfx::ImageSkia image_skia;
  if (bitmap)
    image_skia.AddRepresentation(gfx::ImageSkiaRep(*bitmap, 1.0f));
  return image_skia;
}

bool CefImageImpl::AddBitmap(float scale_factor,
                             const SkBitmap& bitmap) {
#if DCHECK_IS_ON()
  {
    SkAutoLockPixels bitmap_lock(bitmap);
    DCHECK(bitmap.readyToDraw());
  }
#endif
  DCHECK(bitmap.colorType() == kBGRA_8888_SkColorType ||
         bitmap.colorType() == kRGBA_8888_SkColorType);

  gfx::ImageSkiaRep skia_rep(bitmap, scale_factor);
  if (image_.IsEmpty()) {
    gfx::Image image((gfx::ImageSkia(skia_rep)));
    image_.SwapRepresentations(&image);
  } else {
    image_.AsImageSkia().AddRepresentation(skia_rep);
  }
  return true;
}

const SkBitmap* CefImageImpl::GetBitmap(float scale_factor) const {
  gfx::ImageSkia image_skia = image_.AsImageSkia();
  if (image_skia.isNull())
    return nullptr;

  const gfx::ImageSkiaRep& rep = image_skia.GetRepresentation(scale_factor);
  if (rep.is_null())
    return nullptr;

  return &rep.sk_bitmap();
}

// static
bool CefImageImpl::ConvertBitmap(const SkBitmap& src_bitmap,
                                 SkBitmap* target_bitmap,
                                 SkColorType target_ct,
                                 SkAlphaType target_at) {
  DCHECK(src_bitmap.readyToDraw());
  DCHECK(src_bitmap.colorType() != target_ct ||
         src_bitmap.alphaType() != target_at);
  DCHECK(target_bitmap);

  SkImageInfo target_info =
        SkImageInfo::Make(src_bitmap.width(), src_bitmap.height(), target_ct,
                          target_at);
  if (!target_bitmap->tryAllocPixels(target_info))
    return false;

  SkAutoLockPixels bitmap_lock(*target_bitmap);
  if (!src_bitmap.readPixels(target_info, target_bitmap->getPixels(),
                             target_bitmap->rowBytes(), 0, 0)) {
    return false;
  }

  DCHECK(target_bitmap->readyToDraw());
  return true;
}

// static
bool CefImageImpl::WriteCompressedFormat(const SkBitmap& bitmap,
                                         std::vector<unsigned char>* compressed,
                                         const CompressionMethod& method) {
  const SkBitmap* bitmap_ptr = nullptr;
  SkBitmap bitmap_postalpha;
  if (bitmap.alphaType() == kPremul_SkAlphaType) {
    // Compression methods require post-multiplied alpha values.
    SkAutoLockPixels bitmap_lock(bitmap);
    if (!ConvertBitmap(bitmap, &bitmap_postalpha, bitmap.colorType(),
                       kUnpremul_SkAlphaType)) {
      return false;
    }
    bitmap_ptr = &bitmap_postalpha;
  } else {
    bitmap_ptr = &bitmap;
  }

  SkAutoLockPixels bitmap_lock(*bitmap_ptr);
  DCHECK(bitmap_ptr->readyToDraw());
  DCHECK(bitmap_ptr->colorType() == kBGRA_8888_SkColorType ||
         bitmap_ptr->colorType() == kRGBA_8888_SkColorType);
  DCHECK(bitmap_ptr->alphaType() == kOpaque_SkAlphaType ||
         bitmap_ptr->alphaType() == kUnpremul_SkAlphaType);

  return method.Run(*bitmap_ptr, compressed);
}

// static
bool CefImageImpl::WritePNG(const SkBitmap& bitmap,
                            std::vector<unsigned char>* compressed,
                            bool with_transparency) {
  return WriteCompressedFormat(bitmap, compressed,
                               base::Bind(PNGMethod, with_transparency));
}

// static
bool CefImageImpl::WriteJPEG(const SkBitmap& bitmap,
                             std::vector<unsigned char>* compressed,
                             int quality) {
  return WriteCompressedFormat(bitmap, compressed,
                               base::Bind(JPEGMethod, quality));
}
