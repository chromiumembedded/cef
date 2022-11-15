// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MEDIA_STREAM_REGISTRAR_H_
#define CEF_LIBCEF_BROWSER_MEDIA_STREAM_REGISTRAR_H_
#pragma once

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/media_stream_request.h"

class CefBrowserHostBase;
class CefMediaStreamUI;

class CefMediaStreamRegistrar {
 public:
  explicit CefMediaStreamRegistrar(CefBrowserHostBase* browser);

  CefMediaStreamRegistrar(const CefMediaStreamRegistrar&) = delete;
  CefMediaStreamRegistrar& operator=(const CefMediaStreamRegistrar&) = delete;

  std::unique_ptr<content::MediaStreamUI> MaybeCreateMediaStreamUI(
      bool has_video,
      bool has_audio);

 private:
  friend class CefMediaStreamUI;

  // Called from CefMediaStreamUI.
  void RegisterMediaStream(const std::string& label, bool video, bool audio);
  void UnregisterMediaStream(const std::string& label);

  void NotifyMediaStreamChange();

  // Guaranteed to outlive this object.
  CefBrowserHostBase* const browser_;

  struct MediaStreamInfo {
    bool video;
    bool audio;
  };

  // Current in use media streams.
  std::map<std::string, MediaStreamInfo> registered_streams_;

  // Last notified media stream info.
  MediaStreamInfo last_notified_info_{};

  base::WeakPtrFactory<CefMediaStreamRegistrar> weak_ptr_factory_{this};
};

#endif  // CEF_LIBCEF_BROWSER_MEDIA_STREAM_REGISTRAR_H_
