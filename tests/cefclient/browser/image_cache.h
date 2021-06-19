// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_IMAGE_CACHE_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_IMAGE_CACHE_H_
#pragma once

#include <map>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/base/cef_ref_counted.h"
#include "include/cef_image.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

namespace client {

// Simple image caching implementation.
class ImageCache
    : public base::RefCountedThreadSafe<ImageCache, CefDeleteOnUIThread> {
 public:
  ImageCache();

  // Image representation at a specific scale factor.
  struct ImageRep {
    ImageRep(const std::string& path, float scale_factor);

    // Full file system path.
    std::string path_;

    // Image scale factor (usually 1.0f or 2.0f).
    float scale_factor_;
  };
  using ImageRepSet = std::vector<ImageRep>;

  // Unique image that may have multiple representations.
  struct ImageInfo {
    ImageInfo(const std::string& id,
              const ImageRepSet& reps,
              bool internal,
              bool force_reload);

    // Helper for returning an empty image.
    static ImageInfo Empty();

    // Helpers for creating common representations.
    static ImageInfo Create1x(const std::string& id,
                              const std::string& path_1x,
                              bool internal);
    static ImageInfo Create2x(const std::string& id,
                              const std::string& path_1x,
                              const std::string& path_2x,
                              bool internal);
    static ImageInfo Create2x(const std::string& id);

    // Image unique ID.
    std::string id_;

    // Image representations to load.
    ImageRepSet reps_;

    // True if the image is internal (loaded via LoadBinaryResource).
    bool internal_;

    // True to force reload.
    bool force_reload_;
  };
  using ImageInfoSet = std::vector<ImageInfo>;

  using ImageSet = std::vector<CefRefPtr<CefImage>>;

  using LoadImagesCallback =
      base::OnceCallback<void(const ImageSet& /*images*/)>;

  // Loads the images represented by |image_info|. Executes |callback|
  // either synchronously or asychronously on the UI thread after completion.
  void LoadImages(const ImageInfoSet& image_info, LoadImagesCallback callback);

  // Returns an image that has already been cached. Must be called on the
  // UI thread.
  CefRefPtr<CefImage> GetCachedImage(const std::string& image_id);

 private:
  // Only allow deletion via scoped_refptr.
  friend struct CefDeleteOnThread<TID_UI>;
  friend class base::RefCountedThreadSafe<ImageCache, CefDeleteOnUIThread>;

  ~ImageCache();

  enum ImageType {
    TYPE_NONE,
    TYPE_PNG,
    TYPE_JPEG,
  };

  static ImageType GetImageType(const std::string& path);

  struct ImageContent;
  using ImageContentSet = std::vector<ImageContent>;

  // Load missing image contents on the FILE thread.
  void LoadMissing(const ImageInfoSet& image_info,
                   const ImageSet& images,
                   LoadImagesCallback callback);
  static bool LoadImageContents(const ImageInfo& info, ImageContent* content);
  static bool LoadImageContents(const std::string& path,
                                bool internal,
                                ImageType* type,
                                std::string* contents);

  // Create missing CefImage representations on the UI thread.
  void UpdateCache(const ImageInfoSet& image_info,
                   const ImageContentSet& contents,
                   LoadImagesCallback callback);
  static CefRefPtr<CefImage> CreateImage(const std::string& image_id,
                                         const ImageContent& content);

  // Map image ID to image representation. Only accessed on the UI thread.
  using ImageMap = std::map<std::string, CefRefPtr<CefImage>>;
  ImageMap image_map_;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_IMAGE_CACHE_H_
