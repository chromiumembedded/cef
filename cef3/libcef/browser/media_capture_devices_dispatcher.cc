// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/media_capture_devices_dispatcher.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_devices_monitor.h"
#include "content/public/common/media_stream_request.h"

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
  return Singleton<CefMediaCaptureDevicesDispatcher>::get();
}

CefMediaCaptureDevicesDispatcher::CefMediaCaptureDevicesDispatcher()
    : devices_enumerated_(false) {}

CefMediaCaptureDevicesDispatcher::~CefMediaCaptureDevicesDispatcher() {}

void CefMediaCaptureDevicesDispatcher::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDefaultAudioCaptureDevice,
                               std::string());
  registry->RegisterStringPref(prefs::kDefaultVideoCaptureDevice,
                               std::string());
}

void CefMediaCaptureDevicesDispatcher::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void CefMediaCaptureDevicesDispatcher::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

const MediaStreamDevices&
CefMediaCaptureDevicesDispatcher::GetAudioCaptureDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!devices_enumerated_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&content::EnsureMonitorCaptureDevices));
    devices_enumerated_ = true;
  }
  return audio_devices_;
}

const MediaStreamDevices&
CefMediaCaptureDevicesDispatcher::GetVideoCaptureDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!devices_enumerated_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&content::EnsureMonitorCaptureDevices));
    devices_enumerated_ = true;
  }
  return video_devices_;
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

void CefMediaCaptureDevicesDispatcher::OnCaptureDevicesOpened(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices,
    const base::Closure& close_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void CefMediaCaptureDevicesDispatcher::OnCaptureDevicesClosed(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void CefMediaCaptureDevicesDispatcher::OnAudioCaptureDevicesChanged(
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &CefMediaCaptureDevicesDispatcher::UpdateAudioDevicesOnUIThread,
          base::Unretained(this), devices));
}

void CefMediaCaptureDevicesDispatcher::OnVideoCaptureDevicesChanged(
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &CefMediaCaptureDevicesDispatcher::UpdateVideoDevicesOnUIThread,
          base::Unretained(this), devices));
}

void CefMediaCaptureDevicesDispatcher::OnMediaRequestStateChanged(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevice& device,
    content::MediaRequestState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &CefMediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread,
          base::Unretained(this), render_process_id, render_view_id, device,
          state));

}

void CefMediaCaptureDevicesDispatcher::OnAudioStreamPlayingChanged(
    int render_process_id, int render_view_id, int stream_id, bool playing) {
}

void CefMediaCaptureDevicesDispatcher::UpdateAudioDevicesOnUIThread(
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  devices_enumerated_ = true;
  audio_devices_ = devices;
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnUpdateAudioDevices(audio_devices_));
}

void CefMediaCaptureDevicesDispatcher::UpdateVideoDevicesOnUIThread(
    const content::MediaStreamDevices& devices){
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  devices_enumerated_ = true;
  video_devices_ = devices;
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnUpdateVideoDevices(video_devices_));
}

void CefMediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevice& device,
    content::MediaRequestState state) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnRequestUpdate(render_process_id,
                                    render_view_id,
                                    device,
                                    state));
}
