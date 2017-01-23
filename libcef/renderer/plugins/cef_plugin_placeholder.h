// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_PLUGINS_CEF_PLUGIN_PLACEHOLDER_H_
#define CEF_LIBCEF_RENDERER_PLUGINS_CEF_PLUGIN_PLACEHOLDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "chrome/renderer/plugins/power_saver_info.h"
#include "components/plugins/renderer/loadable_plugin_placeholder.h"
#include "content/public/renderer/context_menu_client.h"
#include "content/public/renderer/render_thread_observer.h"

enum class CefViewHostMsg_GetPluginInfo_Status;

class CefPluginPlaceholder final
    : public plugins::LoadablePluginPlaceholder,
      public content::RenderThreadObserver,
      public content::ContextMenuClient,
      public gin::Wrappable<CefPluginPlaceholder> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static CefPluginPlaceholder* CreateBlockedPlugin(
      content::RenderFrame* render_frame,
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params,
      const content::WebPluginInfo& info,
      const std::string& identifier,
      const base::string16& name,
      int resource_id,
      const base::string16& message,
      const PowerSaverInfo& power_saver_info);

  // Creates a new WebViewPlugin with a MissingPlugin as a delegate.
  static CefPluginPlaceholder* CreateLoadableMissingPlugin(
      content::RenderFrame* render_frame,
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params);

  void SetStatus(CefViewHostMsg_GetPluginInfo_Status status);

 private:
  CefPluginPlaceholder(content::RenderFrame* render_frame,
                       blink::WebLocalFrame* frame,
                       const blink::WebPluginParams& params,
                       const std::string& html_data,
                       const base::string16& title);
  ~CefPluginPlaceholder() override;

  // content::LoadablePluginPlaceholder overrides:
  blink::WebPlugin* CreatePlugin() override;
  void OnBlockedTinyContent() override;

  // gin::Wrappable (via PluginPlaceholder) method
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // content::RenderViewObserver (via PluginPlaceholder) override:
  bool OnMessageReceived(const IPC::Message& message) override;

  // WebViewPlugin::Delegate (via PluginPlaceholder) methods:
  v8::Local<v8::Value> GetV8Handle(v8::Isolate* isolate) override;
  void ShowContextMenu(const blink::WebMouseEvent&) override;

  // content::RenderThreadObserver methods:
  void PluginListChanged() override;

  // content::ContextMenuClient methods:
  void OnMenuAction(int request_id, unsigned action) override;
  void OnMenuClosed(int request_id) override;

  // Show the Plugins permission bubble.
  void ShowPermissionBubbleCallback();

  CefViewHostMsg_GetPluginInfo_Status status_;

  base::string16 title_;

  int context_menu_request_id_;  // Nonzero when request pending.
  base::string16 plugin_name_;

  bool did_send_blocked_content_notification_;

  DISALLOW_COPY_AND_ASSIGN(CefPluginPlaceholder);
};

#endif  // CEF_LIBCEF_RENDERER_PLUGINS_CEF_PLUGIN_PLACEHOLDER_H_
