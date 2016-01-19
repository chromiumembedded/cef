// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_IMAGE_IMPL_H_
#define CEF_LIBCEF_BROWSER_IMAGE_IMPL_H_
#pragma once

#include "include/cef_image.h"
#include "libcef/browser/thread_util.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

class CefImageImpl : public CefImage {
 public:
  // Creates an empty image with no representations.
  CefImageImpl();

  // Creates a new image by copying the ImageSkia for use as the default
  // representation.
  explicit CefImageImpl(const gfx::ImageSkia& image_skia);

  // Deletes the image and, if the only owner of the storage, all of its cached
  // representations.
  ~CefImageImpl() override;

  // CefImage methods:
  bool IsEmpty() override;
  bool IsSame(CefRefPtr<CefImage> that) override;
  bool AddBitmap(float scale_factor,
                 int pixel_width,
                 int pixel_height,
                 cef_color_type_t color_type,
                 cef_alpha_type_t alpha_type,
                 const void* pixel_data,
                 size_t pixel_data_size) override;
  bool AddPNG(float scale_factor,
              const void* png_data,
              size_t png_data_size) override;
  bool AddJPEG(float scale_factor,
               const void* jpeg_data,
               size_t jpeg_data_size) override;
  size_t GetWidth() override;
  size_t GetHeight() override;
  bool HasRepresentation(float scale_factor) override;
  bool RemoveRepresentation(float scale_factor) override;
  bool GetRepresentationInfo(float scale_factor,
                             float& actual_scale_factor,
                             int& pixel_width,
                             int& pixel_height) override;
  CefRefPtr<CefBinaryValue> GetAsBitmap(float scale_factor,
                                        cef_color_type_t color_type,
                                        cef_alpha_type_t alpha_type,
                                        int& pixel_width,
                                        int& pixel_height) override;
  CefRefPtr<CefBinaryValue> GetAsPNG(float scale_factor,
                                     bool with_transparency,
                                     int& pixel_width,
                                     int& pixel_height) override;
  CefRefPtr<CefBinaryValue> GetAsJPEG(float scale_factor,
                                      int quality,
                                      int& pixel_width,
                                      int& pixel_height) override;

  // Add |bitmaps| which should be the same image at different scale factors.
  // |scale_1x_size| is the size in pixels of the 1x factor image. If
  // |scale_1x_size| is 0 the smallest image size in pixels will be used as the
  // 1x factor size.
  void AddBitmaps(int32_t scale_1x_size,
                  const std::vector<SkBitmap>& bitmaps);

  // Return a representation of this Image that contains only the bitmap nearest
  // |scale_factor| as the 1x scale representation. Conceptually this is an
  // incorrect representation but is necessary to work around bugs in the views
  // architecture.
  // TODO(cef): Remove once https://crbug.com/597732 is resolved.
  gfx::ImageSkia GetForced1xScaleRepresentation(float scale_factor) const;

  const gfx::Image& image() const { return image_; }

 private:
  // Add a bitmap.
  bool AddBitmap(float scale_factor,
                 const SkBitmap& bitmap);

  // Returns the bitmap that most closely matches |scale_factor| or nullptr if
  // one doesn't exist.
  const SkBitmap* GetBitmap(float scale_factor) const;

  // Convert |src_bitmap| to |target_bitmap| with |target_ct| and |target_at|.
  static bool ConvertBitmap(const SkBitmap& src_bitmap,
                            SkBitmap* target_bitmap,
                            SkColorType target_ct,
                            SkAlphaType target_at);

  // The |bitmap| argument will be RGBA or BGRA and either opaque or transparent
  // with post-multiplied alpha. Writes the compressed output into |compressed|.
  typedef base::Callback<bool(const SkBitmap& /*bitmap*/,
                              std::vector<unsigned char>* /*compressed*/)>
      CompressionMethod;

  // Write |bitmap| into |compressed| using |method|.
  static bool WriteCompressedFormat(const SkBitmap& bitmap,
                                    std::vector<unsigned char>* compressed,
                                    const CompressionMethod& method);

  // Write |bitmap| into |compressed| using PNG encoding.
  static bool WritePNG(const SkBitmap& bitmap,
                       std::vector<unsigned char>* compressed,
                       bool with_transparency);

  // Write |bitmap| into |compressed| using JPEG encoding. The alpha channel
  // will be ignored.
  static bool WriteJPEG(const SkBitmap& bitmap,
                        std::vector<unsigned char>* compressed,
                        int quality);

  gfx::Image image_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefImageImpl);
  DISALLOW_COPY_AND_ASSIGN(CefImageImpl);
};

#endif  // CEF_LIBCEF_BROWSER_IMAGE_IMPL_H_
