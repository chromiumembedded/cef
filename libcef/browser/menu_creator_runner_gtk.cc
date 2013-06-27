// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/menu_creator_runner_gtk.h"
#include "libcef/browser/browser_host_impl.h"

#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/gfx/point.h"

namespace {

class CefMenuDelegate : public MenuGtk::Delegate {
 public:
  CefMenuDelegate() {}
};

}  // namespace


CefMenuCreatorRunnerGtk::CefMenuCreatorRunnerGtk() {
}

CefMenuCreatorRunnerGtk::~CefMenuCreatorRunnerGtk() {
  if (menu_.get())
    menu_->Cancel();
}

bool CefMenuCreatorRunnerGtk::RunContextMenu(CefMenuCreator* manager) {
  gfx::Point screen_point;
  GdkEventButton* event = NULL;

  if (manager->browser()->IsWindowRenderingDisabled()) {
    CefRefPtr<CefClient> client = manager->browser()->GetClient();
    if (!client.get())
      return false;

    CefRefPtr<CefRenderHandler> handler = client->GetRenderHandler();
    if (!handler.get())
      return false;

    int screenX = 0, screenY = 0;
    if (!handler->GetScreenPoint(manager->browser(),
                                 manager->params().x, manager->params().y,
                                 screenX, screenY)) {
      return false;
    }

    screen_point = gfx::Point(screenX, screenY);
  } else {
    gfx::Rect bounds;
    manager->browser()->GetWebContents()->GetView()->GetContainerBounds(&bounds);
    screen_point = bounds.origin();
    screen_point.Offset(manager->params().x, manager->params().y);

    content::RenderWidgetHostView* view =
        manager->browser()->GetWebContents()->GetRenderWidgetHostView();
    event = view->GetLastMouseDown();
  }

  if (!menu_delegate_.get())
    menu_delegate_.reset(new CefMenuDelegate);

  // Create a menu based on the model.
  menu_.reset(new MenuGtk(menu_delegate_.get(), manager->model()));

  uint32_t triggering_event_time = event ? event->time : GDK_CURRENT_TIME;
 
  // Show the menu. Execution will continue asynchronously.
  menu_->PopupAsContext(screen_point, triggering_event_time);

  return true;
}
