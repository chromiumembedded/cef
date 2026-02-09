// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_COMPONENT_UPDATER_IMPL_H_
#define CEF_LIBCEF_BROWSER_COMPONENT_UPDATER_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_component_updater.h"

namespace component_updater {
class ComponentUpdateService;
}

class CefComponentImpl : public CefComponent {
 public:
  CefComponentImpl(std::string id,
                   std::string name,
                   std::string version,
                   cef_component_state_t state);

  CefComponentImpl(const CefComponentImpl&) = delete;
  CefComponentImpl& operator=(const CefComponentImpl&) = delete;

  // CefComponent methods:
  CefString GetID() override;
  CefString GetName() override;
  CefString GetVersion() override;
  cef_component_state_t GetState() override;

 private:
  const std::string id_;
  const std::string name_;
  const std::string version_;
  const cef_component_state_t state_;

  IMPLEMENT_REFCOUNTING(CefComponentImpl);
};

class CefComponentUpdaterImpl : public CefComponentUpdater {
 public:
  explicit CefComponentUpdaterImpl(
      component_updater::ComponentUpdateService* component_updater);

  CefComponentUpdaterImpl(const CefComponentUpdaterImpl&) = delete;
  CefComponentUpdaterImpl& operator=(const CefComponentUpdaterImpl&) = delete;

  // CefComponentUpdater methods:
  size_t GetComponentCount() override;
  void GetComponents(std::vector<CefRefPtr<CefComponent>>& components) override;
  CefRefPtr<CefComponent> GetComponentByID(
      const CefString& component_id) override;
  void Update(const CefString& component_id,
              cef_component_update_priority_t priority,
              CefRefPtr<CefComponentUpdateCallback> callback) override;

 private:
  raw_ptr<component_updater::ComponentUpdateService> component_updater_;

  IMPLEMENT_REFCOUNTING(CefComponentUpdaterImpl);
};

#endif  // CEF_LIBCEF_BROWSER_COMPONENT_UPDATER_IMPL_H_
