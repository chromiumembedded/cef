// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/setting_helper.h"

#include "cef/include/cef_request_context.h"
#include "cef/libcef/browser/thread_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

namespace setting_helper {

class RegistrationImpl final : public Registration, public CefRegistration {
 public:
  RegistrationImpl(Registrar* registrar, CefRefPtr<CefSettingObserver> observer)
      : registrar_(registrar), observer_(observer) {
    DCHECK(registrar_);
    DCHECK(observer_);
  }

  RegistrationImpl(const RegistrationImpl&) = delete;
  RegistrationImpl& operator=(const RegistrationImpl&) = delete;

  ~RegistrationImpl() override {
    CEF_REQUIRE_UIT();
    if (registrar_) {
      registrar_->RemoveObserver(this);
    }
  }

  void Detach() override {
    registrar_ = nullptr;
    observer_ = nullptr;
  }

  void RunCallback(const CefString& requesting_url,
                   const CefString& top_level_url,
                   cef_content_setting_types_t content_type) const override {
    observer_->OnSettingChanged(requesting_url, top_level_url, content_type);
  }

 private:
  raw_ptr<Registrar> registrar_;
  CefRefPtr<CefSettingObserver> observer_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(RegistrationImpl);
};

Registrar::~Registrar() {
  RemoveAll();
}

void Registrar::Init(HostContentSettingsMap* settings) {
  DCHECK(settings);
  DCHECK(IsEmpty() || settings_ == settings);
  settings_ = settings;
}

void Registrar::Reset() {
  RemoveAll();
  settings_ = nullptr;
}

void Registrar::RemoveAll() {
  if (!observers_.empty()) {
    settings_->RemoveObserver(this);
    for (auto& registration : observers_) {
      registration.Detach();
    }
    observers_.Clear();
  }
}

bool Registrar::IsEmpty() const {
  return observers_.empty();
}

CefRefPtr<CefRegistration> Registrar::AddObserver(
    CefRefPtr<CefSettingObserver> observer) {
  CHECK(settings_);

  RegistrationImpl* impl = new RegistrationImpl(this, observer);

  if (observers_.empty()) {
    settings_->AddObserver(this);
  }
  observers_.AddObserver(impl);

  return impl;
}

void Registrar::RemoveObserver(Registration* registration) {
  CHECK(settings_);

  observers_.RemoveObserver(registration);
  if (observers_.empty()) {
    settings_->RemoveObserver(this);
  }
}

void Registrar::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  DCHECK(!IsEmpty());

  const CefString requesting_url(primary_pattern.ToRepresentativeUrl().spec());
  const CefString top_level_url(secondary_pattern.ToRepresentativeUrl().spec());
  const auto content_type =
      static_cast<cef_content_setting_types_t>(content_type_set.GetType());

  for (Registration& registration : observers_) {
    registration.RunCallback(requesting_url, top_level_url, content_type);
  }
}

}  // namespace setting_helper
