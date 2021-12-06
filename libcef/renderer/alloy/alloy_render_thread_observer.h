// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_ALLOY_ALLOY_RENDER_THREAD_OBSERVER_H_
#define CEF_LIBCEF_RENDERER_ALLOY_ALLOY_RENDER_THREAD_OBSERVER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"

// This class sends and receives control messages in the renderer process.
class AlloyRenderThreadObserver : public content::RenderThreadObserver,
                                  public chrome::mojom::RendererConfiguration {
 public:
  AlloyRenderThreadObserver();

  AlloyRenderThreadObserver(const AlloyRenderThreadObserver&) = delete;
  AlloyRenderThreadObserver& operator=(const AlloyRenderThreadObserver&) =
      delete;

  ~AlloyRenderThreadObserver() override;

  static bool is_incognito_process() { return is_incognito_process_; }

  // Return the dynamic parameters - those that may change while the
  // render process is running.
  static const chrome::mojom::DynamicParams& GetDynamicParams();

 private:
  // content::RenderThreadObserver:
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // chrome::mojom::RendererConfiguration:
  void SetInitialConfiguration(
      bool is_incognito_process,
      mojo::PendingReceiver<chrome::mojom::ChromeOSListener> chromeos_listener,
      mojo::PendingRemote<content_settings::mojom::ContentSettingsManager>
          content_settings_manager) override;
  void SetConfiguration(chrome::mojom::DynamicParamsPtr params) override;
  void SetContentSettingRules(
      const RendererContentSettingRules& rules) override;

  void OnRendererConfigurationAssociatedRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::RendererConfiguration>
          receiver);

  static bool is_incognito_process_;

  mojo::AssociatedReceiverSet<chrome::mojom::RendererConfiguration>
      renderer_configuration_receivers_;
};

#endif  // CEF_LIBCEF_RENDERER_ALLOY_ALLOY_RENDER_THREAD_OBSERVER_H_
