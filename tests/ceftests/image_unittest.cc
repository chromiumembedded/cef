// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_image.h"
#include "tests/ceftests/image_util.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

// The expected image size in device independent pixels (DIPs).
const int kExpectedDIPSize = 16;

void LoadImage(CefRefPtr<CefImage> image,
               double scale_factor) {
  image_util::LoadIconImage(image, scale_factor);
}

void VerifyScaleEmpty(CefRefPtr<CefImage> image,
                      float scale_factor) {
  float actual_scale_factor = 0.0f;
  int pixel_width = 0;
  int pixel_height = 0;

  EXPECT_FALSE(image->HasRepresentation(scale_factor));
  EXPECT_FALSE(image->GetRepresentationInfo(scale_factor, actual_scale_factor,
                                           pixel_width, pixel_height));
  EXPECT_EQ(0.0f, actual_scale_factor);
  EXPECT_EQ(0, pixel_width);
  EXPECT_EQ(0, pixel_height);
  EXPECT_FALSE(image->RemoveRepresentation(scale_factor));
}

void VerifyScaleExists(CefRefPtr<CefImage> image,
                       float scale_factor,
                       float expected_scale_factor) {
  float actual_scale_factor = 0.0f;
  int pixel_width = 0;
  int pixel_height = 0;
  int expected_pixel_size = kExpectedDIPSize * expected_scale_factor;

  // Only returns true for exact matches.
  if (scale_factor == expected_scale_factor)
    EXPECT_TRUE(image->HasRepresentation(scale_factor));
  else
    EXPECT_FALSE(image->HasRepresentation(scale_factor));

  // Returns the closest match.
  EXPECT_TRUE(image->GetRepresentationInfo(scale_factor, actual_scale_factor,
                                           pixel_width, pixel_height));
  EXPECT_EQ(expected_scale_factor, actual_scale_factor);
  EXPECT_EQ(expected_pixel_size, pixel_width);
  EXPECT_EQ(expected_pixel_size, pixel_height);

  // Only returns true for exact matches.
  if (scale_factor == expected_scale_factor) {
    EXPECT_TRUE(image->RemoveRepresentation(scale_factor));
    EXPECT_FALSE(image->HasRepresentation(scale_factor));
  } else {
    EXPECT_FALSE(image->RemoveRepresentation(scale_factor));
  }
}

void VerifySaveAsBitmap(CefRefPtr<CefImage> image,
                        float scale_factor,
                        float expected_scale_factor) {
  int pixel_width = 0;
  int pixel_height = 0;
  int expected_pixel_size = kExpectedDIPSize * expected_scale_factor;
  size_t expected_data_size = expected_pixel_size * expected_pixel_size * 4U;

  CefRefPtr<CefBinaryValue> value = image->GetAsBitmap(
      scale_factor, CEF_COLOR_TYPE_RGBA_8888, CEF_ALPHA_TYPE_PREMULTIPLIED,
      pixel_width, pixel_height);
  EXPECT_TRUE(value.get());
  size_t data_size = value->GetSize();
  EXPECT_EQ(expected_data_size, data_size);
  EXPECT_EQ(expected_pixel_size, pixel_width);
  EXPECT_EQ(expected_pixel_size, pixel_height);

  std::vector<unsigned char> data(data_size);
  value->GetData(&data[0], data_size, 0U);

  CefRefPtr<CefImage> image2 = CefImage::CreateImage();
  EXPECT_TRUE(image2.get());
  EXPECT_TRUE(image2->AddBitmap(expected_scale_factor,
      pixel_width, pixel_height,
      CEF_COLOR_TYPE_RGBA_8888, CEF_ALPHA_TYPE_PREMULTIPLIED,
      &data[0], data_size));
  VerifyScaleExists(image2, expected_scale_factor, expected_scale_factor);
}

void VerifySaveAsPNG(CefRefPtr<CefImage> image,
                     float scale_factor,
                     float expected_scale_factor) {
  int pixel_width = 0;
  int pixel_height = 0;
  int expected_pixel_size = kExpectedDIPSize * expected_scale_factor;

  CefRefPtr<CefBinaryValue> value = image->GetAsPNG(scale_factor, true,
                                                    pixel_width, pixel_height);
  EXPECT_TRUE(value.get());
  size_t data_size = value->GetSize();
  EXPECT_GT(data_size, 0U);
  EXPECT_EQ(expected_pixel_size, pixel_width);
  EXPECT_EQ(expected_pixel_size, pixel_height);

  std::vector<unsigned char> data(data_size);
  value->GetData(&data[0], data_size, 0U);

  CefRefPtr<CefImage> image2 = CefImage::CreateImage();
  EXPECT_TRUE(image2.get());
  EXPECT_TRUE(image2->AddPNG(expected_scale_factor,
                             &data[0], data_size));
  VerifyScaleExists(image2, expected_scale_factor, expected_scale_factor);
}

void VerifySaveAsJPEG(CefRefPtr<CefImage> image,
                      float scale_factor,
                      float expected_scale_factor) {
  int pixel_width = 0;
  int pixel_height = 0;
  int expected_pixel_size = kExpectedDIPSize * expected_scale_factor;

  CefRefPtr<CefBinaryValue> value = image->GetAsJPEG(scale_factor, 80,
                                                     pixel_width, pixel_height);
  EXPECT_TRUE(value.get());
  size_t data_size = value->GetSize();
  EXPECT_GT(data_size, 0U);
  EXPECT_EQ(expected_pixel_size, pixel_width);
  EXPECT_EQ(expected_pixel_size, pixel_height);

  std::vector<unsigned char> data(data_size);
  value->GetData(&data[0], data_size, 0U);

  CefRefPtr<CefImage> image2 = CefImage::CreateImage();
  EXPECT_TRUE(image2.get());
  EXPECT_TRUE(image2->AddJPEG(expected_scale_factor,
                              &data[0], data_size));
  VerifyScaleExists(image2, expected_scale_factor, expected_scale_factor);
}

void EmptyImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  // An image is the same as itself.
  EXPECT_TRUE(image->IsSame(image));

  EXPECT_TRUE(image->IsEmpty());
  EXPECT_EQ(0U, image->GetWidth());
  EXPECT_EQ(0U, image->GetHeight());

  // Empty images are the same.
  CefRefPtr<CefImage> image2 = CefImage::CreateImage();
  EXPECT_TRUE(image->IsSame(image2));
  EXPECT_TRUE(image2->IsSame(image));

  // 1x scale does not exist.
  VerifyScaleEmpty(image, 1.0f);

  // 2x scale does not exist.
  VerifyScaleEmpty(image, 2.0f);
}

void Scale1xImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 1.0f);

  // 1x scale should exist.
  VerifyScaleExists(image, 1.0f, 1.0f);

  // 2x scale should not exist.
  VerifyScaleEmpty(image, 2.0f);
}

void Scale2xImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 2.0f);

  // 1x scale should return the 2x image.
  VerifyScaleExists(image, 1.0f, 2.0f);

  // 2x scale should exist.
  VerifyScaleExists(image, 2.0f, 2.0f);
}

void ScaleMultiImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 1.0f);
  LoadImage(image, 2.0f);

  // 1x scale should exist.
  VerifyScaleExists(image, 1.0f, 1.0f);

  // 2x scale should exist.
  VerifyScaleExists(image, 2.0f, 2.0f);
}

void SaveBitmap1xImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 1.0f);

  VerifySaveAsBitmap(image, 1.0f, 1.0f);
}

void SaveBitmap2xImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 2.0f);

  VerifySaveAsBitmap(image, 2.0f, 2.0f);
}

void SaveBitmapMultiImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 2.0f);

  VerifySaveAsBitmap(image, 1.0f, 2.0f);
}

void SavePNG1xImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 1.0f);

  VerifySaveAsPNG(image, 1.0f, 1.0f);
}

void SavePNG2xImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 2.0f);

  VerifySaveAsPNG(image, 2.0f, 2.0f);
}

void SavePNGMultiImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 2.0f);

  VerifySaveAsPNG(image, 1.0f, 2.0f);
}

void SaveJPEG1xImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 1.0f);

  VerifySaveAsJPEG(image, 1.0f, 1.0f);
}

void SaveJPEG2xImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 2.0f);

  VerifySaveAsJPEG(image, 2.0f, 2.0f);
}

void SaveJPEGMultiImpl() {
  CefRefPtr<CefImage> image = CefImage::CreateImage();
  EXPECT_TRUE(image.get());

  LoadImage(image, 2.0f);

  VerifySaveAsJPEG(image, 1.0f, 2.0f);
}

}  // namespace

#define IMAGE_TEST(name) UI_THREAD_TEST(ImageTest, name)

IMAGE_TEST(Empty);
IMAGE_TEST(Scale1x);
IMAGE_TEST(Scale2x);
IMAGE_TEST(ScaleMulti);
IMAGE_TEST(SaveBitmap1x);
IMAGE_TEST(SaveBitmap2x);
IMAGE_TEST(SaveBitmapMulti);
IMAGE_TEST(SavePNG1x);
IMAGE_TEST(SavePNG2x);
IMAGE_TEST(SavePNGMulti);
IMAGE_TEST(SaveJPEG1x);
IMAGE_TEST(SaveJPEG2x);
IMAGE_TEST(SaveJPEGMulti);
