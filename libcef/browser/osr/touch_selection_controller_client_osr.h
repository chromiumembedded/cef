// Copyright 2022 The Chromium Embedded Framework Authors.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_TOUCH_SELECTION_CONTROLLER_CLIENT_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_TOUCH_SELECTION_CONTROLLER_CLIENT_OSR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"

namespace content {
struct ContextMenuParams;
}

class CefRenderWidgetHostViewOSR;

// An implementation of |TouchSelectionControllerClient| to be used in OSR's
// implementation of touch selection for contents.
// Copied from TouchSelectionControllerClientAura.
class CefTouchSelectionControllerClientOSR
    : public ui::TouchSelectionControllerClient,
      public ui::TouchSelectionMenuClient,
      public content::TouchSelectionControllerClientManager {
 public:
  explicit CefTouchSelectionControllerClientOSR(
      CefRenderWidgetHostViewOSR* rwhv);

  CefTouchSelectionControllerClientOSR(
      const CefTouchSelectionControllerClientOSR&) = delete;
  CefTouchSelectionControllerClientOSR& operator=(
      const CefTouchSelectionControllerClientOSR&) = delete;

  ~CefTouchSelectionControllerClientOSR() override;

  void CloseQuickMenuAndHideHandles();

  void OnWindowMoved();

  // Called on first touch down/last touch up to hide/show the quick menu.
  void OnTouchDown();

  void OnTouchUp();

  // Called when touch scroll starts/completes to hide/show touch handles and
  // the quick menu.
  void OnScrollStarted();

  void OnScrollCompleted();

  // Gives an opportunity to the client to handle context menu request and show
  // the quick menu instead, if appropriate. Returns |true| to indicate that no
  // further handling is needed.
  // TODO(mohsen): This is to match Chrome on Android behavior. However, it is
  // better not to send context menu request from the renderer in this case and
  // instead decide in the client about showing the quick menu in response to
  // selection events. (http://crbug.com/548245)
  bool HandleContextMenu(const content::ContextMenuParams& params);

  void UpdateClientSelectionBounds(const gfx::SelectionBound& start,
                                   const gfx::SelectionBound& end);

  // TouchSelectionControllerClientManager.
  void DidStopFlinging() override;
  void OnSwipeToMoveCursorBegin() override;
  void OnSwipeToMoveCursorEnd() override;
  void OnClientHitTestRegionUpdated(
      ui::TouchSelectionControllerClient* client) override;
  void UpdateClientSelectionBounds(
      const gfx::SelectionBound& start,
      const gfx::SelectionBound& end,
      ui::TouchSelectionControllerClient* client,
      ui::TouchSelectionMenuClient* menu_client) override;
  void InvalidateClient(ui::TouchSelectionControllerClient* client) override;
  ui::TouchSelectionController* GetTouchSelectionController() override;
  void AddObserver(
      TouchSelectionControllerClientManager::Observer* observer) override;
  void RemoveObserver(
      TouchSelectionControllerClientManager::Observer* observer) override;

 private:
  class EnvEventObserver;

  bool IsQuickMenuAvailable() const;
  void CloseQuickMenu();
  void ShowQuickMenu();
  void UpdateQuickMenu();

  // ui::TouchSelectionControllerClient:
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  void OnDragUpdate(const ui::TouchSelectionDraggable::Type type,
                    const gfx::PointF& position) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;
  void DidScroll() override;

  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void RunContextMenu() override;
  bool ShouldShowQuickMenu() override;
  std::u16string GetSelectedText() override;

  // Not owned, non-null for the lifetime of this object.
  raw_ptr<CefRenderWidgetHostViewOSR> rwhv_;

  class InternalClient final : public ui::TouchSelectionControllerClient {
   public:
    explicit InternalClient(CefRenderWidgetHostViewOSR* rwhv) : rwhv_(rwhv) {}
    ~InternalClient() final = default;

    bool SupportsAnimation() const final;
    void SetNeedsAnimate() final;
    void MoveCaret(const gfx::PointF& position) final;
    void MoveRangeSelectionExtent(const gfx::PointF& extent) final;
    void SelectBetweenCoordinates(const gfx::PointF& base,
                                  const gfx::PointF& extent) final;
    void OnSelectionEvent(ui::SelectionEventType event) final;
    void OnDragUpdate(const ui::TouchSelectionDraggable::Type type,
                      const gfx::PointF& position) final;
    std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() final;
    void DidScroll() override;

   private:
    raw_ptr<CefRenderWidgetHostViewOSR> rwhv_;
  } internal_client_;

  // Keep track of which client interface to use.
  raw_ptr<TouchSelectionControllerClient> active_client_;
  raw_ptr<TouchSelectionMenuClient> active_menu_client_;
  gfx::SelectionBound manager_selection_start_;
  gfx::SelectionBound manager_selection_end_;

  base::ObserverList<TouchSelectionControllerClientManager::Observer>
      observers_;

  base::RetainingOneShotTimer quick_menu_timer_;
  bool quick_menu_requested_ = false;
  bool quick_menu_running_ = false;
  bool touch_down_ = false;
  bool scroll_in_progress_ = false;
  bool handle_drag_in_progress_ = false;

  base::WeakPtrFactory<CefTouchSelectionControllerClientOSR> weak_ptr_factory_;
};

#endif
