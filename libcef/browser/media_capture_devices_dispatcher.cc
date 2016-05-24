// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/media_capture_devices_dispatcher.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_capture_devices.h"

using content::BrowserThread;
using content::MediaStreamDevices;

namespace {

const content::MediaStreamDevice* FindDefaultDeviceWithId(
    const content::MediaStreamDevices& devices,
    const std::string& device_id) {
  if (devices.empty())
    return NULL;

  content::MediaStreamDevices::const_iterator iter = devices.begin();
  for (; iter != devices.end(); ++iter) {
    if (iter->id == device_id) {
      return &(*iter);
    }
  }

  return &(*devices.begin());
};

}  // namespace


CefMediaCaptureDevicesDispatcher*
    CefMediaCaptureDevicesDispatcher::GetInstance() {
  return base::Singleton<CefMediaCaptureDevicesDispatcher>::get();
}

CefMediaCaptureDevicesDispatcher::CefMediaCaptureDevicesDispatcher() {}

CefMediaCaptureDevicesDispatcher::~CefMediaCaptureDevicesDispatcher() {}

void CefMediaCaptureDevicesDispatcher::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDefaultAudioCaptureDevice,
                               std::string());
  registry->RegisterStringPref(prefs::kDefaultVideoCaptureDevice,
                               std::string());
}

void CefMediaCaptureDevicesDispatcher::GetDefaultDevices(
    PrefService* prefs,
    bool audio,
    bool video,
    content::MediaStreamDevices* devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(audio || video);

  std::string default_device;
  if (audio) {
    default_device = prefs->GetString(prefs::kDefaultAudioCaptureDevice);
    GetRequestedDevice(default_device, true, false, devices);
  }

  if (video) {
    default_device = prefs->GetString(prefs::kDefaultVideoCaptureDevice);
    GetRequestedDevice(default_device, false, true, devices);
  }
}

void CefMediaCaptureDevicesDispatcher::GetRequestedDevice(
    const std::string& requested_device_id,
    bool audio,
    bool video,
    content::MediaStreamDevices* devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(audio || video);

  if (audio) {
    const content::MediaStreamDevices& audio_devices = GetAudioCaptureDevices();
    const content::MediaStreamDevice* const device =
        FindDefaultDeviceWithId(audio_devices, requested_device_id);
    if (device)
      devices->push_back(*device);
  }
  if (video) {
    const content::MediaStreamDevices& video_devices = GetVideoCaptureDevices();
    const content::MediaStreamDevice* const device =
        FindDefaultDeviceWithId(video_devices, requested_device_id);
    if (device)
      devices->push_back(*device);
  }
}

void CefMediaCaptureDevicesDispatcher::OnAudioCaptureDevicesChanged() {
}

void CefMediaCaptureDevicesDispatcher::OnVideoCaptureDevicesChanged() {
}

void CefMediaCaptureDevicesDispatcher::OnMediaRequestStateChanged(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const GURL& security_origin,
    content::MediaStreamType stream_type,
    content::MediaRequestState state) {
}

void CefMediaCaptureDevicesDispatcher::OnCreatingAudioStream(
    int render_process_id,
    int render_view_id) {
}

void CefMediaCaptureDevicesDispatcher::OnSetCapturingLinkSecured(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    content::MediaStreamType stream_type,
    bool is_secure) {
}

const MediaStreamDevices&
CefMediaCaptureDevicesDispatcher::GetAudioCaptureDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return content::MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices();
}

const MediaStreamDevices&
CefMediaCaptureDevicesDispatcher::GetVideoCaptureDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return content::MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices();
}
