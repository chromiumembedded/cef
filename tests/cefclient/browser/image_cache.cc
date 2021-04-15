// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/image_cache.h"

#include "tests/shared/browser/file_util.h"
#include "tests/shared/browser/resource_util.h"

namespace client {

namespace {

const char kEmptyId[] = "__empty";

}  // namespace

ImageCache::ImageCache() {}

ImageCache::~ImageCache() {
  CEF_REQUIRE_UI_THREAD();
}

ImageCache::ImageRep::ImageRep(const std::string& path, float scale_factor)
    : path_(path), scale_factor_(scale_factor) {
  DCHECK(!path_.empty());
  DCHECK_GT(scale_factor_, 0.0f);
}

ImageCache::ImageInfo::ImageInfo(const std::string& id,
                                 const ImageRepSet& reps,
                                 bool internal,
                                 bool force_reload)
    : id_(id), reps_(reps), internal_(internal), force_reload_(force_reload) {
#ifndef NDEBUG
  DCHECK(!id_.empty());
  if (id_ != kEmptyId)
    DCHECK(!reps_.empty());
#endif
}

// static
ImageCache::ImageInfo ImageCache::ImageInfo::Empty() {
  return ImageInfo(kEmptyId, ImageRepSet(), true, false);
}

// static
ImageCache::ImageInfo ImageCache::ImageInfo::Create1x(
    const std::string& id,
    const std::string& path_1x,
    bool internal) {
  ImageRepSet reps;
  reps.push_back(ImageRep(path_1x, 1.0f));
  return ImageInfo(id, reps, internal, false);
}

// static
ImageCache::ImageInfo ImageCache::ImageInfo::Create2x(
    const std::string& id,
    const std::string& path_1x,
    const std::string& path_2x,
    bool internal) {
  ImageRepSet reps;
  reps.push_back(ImageRep(path_1x, 1.0f));
  reps.push_back(ImageRep(path_2x, 2.0f));
  return ImageInfo(id, reps, internal, false);
}

// static
ImageCache::ImageInfo ImageCache::ImageInfo::Create2x(const std::string& id) {
  return Create2x(id, id + ".1x.png", id + ".2x.png", true);
}

struct ImageCache::ImageContent {
  ImageContent() {}

  struct RepContent {
    RepContent(ImageType type, float scale_factor, const std::string& contents)
        : type_(type), scale_factor_(scale_factor), contents_(contents) {}

    ImageType type_;
    float scale_factor_;
    std::string contents_;
  };
  typedef std::vector<RepContent> RepContentSet;
  RepContentSet contents_;

  CefRefPtr<CefImage> image_;
};

void ImageCache::LoadImages(const ImageInfoSet& image_info,
                            const LoadImagesCallback& callback) {
  DCHECK(!image_info.empty());
  DCHECK(!callback.is_null());

  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::Bind(&ImageCache::LoadImages, this, image_info,
                                   callback));
    return;
  }

  ImageSet images;
  bool missing_images = false;

  ImageInfoSet::const_iterator it = image_info.begin();
  for (; it != image_info.end(); ++it) {
    const ImageInfo& info = *it;

    if (info.id_ == kEmptyId) {
      // Image intentionally left empty.
      images.push_back(nullptr);
      continue;
    }

    ImageMap::iterator it2 = image_map_.find(info.id_);
    if (it2 != image_map_.end()) {
      if (!info.force_reload_) {
        // Image already exists.
        images.push_back(it2->second);
        continue;
      }

      // Remove the existing image from the map.
      image_map_.erase(it2);
    }

    // Load the image.
    images.push_back(nullptr);
    if (!missing_images)
      missing_images = true;
  }

  if (missing_images) {
    CefPostTask(TID_FILE_USER_BLOCKING,
                base::Bind(&ImageCache::LoadMissing, this, image_info, images,
                           callback));
  } else {
    callback.Run(images);
  }
}

CefRefPtr<CefImage> ImageCache::GetCachedImage(const std::string& image_id) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(!image_id.empty());

  ImageMap::const_iterator it = image_map_.find(image_id);
  if (it != image_map_.end())
    return it->second;

  return nullptr;
}

// static
ImageCache::ImageType ImageCache::GetImageType(const std::string& path) {
  std::string ext = file_util::GetFileExtension(path);
  if (ext.empty())
    return TYPE_NONE;

  std::transform(ext.begin(), ext.end(), ext.begin(), tolower);
  if (ext == "png")
    return TYPE_PNG;
  if (ext == "jpg" || ext == "jpeg")
    return TYPE_JPEG;

  return TYPE_NONE;
}

void ImageCache::LoadMissing(const ImageInfoSet& image_info,
                             const ImageSet& images,
                             const LoadImagesCallback& callback) {
  DCHECK(!CefCurrentlyOn(TID_UI) && !CefCurrentlyOn(TID_IO));

  DCHECK_EQ(image_info.size(), images.size());

  ImageContentSet contents;

  ImageInfoSet::const_iterator it1 = image_info.begin();
  ImageSet::const_iterator it2 = images.begin();
  for (; it1 != image_info.end() && it2 != images.end(); ++it1, ++it2) {
    const ImageInfo& info = *it1;
    ImageContent content;
    if (*it2 || info.id_ == kEmptyId) {
      // Image already exists or is intentionally empty.
      content.image_ = *it2;
    } else {
      LoadImageContents(info, &content);
    }
    contents.push_back(content);
  }

  CefPostTask(TID_UI, base::Bind(&ImageCache::UpdateCache, this, image_info,
                                 contents, callback));
}

// static
bool ImageCache::LoadImageContents(const ImageInfo& info,
                                   ImageContent* content) {
  DCHECK(!CefCurrentlyOn(TID_UI) && !CefCurrentlyOn(TID_IO));

  ImageRepSet::const_iterator it = info.reps_.begin();
  for (; it != info.reps_.end(); ++it) {
    const ImageRep& rep = *it;
    ImageType rep_type;
    std::string rep_contents;
    if (!LoadImageContents(rep.path_, info.internal_, &rep_type,
                           &rep_contents)) {
      LOG(ERROR) << "Failed to load image " << info.id_ << " from path "
                 << rep.path_;
      return false;
    }
    content->contents_.push_back(
        ImageContent::RepContent(rep_type, rep.scale_factor_, rep_contents));
  }

  return true;
}

// static
bool ImageCache::LoadImageContents(const std::string& path,
                                   bool internal,
                                   ImageType* type,
                                   std::string* contents) {
  DCHECK(!CefCurrentlyOn(TID_UI) && !CefCurrentlyOn(TID_IO));

  *type = GetImageType(path);
  if (*type == TYPE_NONE)
    return false;

  if (internal) {
    if (!LoadBinaryResource(path.c_str(), *contents))
      return false;
  } else if (!file_util::ReadFileToString(path, contents)) {
    return false;
  }

  return !contents->empty();
}

void ImageCache::UpdateCache(const ImageInfoSet& image_info,
                             const ImageContentSet& contents,
                             const LoadImagesCallback& callback) {
  CEF_REQUIRE_UI_THREAD();

  DCHECK_EQ(image_info.size(), contents.size());

  ImageSet images;

  ImageInfoSet::const_iterator it1 = image_info.begin();
  ImageContentSet::const_iterator it2 = contents.begin();
  for (; it1 != image_info.end() && it2 != contents.end(); ++it1, ++it2) {
    const ImageInfo& info = *it1;
    const ImageContent& content = *it2;
    if (content.image_ || info.id_ == kEmptyId) {
      // Image already exists or is intentionally empty.
      images.push_back(content.image_);
    } else {
      CefRefPtr<CefImage> image = CreateImage(info.id_, content);
      images.push_back(image);

      // Add the image to the map.
      image_map_.insert(std::make_pair(info.id_, image));
    }
  }

  callback.Run(images);
}

// static
CefRefPtr<CefImage> ImageCache::CreateImage(const std::string& image_id,
                                            const ImageContent& content) {
  CEF_REQUIRE_UI_THREAD();

  // Shouldn't be creating an image if one already exists.
  DCHECK(!content.image_);

  if (content.contents_.empty())
    return nullptr;

  CefRefPtr<CefImage> image = CefImage::CreateImage();

  ImageContent::RepContentSet::const_iterator it = content.contents_.begin();
  for (; it != content.contents_.end(); ++it) {
    const ImageContent::RepContent& rep = *it;
    if (rep.type_ == TYPE_PNG) {
      if (!image->AddPNG(rep.scale_factor_, rep.contents_.c_str(),
                         rep.contents_.size())) {
        LOG(ERROR) << "Failed to create image " << image_id << " for PNG@"
                   << rep.scale_factor_;
        return nullptr;
      }
    } else if (rep.type_ == TYPE_JPEG) {
      if (!image->AddJPEG(rep.scale_factor_, rep.contents_.c_str(),
                          rep.contents_.size())) {
        LOG(ERROR) << "Failed to create image " << image_id << " for JPG@"
                   << rep.scale_factor_;
        return nullptr;
      }
    } else {
      NOTREACHED();
      return nullptr;
    }
  }

  return image;
}

}  // namespace client
