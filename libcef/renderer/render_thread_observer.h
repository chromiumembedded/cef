// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_RENDER_THREAD_OBSERVER_H_
#define CEF_LIBCEF_RENDERER_RENDER_THREAD_OBSERVER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"

struct Cef_CrossOriginWhiteListEntry_Params;

// This class sends and receives control messages in the renderer process.
class CefRenderThreadObserver : public content::RenderThreadObserver,
                                public chrome::mojom::RendererConfiguration {
 public:
  CefRenderThreadObserver();
  ~CefRenderThreadObserver() override;

  static bool is_incognito_process() { return is_incognito_process_; }

  // Return the dynamic parameters - those that may change while the
  // render process is running.
  static const chrome::mojom::DynamicParams& GetDynamicParams();

 private:
  // content::RenderThreadObserver:
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void RegisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      blink::AssociatedInterfaceRegistry* associated_interfaces) override;

  // chrome::mojom::RendererConfiguration:
  void SetInitialConfiguration(
      bool is_incognito_process,
      mojo::PendingReceiver<chrome::mojom::ChromeOSListener> chromeos_listener)
      override;
  void SetConfiguration(chrome::mojom::DynamicParamsPtr params) override;
  void SetContentSettingRules(
      const RendererContentSettingRules& rules) override;

  void OnRendererConfigurationAssociatedRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::RendererConfiguration>
          receiver);

  // Message handlers called on the render thread.
  void OnModifyCrossOriginWhitelistEntry(
      bool add,
      const Cef_CrossOriginWhiteListEntry_Params& params);
  void OnClearCrossOriginWhitelist();

  static bool is_incognito_process_;

  mojo::AssociatedReceiverSet<chrome::mojom::RendererConfiguration>
      renderer_configuration_receivers_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderThreadObserver);
};

#endif  // CEF_LIBCEF_RENDERER_RENDER_THREAD_OBSERVER_H_
