// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_
#define CEF_LIBCEF_BROWSER_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "content/public/browser/media_observer.h"
#include "content/public/common/media_stream_request.h"

class PrefRegistrySimple;
class PrefService;

// This singleton is used to receive updates about media events from the content
// layer. Based on chrome/browser/media/media_capture_devices_dispatcher.[h|cc].
class CefMediaCaptureDevicesDispatcher : public content::MediaObserver {
 public:
  static CefMediaCaptureDevicesDispatcher* GetInstance();

  // Registers the preferences related to Media Stream default devices.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Helper to get the default devices which can be used by the media request,
  // if the return list is empty, it means there is no available device on the
  // OS. Called on the UI thread.
  void GetDefaultDevices(PrefService* prefs,
                         bool audio,
                         bool video,
                         content::MediaStreamDevices* devices);

  // Helper for picking the device that was requested for an OpenDevice request.
  // If the device requested is not available it will revert to using the first
  // available one instead or will return an empty list if no devices of the
  // requested kind are present. Called on the UI thread.
  void GetRequestedDevice(const std::string& requested_device_id,
                          bool audio,
                          bool video,
                          content::MediaStreamDevices* devices);

  // Overridden from content::MediaObserver:
  void OnAudioCaptureDevicesChanged() override;
  void OnVideoCaptureDevicesChanged() override;
  void OnMediaRequestStateChanged(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      const GURL& security_origin,
      content::MediaStreamType stream_type,
      content::MediaRequestState state) override;
   void OnCreatingAudioStream(int render_process_id,
                              int render_view_id) override;
   void OnSetCapturingLinkSecured(int render_process_id,
                                  int render_frame_id,
                                  int page_request_id,
                                  content::MediaStreamType stream_type,
                                  bool is_secure) override;

 private:
  friend struct base::DefaultSingletonTraits<CefMediaCaptureDevicesDispatcher>;

  CefMediaCaptureDevicesDispatcher();
  ~CefMediaCaptureDevicesDispatcher() override;

  const content::MediaStreamDevices& GetAudioCaptureDevices();
  const content::MediaStreamDevices& GetVideoCaptureDevices();
};

#endif  // CEF_LIBCEF_BROWSER_MEDIA_CAPTURE_DEVICES_DISPATCHER_H_
