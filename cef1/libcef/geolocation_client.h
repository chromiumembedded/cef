// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_GEOLOCATION_CLIENT_H_
#define CEF_LIBCEF_GEOLOCATION_CLIENT_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/geolocation/geolocation_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationController.h"

class CefBrowserImpl;
class CefGeolocationCallbackImpl;

namespace content {
class GeolocationProvider;
struct Geoposition;
}

namespace WebKit {
class WebGeolocationController;
class WebGeolocationPermissionRequest;
class WebGeolocationPermissionRequestManager;
class WebGeolocationPosition;
}

// Delegate for Geolocation messages used by WebKit.
class CefGeolocationClient
    : public WebKit::WebGeolocationClient,
      public content::GeolocationObserver,
      public base::RefCountedThreadSafe<CefGeolocationClient> {
 public:
  explicit CefGeolocationClient(CefBrowserImpl* browser);
  virtual ~CefGeolocationClient();

 private:
  friend class CefGeolocationCallbackImpl;

  // WebGeolocationClient methods.
  virtual void geolocationDestroyed() OVERRIDE;
  virtual void startUpdating() OVERRIDE;
  virtual void stopUpdating() OVERRIDE;
  virtual void setEnableHighAccuracy(bool enable_high_accuracy) OVERRIDE;
  virtual void setController(
      WebKit::WebGeolocationController* controller) OVERRIDE;
  virtual bool lastPosition(WebKit::WebGeolocationPosition& position) OVERRIDE;
  virtual void requestPermission(
      const WebKit::WebGeolocationPermissionRequest& permissionRequest)
      OVERRIDE;
  virtual void cancelPermissionRequest(
      const WebKit::WebGeolocationPermissionRequest& permissionRequest)
      OVERRIDE;

  // Called to continue processing on the IO thread.
  void OnStartUpdating(bool enable_high_accuracy);
  void OnStopUpdating();

  // GeolocationObserver methods.
  virtual void OnLocationUpdate(const content::Geoposition& position) OVERRIDE;

  // Permission for using geolocation has been set.
  void OnPermissionSet(int bridge_id, bool is_allowed);

  // We have an updated geolocation position or error code.
  void OnPositionUpdated(const content::Geoposition& geoposition);

  // The following members are only accessed on the UI thread.

  // The |browser_| pointer is guaranteed to outlive this object.
  CefBrowserImpl* browser_;

  // The controller_ is valid for the lifetime of the underlying
  // WebCore::GeolocationController. geolocationDestroyed() is
  // invoked when the underlying object is destroyed.
  scoped_ptr<WebKit::WebGeolocationController> controller_;

  scoped_ptr<WebKit::WebGeolocationPermissionRequestManager>
      pending_permissions_;
  bool enable_high_accuracy_;
  bool updating_;

  // The following members are only accessed on the IO thread.

  // Only set whilst we are registered with the arbitrator.
  content::GeolocationProvider* location_provider_;
  
  DISALLOW_COPY_AND_ASSIGN(CefGeolocationClient);
};

#endif  // CEF_LIBCEF_GEOLOCATION_CLIENT_H_
