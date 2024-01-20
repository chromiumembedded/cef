// Copyright 2022 The Chromium Embedded Framework Authors.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/touch_selection_controller_client_osr.h"

#include <cmath>
#include <set>

#include "libcef/browser/osr/render_widget_host_view_osr.h"
#include "libcef/browser/osr/touch_handle_drawable_osr.h"

#include "base/functional/bind.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_view_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace {

// Delay before showing the quick menu, in milliseconds.
constexpr int kQuickMenuDelayInMs = 100;

constexpr cef_quick_menu_edit_state_flags_t kMenuCommands[] = {
    QM_EDITFLAG_CAN_ELLIPSIS, QM_EDITFLAG_CAN_CUT, QM_EDITFLAG_CAN_COPY,
    QM_EDITFLAG_CAN_PASTE};

constexpr int kInvalidCommandId = -1;
constexpr cef_event_flags_t kEmptyEventFlags =
    static_cast<cef_event_flags_t>(0);

class CefRunQuickMenuCallbackImpl : public CefRunQuickMenuCallback {
 public:
  using Callback = base::OnceCallback<void(int, int)>;

  explicit CefRunQuickMenuCallbackImpl(Callback callback)
      : callback_(std::move(callback)) {}

  CefRunQuickMenuCallbackImpl(const CefRunQuickMenuCallbackImpl&) = delete;
  CefRunQuickMenuCallbackImpl& operator=(const CefRunQuickMenuCallbackImpl&) =
      delete;

  ~CefRunQuickMenuCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(std::move(callback_), kInvalidCommandId, kEmptyEventFlags);
      } else {
        CEF_POST_TASK(CEF_UIT,
                      base::BindOnce(&CefRunQuickMenuCallbackImpl::RunNow,
                                     std::move(callback_), kInvalidCommandId,
                                     kEmptyEventFlags));
      }
    }
  }

  void Continue(int command_id, cef_event_flags_t event_flags) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        RunNow(std::move(callback_), command_id, event_flags);
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefRunQuickMenuCallbackImpl::Continue, this,
                                   command_id, event_flags));
    }
  }

  void Cancel() override { Continue(kInvalidCommandId, kEmptyEventFlags); }

  void Disconnect() { callback_.Reset(); }

 private:
  static void RunNow(Callback callback,
                     int command_id,
                     cef_event_flags_t event_flags) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(command_id, event_flags);
  }

  Callback callback_;

  IMPLEMENT_REFCOUNTING(CefRunQuickMenuCallbackImpl);
};

}  // namespace

CefTouchSelectionControllerClientOSR::CefTouchSelectionControllerClientOSR(
    CefRenderWidgetHostViewOSR* rwhv)
    : rwhv_(rwhv),
      internal_client_(rwhv),
      active_client_(&internal_client_),
      active_menu_client_(this),
      quick_menu_timer_(
          FROM_HERE,
          base::Milliseconds(kQuickMenuDelayInMs),
          base::BindRepeating(
              &CefTouchSelectionControllerClientOSR::ShowQuickMenu,
              base::Unretained(this))),
      weak_ptr_factory_(this) {
  DCHECK(rwhv_);
}

CefTouchSelectionControllerClientOSR::~CefTouchSelectionControllerClientOSR() {
  for (auto& observer : observers_) {
    observer.OnManagerWillDestroy(this);
  }
}

void CefTouchSelectionControllerClientOSR::CloseQuickMenuAndHideHandles() {
  CloseQuickMenu();
  rwhv_->selection_controller()->HideAndDisallowShowingAutomatically();
}

void CefTouchSelectionControllerClientOSR::OnWindowMoved() {
  UpdateQuickMenu();
}

void CefTouchSelectionControllerClientOSR::OnTouchDown() {
  touch_down_ = true;
  UpdateQuickMenu();
}

void CefTouchSelectionControllerClientOSR::OnTouchUp() {
  touch_down_ = false;
  UpdateQuickMenu();
}

void CefTouchSelectionControllerClientOSR::OnScrollStarted() {
  scroll_in_progress_ = true;
  rwhv_->selection_controller()->SetTemporarilyHidden(true);
  UpdateQuickMenu();
}

void CefTouchSelectionControllerClientOSR::OnScrollCompleted() {
  scroll_in_progress_ = false;
  active_client_->DidScroll();
  rwhv_->selection_controller()->SetTemporarilyHidden(false);
  UpdateQuickMenu();
}

bool CefTouchSelectionControllerClientOSR::HandleContextMenu(
    const content::ContextMenuParams& params) {
  if ((params.source_type == ui::MENU_SOURCE_LONG_PRESS ||
       params.source_type == ui::MENU_SOURCE_LONG_TAP) &&
      params.is_editable && params.selection_text.empty() &&
      IsQuickMenuAvailable()) {
    quick_menu_requested_ = true;
    UpdateQuickMenu();
    return true;
  }

  const bool from_touch = params.source_type == ui::MENU_SOURCE_LONG_PRESS ||
                          params.source_type == ui::MENU_SOURCE_LONG_TAP ||
                          params.source_type == ui::MENU_SOURCE_TOUCH;
  if (from_touch && !params.selection_text.empty()) {
    return true;
  }

  rwhv_->selection_controller()->HideAndDisallowShowingAutomatically();
  return false;
}

void CefTouchSelectionControllerClientOSR::DidStopFlinging() {
  OnScrollCompleted();
}

void CefTouchSelectionControllerClientOSR::OnSwipeToMoveCursorBegin() {
  rwhv_->selection_controller()->OnSwipeToMoveCursorBegin();
  OnSelectionEvent(ui::INSERTION_HANDLE_DRAG_STARTED);
}

void CefTouchSelectionControllerClientOSR::OnSwipeToMoveCursorEnd() {
  rwhv_->selection_controller()->OnSwipeToMoveCursorEnd();
  OnSelectionEvent(ui::INSERTION_HANDLE_DRAG_STOPPED);
}

void CefTouchSelectionControllerClientOSR::OnClientHitTestRegionUpdated(
    ui::TouchSelectionControllerClient* client) {
  if (client != active_client_ || !rwhv_->selection_controller() ||
      rwhv_->selection_controller()->active_status() ==
          ui::TouchSelectionController::INACTIVE) {
    return;
  }

  active_client_->DidScroll();
}

void CefTouchSelectionControllerClientOSR::UpdateClientSelectionBounds(
    const gfx::SelectionBound& start,
    const gfx::SelectionBound& end) {
  UpdateClientSelectionBounds(start, end, &internal_client_, this);
}

void CefTouchSelectionControllerClientOSR::UpdateClientSelectionBounds(
    const gfx::SelectionBound& start,
    const gfx::SelectionBound& end,
    ui::TouchSelectionControllerClient* client,
    ui::TouchSelectionMenuClient* menu_client) {
  if (client != active_client_ &&
      (start.type() == gfx::SelectionBound::EMPTY || !start.visible()) &&
      (end.type() == gfx::SelectionBound::EMPTY || !end.visible()) &&
      (manager_selection_start_.type() != gfx::SelectionBound::EMPTY ||
       manager_selection_end_.type() != gfx::SelectionBound::EMPTY)) {
    return;
  }

  active_client_ = client;
  active_menu_client_ = menu_client;
  manager_selection_start_ = start;
  manager_selection_end_ = end;

  // Notify TouchSelectionController if anything should change here. Only
  // update if the client is different and not making a change to empty, or
  // is the same client.
  GetTouchSelectionController()->OnSelectionBoundsChanged(start, end);
}

void CefTouchSelectionControllerClientOSR::InvalidateClient(
    ui::TouchSelectionControllerClient* client) {
  DCHECK(client != &internal_client_);
  if (client == active_client_) {
    active_client_ = &internal_client_;
    active_menu_client_ = this;
  }
}

ui::TouchSelectionController*
CefTouchSelectionControllerClientOSR::GetTouchSelectionController() {
  return rwhv_->selection_controller();
}

void CefTouchSelectionControllerClientOSR::AddObserver(
    TouchSelectionControllerClientManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void CefTouchSelectionControllerClientOSR::RemoveObserver(
    TouchSelectionControllerClientManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool CefTouchSelectionControllerClientOSR::IsQuickMenuAvailable() const {
  DCHECK(active_menu_client_);

  const auto is_enabled = [this](cef_quick_menu_edit_state_flags_t command) {
    return active_menu_client_->IsCommandIdEnabled(command);
  };
  return std::any_of(std::cbegin(kMenuCommands), std::cend(kMenuCommands),
                     is_enabled);
}

void CefTouchSelectionControllerClientOSR::CloseQuickMenu() {
  if (!quick_menu_running_) {
    return;
  }

  quick_menu_running_ = false;

  auto browser = rwhv_->browser_impl();
  if (auto handler = browser->client()->GetContextMenuHandler()) {
    handler->OnQuickMenuDismissed(browser.get(), browser->GetFocusedFrame());
  }
}

void CefTouchSelectionControllerClientOSR::ShowQuickMenu() {
  auto browser = rwhv_->browser_impl();
  if (auto handler = browser->client()->GetContextMenuHandler()) {
    gfx::RectF rect =
        rwhv_->selection_controller()->GetVisibleRectBetweenBounds();

    gfx::PointF origin = rect.origin();
    gfx::PointF bottom_right = rect.bottom_right();
    auto client_bounds = gfx::RectF(rwhv_->GetViewBounds());
    origin.SetToMax(client_bounds.origin());
    bottom_right.SetToMin(client_bounds.bottom_right());
    if (origin.x() > bottom_right.x() || origin.y() > bottom_right.y()) {
      return;
    }

    gfx::Vector2dF diagonal = bottom_right - origin;
    gfx::SizeF size(diagonal.x(), diagonal.y());

    int quickmenuflags = 0;
    for (const auto& command : kMenuCommands) {
      if (active_menu_client_->IsCommandIdEnabled(command)) {
        quickmenuflags |= command;
      }
    }

    CefRefPtr<CefRunQuickMenuCallbackImpl> callbackImpl(
        new CefRunQuickMenuCallbackImpl(base::BindOnce(
            &CefTouchSelectionControllerClientOSR::ExecuteCommand,
            weak_ptr_factory_.GetWeakPtr())));

    quick_menu_running_ = true;
    if (!handler->RunQuickMenu(
            browser, browser->GetFocusedFrame(),
            {static_cast<int>(std::round(origin.x())),
             static_cast<int>(std::round(origin.y()))},
            {static_cast<int>(std::round(size.width())),
             static_cast<int>(std::round(size.height()))},
            static_cast<CefContextMenuHandler::QuickMenuEditStateFlags>(
                quickmenuflags),
            callbackImpl)) {
      callbackImpl->Disconnect();
      CloseQuickMenu();
    }
  }
}

void CefTouchSelectionControllerClientOSR::UpdateQuickMenu() {
  // Hide the quick menu if there is any. This should happen even if the menu
  // should be shown again, in order to update its location or content.
  if (quick_menu_running_) {
    CloseQuickMenu();
  } else {
    quick_menu_timer_.Stop();
  }

  // Start timer to show quick menu if necessary.
  if (ShouldShowQuickMenu()) {
    quick_menu_timer_.Reset();
  }
}

bool CefTouchSelectionControllerClientOSR::SupportsAnimation() const {
  return false;
}

bool CefTouchSelectionControllerClientOSR::InternalClient::SupportsAnimation()
    const {
  DCHECK(false);
  return false;
}

void CefTouchSelectionControllerClientOSR::SetNeedsAnimate() {
  DCHECK(false);
}

void CefTouchSelectionControllerClientOSR::InternalClient::SetNeedsAnimate() {
  DCHECK(false);
}

void CefTouchSelectionControllerClientOSR::MoveCaret(
    const gfx::PointF& position) {
  active_client_->MoveCaret(position);
}

void CefTouchSelectionControllerClientOSR::InternalClient::MoveCaret(
    const gfx::PointF& position) {
  if (auto host_delegate = rwhv_->host()->delegate()) {
    host_delegate->MoveCaret(gfx::ToRoundedPoint(position));
  }
}

void CefTouchSelectionControllerClientOSR::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  active_client_->MoveRangeSelectionExtent(extent);
}

void CefTouchSelectionControllerClientOSR::InternalClient::
    MoveRangeSelectionExtent(const gfx::PointF& extent) {
  if (auto host_delegate = rwhv_->host()->delegate()) {
    host_delegate->MoveRangeSelectionExtent(gfx::ToRoundedPoint(extent));
  }
}

void CefTouchSelectionControllerClientOSR::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  active_client_->SelectBetweenCoordinates(base, extent);
}

void CefTouchSelectionControllerClientOSR::InternalClient::
    SelectBetweenCoordinates(const gfx::PointF& base,
                             const gfx::PointF& extent) {
  if (auto host_delegate = rwhv_->host()->delegate()) {
    host_delegate->SelectRange(gfx::ToRoundedPoint(base),
                               gfx::ToRoundedPoint(extent));
  }
}

void CefTouchSelectionControllerClientOSR::OnSelectionEvent(
    ui::SelectionEventType event) {
  // This function (implicitly) uses active_menu_client_, so we don't go to the
  // active view for this.
  switch (event) {
    case ui::SELECTION_HANDLES_SHOWN:
      quick_menu_requested_ = true;
      [[fallthrough]];
    case ui::INSERTION_HANDLE_SHOWN:
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLES_CLEARED:
    case ui::INSERTION_HANDLE_CLEARED:
      quick_menu_requested_ = false;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLE_DRAG_STARTED:
    case ui::INSERTION_HANDLE_DRAG_STARTED:
      handle_drag_in_progress_ = true;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLE_DRAG_STOPPED:
    case ui::INSERTION_HANDLE_DRAG_STOPPED:
      handle_drag_in_progress_ = false;
      UpdateQuickMenu();
      break;
    case ui::SELECTION_HANDLES_MOVED:
    case ui::INSERTION_HANDLE_MOVED:
      UpdateQuickMenu();
      break;
    case ui::INSERTION_HANDLE_TAPPED:
      quick_menu_requested_ = !quick_menu_requested_;
      UpdateQuickMenu();
      break;
  }
}

void CefTouchSelectionControllerClientOSR::InternalClient::OnSelectionEvent(
    ui::SelectionEventType event) {
  DCHECK(false);
}

void CefTouchSelectionControllerClientOSR::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {}

void CefTouchSelectionControllerClientOSR::InternalClient::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {
  DCHECK(false);
}

std::unique_ptr<ui::TouchHandleDrawable>
CefTouchSelectionControllerClientOSR::CreateDrawable() {
  return std::make_unique<CefTouchHandleDrawableOSR>(rwhv_);
}

void CefTouchSelectionControllerClientOSR::DidScroll() {}

std::unique_ptr<ui::TouchHandleDrawable>
CefTouchSelectionControllerClientOSR::InternalClient::CreateDrawable() {
  DCHECK(false);
  return nullptr;
}

void CefTouchSelectionControllerClientOSR::InternalClient::DidScroll() {
  DCHECK(false);
}

bool CefTouchSelectionControllerClientOSR::IsCommandIdEnabled(
    int command_id) const {
  bool editable = rwhv_->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
  bool readable = rwhv_->GetTextInputType() != ui::TEXT_INPUT_TYPE_PASSWORD;
  bool has_selection = !rwhv_->GetSelectedText().empty();
  switch (command_id) {
    case QM_EDITFLAG_CAN_ELLIPSIS:
      return true;  // Always allowed to show the ellipsis button.
    case QM_EDITFLAG_CAN_CUT:
      return editable && readable && has_selection;
    case QM_EDITFLAG_CAN_COPY:
      return readable && has_selection;
    case QM_EDITFLAG_CAN_PASTE: {
      std::u16string result;
      ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
          ui::EndpointType::kDefault, /*notify_if_restricted=*/false);
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::ClipboardBuffer::kCopyPaste, &data_dst, &result);
      return editable && !result.empty();
    }
    default:
      return false;
  }
}

void CefTouchSelectionControllerClientOSR::ExecuteCommand(int command_id,
                                                          int event_flags) {
  if (command_id == kInvalidCommandId) {
    return;
  }

  if (command_id != QM_EDITFLAG_CAN_ELLIPSIS) {
    rwhv_->selection_controller()->HideAndDisallowShowingAutomatically();
  }

  content::RenderWidgetHostDelegate* host_delegate = rwhv_->host()->delegate();
  if (!host_delegate) {
    return;
  }

  auto browser = rwhv_->browser_impl();
  if (auto handler = browser->client()->GetContextMenuHandler()) {
    if (handler->OnQuickMenuCommand(
            browser.get(), browser->GetFocusedFrame(), command_id,
            static_cast<cef_event_flags_t>(event_flags))) {
      return;
    }
  }

  switch (command_id) {
    case QM_EDITFLAG_CAN_CUT:
      host_delegate->Cut();
      break;
    case QM_EDITFLAG_CAN_COPY:
      host_delegate->Copy();
      break;
    case QM_EDITFLAG_CAN_PASTE:
      host_delegate->Paste();
      break;
    case QM_EDITFLAG_CAN_ELLIPSIS:
      CloseQuickMenu();
      RunContextMenu();
      break;
    default:
      // Invalid command, do nothing.
      // Also reached when callback is destroyed/cancelled.
      break;
  }
}

void CefTouchSelectionControllerClientOSR::RunContextMenu() {
  const gfx::RectF anchor_rect =
      rwhv_->selection_controller()->GetVisibleRectBetweenBounds();
  const gfx::PointF anchor_point =
      gfx::PointF(anchor_rect.CenterPoint().x(), anchor_rect.y());
  rwhv_->host()->ShowContextMenuAtPoint(gfx::ToRoundedPoint(anchor_point),
                                        ui::MENU_SOURCE_TOUCH_EDIT_MENU);

  // Hide selection handles after getting rect-between-bounds from touch
  // selection controller; otherwise, rect would be empty and the above
  // calculations would be invalid.
  rwhv_->selection_controller()->HideAndDisallowShowingAutomatically();
}

bool CefTouchSelectionControllerClientOSR::ShouldShowQuickMenu() {
  return quick_menu_requested_ && !touch_down_ && !scroll_in_progress_ &&
         !handle_drag_in_progress_ && IsQuickMenuAvailable();
}

std::u16string CefTouchSelectionControllerClientOSR::GetSelectedText() {
  return rwhv_->GetSelectedText();
}
