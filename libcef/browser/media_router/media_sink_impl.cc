// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/media_router/media_sink_impl.h"

#include "libcef/browser/thread_util.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"

namespace {

using SinkServiceVector = std::vector<media_router::MediaSinkServiceBase*>;

SinkServiceVector GetSinkServices() {
  CEF_REQUIRE_UIT();
  auto sink_service = media_router::DualMediaSinkService::GetInstance();
  return {sink_service->GetCastMediaSinkServiceImpl(),
          sink_service->GetDialMediaSinkServiceImpl()};
}

void GetSinkInternalAndContinue(
    SinkServiceVector services,
    const media_router::MediaSink::Id& sink_id,
    CefRefPtr<CefMediaSinkDeviceInfoCallback> callback) {
  CEF_REQUIRE_IOT();

  CefMediaSinkDeviceInfo device_info;
  const media_router::MediaSinkInternal* sink_internal = nullptr;

  for (auto service : services) {
    sink_internal = service->GetSinkById(sink_id);
    if (sink_internal) {
      break;
    }
  }

  if (sink_internal) {
    if (sink_internal->is_cast_sink()) {
      const auto& cast_data = sink_internal->cast_data();
      CefString(&device_info.ip_address) =
          cast_data.ip_endpoint.ToStringWithoutPort();
      device_info.port = cast_data.ip_endpoint.port();
      CefString(&device_info.model_name) = cast_data.model_name;
    } else if (sink_internal->is_dial_sink()) {
      const auto& dial_data = sink_internal->dial_data();
      CefString(&device_info.ip_address) = dial_data.ip_address.ToString();
      if (dial_data.app_url.is_valid() && dial_data.app_url.has_port()) {
        base::StringToInt(dial_data.app_url.port_piece(), &device_info.port);
      }
      CefString(&device_info.model_name) = dial_data.model_name;
    }
  }

  // Execute the callback on the UI thread.
  CEF_POST_TASK(
      CEF_UIT,
      base::BindOnce(&CefMediaSinkDeviceInfoCallback::OnMediaSinkDeviceInfo,
                     callback, device_info));
}

void GetDeviceInfo(const media_router::MediaSink::Id& sink_id,
                   CefRefPtr<CefMediaSinkDeviceInfoCallback> callback) {
  auto next_step = base::BindOnce(
      [](const media_router::MediaSink::Id& sink_id,
         CefRefPtr<CefMediaSinkDeviceInfoCallback> callback) {
        CEF_POST_TASK(CEF_IOT,
                      base::BindOnce(GetSinkInternalAndContinue,
                                     GetSinkServices(), sink_id, callback));
      },
      sink_id, callback);

  if (CEF_CURRENTLY_ON(CEF_UIT)) {
    std::move(next_step).Run();
  } else {
    CEF_POST_TASK(CEF_UIT, std::move(next_step));
  }
}

}  // namespace

CefMediaSinkImpl::CefMediaSinkImpl(const media_router::MediaSink& sink)
    : sink_(sink) {}

CefMediaSinkImpl::CefMediaSinkImpl(
    const media_router::MediaSink::Id& sink_id,
    const std::string& sink_name,
    media_router::mojom::MediaRouteProviderId provider_id)
    : sink_(sink_id,
            sink_name,
            media_router::SinkIconType::GENERIC,
            provider_id) {}

CefString CefMediaSinkImpl::GetId() {
  return sink_.id();
}

CefString CefMediaSinkImpl::GetName() {
  return sink_.name();
}

CefMediaSink::IconType CefMediaSinkImpl::GetIconType() {
  // Verify that our enum matches Chromium's values.
  static_assert(static_cast<int>(CEF_MSIT_TOTAL_COUNT) ==
                    static_cast<int>(media_router::SinkIconType::TOTAL_COUNT),
                "enum mismatch");

  return static_cast<CefMediaSink::IconType>(sink_.icon_type());
}

void CefMediaSinkImpl::GetDeviceInfo(
    CefRefPtr<CefMediaSinkDeviceInfoCallback> callback) {
  ::GetDeviceInfo(sink_.id(), callback);
}

bool CefMediaSinkImpl::IsCastSink() {
  return sink_.provider_id() == media_router::mojom::MediaRouteProviderId::CAST;
}

bool CefMediaSinkImpl::IsDialSink() {
  return sink_.provider_id() == media_router::mojom::MediaRouteProviderId::DIAL;
}

bool CefMediaSinkImpl::IsCompatibleWith(CefRefPtr<CefMediaSource> source) {
  if (source) {
    if (IsCastSink()) {
      return source->IsCastSource();
    }
    if (IsDialSink()) {
      return source->IsDialSource();
    }
  }
  return false;
}
