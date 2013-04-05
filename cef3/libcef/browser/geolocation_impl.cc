// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_geolocation.h"
#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/time_util.h"
#include "base/logging.h"
#include "content/browser/geolocation/geolocation_provider.h"
#include "content/public/common/geoposition.h"

namespace {

void SetPosition(const content::Geoposition& source, CefGeoposition& target) {
  target.latitude = source.latitude;
  target.longitude = source.longitude;
  target.altitude = source.altitude;
  target.accuracy = source.accuracy;
  target.altitude_accuracy = source.altitude_accuracy;
  target.heading = source.heading;
  target.speed = source.speed;
  cef_time_from_basetime(source.timestamp, target.timestamp);

  switch (source.error_code) {
    case content::Geoposition::ERROR_CODE_NONE:
      target.error_code = GEOPOSITON_ERROR_NONE;
      break;
    case content::Geoposition::ERROR_CODE_PERMISSION_DENIED:
      target.error_code = GEOPOSITON_ERROR_PERMISSION_DENIED;
      break;
    case content::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE:
      target.error_code = GEOPOSITON_ERROR_POSITION_UNAVAILABLE;
      break;
    case content::Geoposition::ERROR_CODE_TIMEOUT:
      target.error_code = GEOPOSITON_ERROR_TIMEOUT;
      break;
  }

  CefString(&target.error_message) = source.error_message;
}

void LocationCallback(CefRefPtr<CefGetGeolocationCallback> callback,
                      const content::Geoposition& position) {
  if (CEF_CURRENTLY_ON_UIT()) {
    CefGeoposition cef_position;
    SetPosition(position, cef_position);
    callback->OnLocationUpdate(cef_position);
  } else {
    CEF_POST_TASK(CEF_UIT, base::Bind(LocationCallback, callback, position));
  }
}

}  // namespace

bool CefGetGeolocation(CefRefPtr<CefGetGeolocationCallback> callback) {
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  if (!callback.get()) {
    NOTREACHED() << "invalid parameter";
    return false;
  }

  if (CEF_CURRENTLY_ON_IOT()) {
    content::GeolocationProvider* provider =
        content::GeolocationProvider::GetInstance();
    if (provider) {
      provider->RequestCallback(base::Bind(LocationCallback, callback));
      return true;
    }
    return false;
  } else {
    CEF_POST_TASK(CEF_IOT,
        base::Bind(base::IgnoreResult(CefGetGeolocation), callback));
    return true;
  }
}
