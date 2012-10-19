// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/geolocation_client.h"
#include "libcef/browser_impl.h"
#include "libcef/cef_context.h"
#include "libcef/cef_thread.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/geolocation/arbitrator_dependency_factory.h"
#include "content/browser/geolocation/geolocation_observer.h"
#include "content/browser/geolocation/geolocation_provider.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/common/geoposition.h"
#include "content/public/common/content_switches.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationPermissionRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationPermissionRequestManager.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationPosition.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

using WebKit::WebGeolocationController;
using WebKit::WebGeolocationError;
using WebKit::WebGeolocationPermissionRequest;
using WebKit::WebGeolocationPermissionRequestManager;
using WebKit::WebGeolocationPosition;

namespace {

class CefURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  CefURLRequestContextGetter() {}

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    return _Context->request_context();
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE {
    return CefThread::GetMessageLoopProxyForThread(CefThread::IO);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CefURLRequestContextGetter);
};

// In-memory store for access tokens used by geolocation.
class CefAccessTokenStore : public content::AccessTokenStore {
 public:
  CefAccessTokenStore() {}

  virtual void LoadAccessTokens(
      const LoadAccessTokensCallbackType& callback) OVERRIDE {
    if (!request_context_getter_)
      request_context_getter_ = new CefURLRequestContextGetter;
    callback.Run(access_token_set_, request_context_getter_);
  }

  virtual void SaveAccessToken(
      const GURL& server_url, const string16& access_token) OVERRIDE {
    access_token_set_[server_url] = access_token;
  }

 private:
  AccessTokenSet access_token_set_;
  scoped_refptr<CefURLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(CefAccessTokenStore);
};

void NotifyArbitratorPermissionGranted() {
  DCHECK(CefThread::CurrentlyOn(CefThread::IO));
  GeolocationProvider::GetInstance()->OnPermissionGranted();
}

}  // namespace


// CefGeolocationCallback implementation.
class CefGeolocationCallbackImpl : public CefGeolocationCallback {
 public:
  CefGeolocationCallbackImpl(CefGeolocationClient* client, int bridge_id)
      : client_(client),
        bridge_id_(bridge_id) {}

  virtual void Continue(bool allow) OVERRIDE {
    if (CefThread::CurrentlyOn(CefThread::UI)) {
      if (client_) {
        client_->OnPermissionSet(bridge_id_, allow);
        client_ = NULL;
      }
    } else {
      CefThread::PostTask(CefThread::UI, FROM_HERE,
          base::Bind(&CefGeolocationCallbackImpl::Continue, this, allow));
    }
  }

 private:
  scoped_refptr<CefGeolocationClient> client_;
  int bridge_id_;

  IMPLEMENT_REFCOUNTING(CefGeolocationCallbackImpl);
};


CefGeolocationClient::CefGeolocationClient(CefBrowserImpl* browser)
    : browser_(browser),
      pending_permissions_(new WebGeolocationPermissionRequestManager()),
      enable_high_accuracy_(false),
      updating_(false),
      location_provider_(NULL) {
}

CefGeolocationClient::~CefGeolocationClient() {}

void CefGeolocationClient::geolocationDestroyed() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  controller_.reset();
  DCHECK(!updating_);
}

void CefGeolocationClient::startUpdating() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  CefThread::PostTask(CefThread::IO, FROM_HERE,
      base::Bind(&CefGeolocationClient::OnStartUpdating, this,
                 enable_high_accuracy_));
  updating_ = true;
}

void CefGeolocationClient::stopUpdating() {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  CefThread::PostTask(CefThread::IO, FROM_HERE,
      base::Bind(&CefGeolocationClient::OnStopUpdating, this));
  updating_ = false;
}

void CefGeolocationClient::setEnableHighAccuracy(bool enable_high_accuracy) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  // GeolocationController calls setEnableHighAccuracy(true) before
  // startUpdating in response to the first high-accuracy Geolocation
  // subscription. When the last high-accuracy Geolocation unsubscribes
  // it calls setEnableHighAccuracy(false) after stopUpdating.
  bool has_changed = enable_high_accuracy_ != enable_high_accuracy;
  enable_high_accuracy_ = enable_high_accuracy;
  // We have a different accuracy requirement. Request browser to update.
  if (updating_ && has_changed)
    startUpdating();
}

void CefGeolocationClient::setController(
    WebGeolocationController* controller) {
  controller_.reset(controller);
}

bool CefGeolocationClient::lastPosition(WebGeolocationPosition&) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  // The latest position is stored in the browser, not the renderer, so we
  // would have to fetch it synchronously to give a good value here.  The
  // WebCore::GeolocationController already caches the last position it
  // receives, so there is not much benefit to more position caching here.
  return false;
}

void CefGeolocationClient::requestPermission(
    const WebGeolocationPermissionRequest& permissionRequest) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  int bridge_id = pending_permissions_->add(permissionRequest);
  GURL origin = GURL(permissionRequest.securityOrigin().toString());

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefGeolocationHandler> handler =
        client->GetGeolocationHandler();
    if (handler.get()) {
      CefRefPtr<CefGeolocationCallbackImpl> callbackPtr(
          new CefGeolocationCallbackImpl(this, bridge_id));

      handler->OnRequestGeolocationPermission(browser_, origin.spec(),
                                              bridge_id, callbackPtr.get());
      return;
    }
  }

  // Disallow geolocation access by default.
  OnPermissionSet(bridge_id, false);
}

void CefGeolocationClient::cancelPermissionRequest(
    const WebGeolocationPermissionRequest& permissionRequest) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  int bridge_id;
  if (!pending_permissions_->remove(permissionRequest, bridge_id))
    return;
  GURL origin = GURL(permissionRequest.securityOrigin().toString());

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefGeolocationHandler> handler =
        client->GetGeolocationHandler();
    if (handler.get()) {
      handler->OnCancelGeolocationPermission(browser_, origin.spec(),
                                             bridge_id);
    }
  }
}

void CefGeolocationClient::OnStartUpdating(bool enable_high_accuracy) {
  DCHECK(CefThread::CurrentlyOn(CefThread::IO));

  if (!location_provider_)
    location_provider_ = GeolocationProvider::GetInstance();

  // Re-add to re-establish our options, in case they changed.
  location_provider_->AddObserver(
      this, GeolocationObserverOptions(enable_high_accuracy));
}

void CefGeolocationClient::OnStopUpdating() {
  DCHECK(CefThread::CurrentlyOn(CefThread::IO));

  if (location_provider_) {
    location_provider_->RemoveObserver(this);
    location_provider_ = NULL;
  }
}

void CefGeolocationClient::OnLocationUpdate(
    const content::Geoposition& position) {
  DCHECK(CefThread::CurrentlyOn(CefThread::IO));

  CefThread::PostTask(CefThread::UI, FROM_HERE,
      base::Bind(&CefGeolocationClient::OnPositionUpdated, this, position));
}

// Permission for using geolocation has been set.
void CefGeolocationClient::OnPermissionSet(int bridge_id, bool is_allowed) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  WebGeolocationPermissionRequest permissionRequest;
  if (!pending_permissions_->remove(bridge_id, permissionRequest))
    return;
  permissionRequest.setIsAllowed(is_allowed);

  if (is_allowed) {
    CefThread::PostTask(
        CefThread::IO, FROM_HERE,
        base::Bind(&NotifyArbitratorPermissionGranted));
  }
}

// We have an updated geolocation position or error code.
void CefGeolocationClient::OnPositionUpdated(
    const content::Geoposition& geoposition) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  // It is possible for the browser process to have queued an update message
  // before receiving the stop updating message.
  if (!updating_)
    return;

  if (geoposition.Validate()) {
    controller_->positionChanged(
        WebGeolocationPosition(
            geoposition.timestamp.ToDoubleT(),
            geoposition.latitude, geoposition.longitude,
            geoposition.accuracy,
            // Lowest point on land is at approximately -400 meters.
            geoposition.altitude > -10000.,
            geoposition.altitude,
            geoposition.altitude_accuracy >= 0.,
            geoposition.altitude_accuracy,
            geoposition.heading >= 0. && geoposition.heading <= 360.,
            geoposition.heading,
            geoposition.speed >= 0.,
            geoposition.speed));
  } else {
    WebGeolocationError::Error code;
    switch (geoposition.error_code) {
      case content::Geoposition::ERROR_CODE_PERMISSION_DENIED:
        code = WebGeolocationError::ErrorPermissionDenied;
        break;
      case content::Geoposition::ERROR_CODE_POSITION_UNAVAILABLE:
        code = WebGeolocationError::ErrorPositionUnavailable;
        break;
      default:
        NOTREACHED() << geoposition.error_code;
        return;
    }
    controller_->errorOccurred(
        WebGeolocationError(
            code, WebKit::WebString::fromUTF8(geoposition.error_message)));
  }
}


// Replacement for content/browser/geolocation/arbitrator_dependency_factory.cc

// GeolocationArbitratorDependencyFactory
GeolocationArbitratorDependencyFactory::
~GeolocationArbitratorDependencyFactory() {
}

// DefaultGeolocationArbitratorDependencyFactory
DefaultGeolocationArbitratorDependencyFactory::
~DefaultGeolocationArbitratorDependencyFactory() {
}

DefaultGeolocationArbitratorDependencyFactory::GetTimeNow
DefaultGeolocationArbitratorDependencyFactory::GetTimeFunction() {
  return base::Time::Now;
}

content::AccessTokenStore*
DefaultGeolocationArbitratorDependencyFactory::NewAccessTokenStore() {
  return new CefAccessTokenStore;
}

LocationProviderBase*
DefaultGeolocationArbitratorDependencyFactory::NewNetworkLocationProvider(
    content::AccessTokenStore* access_token_store,
    net::URLRequestContextGetter* context,
    const GURL& url,
    const string16& access_token) {
  return ::NewNetworkLocationProvider(access_token_store, context,
                                      url, access_token);
}

LocationProviderBase*
DefaultGeolocationArbitratorDependencyFactory::NewSystemLocationProvider() {
  return ::NewSystemLocationProvider();
}


// Extract from content/public/common/content_switches.cc.

namespace switches {

const char kExperimentalLocationFeatures[]  = "experimental-location-features";

}  // namespace switches
