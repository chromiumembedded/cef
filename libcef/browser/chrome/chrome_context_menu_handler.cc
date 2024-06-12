// Copyright (c) 2021 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/chrome/chrome_context_menu_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/context_menu_params_impl.h"
#include "cef/libcef/browser/simple_menu_model_impl.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

namespace context_menu {

namespace {

constexpr int kInvalidCommandId = -1;
const cef_event_flags_t kEmptyEventFlags = static_cast<cef_event_flags_t>(0);

class CefRunContextMenuCallbackImpl : public CefRunContextMenuCallback {
 public:
  using Callback =
      base::OnceCallback<void(int /*command_id*/, int /*event_flags*/)>;

  explicit CefRunContextMenuCallbackImpl(Callback callback)
      : callback_(std::move(callback)) {}

  CefRunContextMenuCallbackImpl(const CefRunContextMenuCallbackImpl&) = delete;
  CefRunContextMenuCallbackImpl& operator=(
      const CefRunContextMenuCallbackImpl&) = delete;

  ~CefRunContextMenuCallbackImpl() override {
    if (!callback_.is_null()) {
      // The callback is still pending. Cancel it now.
      if (CEF_CURRENTLY_ON_UIT()) {
        RunNow(std::move(callback_), kInvalidCommandId, kEmptyEventFlags);
      } else {
        CEF_POST_TASK(CEF_UIT,
                      base::BindOnce(&CefRunContextMenuCallbackImpl::RunNow,
                                     std::move(callback_), kInvalidCommandId,
                                     kEmptyEventFlags));
      }
    }
  }

  void Continue(int command_id, cef_event_flags_t event_flags) override {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!callback_.is_null()) {
        RunNow(std::move(callback_), command_id, event_flags);
        callback_.Reset();
      }
    } else {
      CEF_POST_TASK(CEF_UIT,
                    base::BindOnce(&CefRunContextMenuCallbackImpl::Continue,
                                   this, command_id, event_flags));
    }
  }

  void Cancel() override { Continue(kInvalidCommandId, kEmptyEventFlags); }

  bool IsDisconnected() const { return callback_.is_null(); }
  void Disconnect() { callback_.Reset(); }

 private:
  static void RunNow(Callback callback, int command_id, int event_flags) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(command_id, event_flags);
  }

  Callback callback_;

  IMPLEMENT_REFCOUNTING(CefRunContextMenuCallbackImpl);
};

// Lifespan is controlled by RenderViewContextMenu.
class CefContextMenuObserver : public RenderViewContextMenuObserver,
                               public CefSimpleMenuModelImpl::StateDelegate {
 public:
  CefContextMenuObserver(RenderViewContextMenu* context_menu,
                         CefRefPtr<CefBrowserHostBase> browser,
                         CefRefPtr<CefContextMenuHandler> handler)
      : context_menu_(context_menu), browser_(browser), handler_(handler) {
    // This remains valid until the next time a context menu is created.
    browser_->set_context_menu_observer(this);
  }

  CefContextMenuObserver(const CefContextMenuObserver&) = delete;
  CefContextMenuObserver& operator=(const CefContextMenuObserver&) = delete;

  // RenderViewContextMenuObserver methods:

  void InitMenu(const content::ContextMenuParams& params) override {
    params_ = new CefContextMenuParamsImpl(
        const_cast<content::ContextMenuParams*>(&context_menu_->params()));
    model_ = new CefSimpleMenuModelImpl(
        const_cast<ui::SimpleMenuModel*>(&context_menu_->menu_model()),
        context_menu_, this, /*is_owned=*/false, /*is_submenu=*/false);

    handler_->OnBeforeContextMenu(browser_, GetFrame(), params_, model_);
  }

  bool IsCommandIdSupported(int command_id) override {
    // Always claim support for the reserved user ID range.
    if (command_id >= MENU_ID_USER_FIRST && command_id <= MENU_ID_USER_LAST) {
      return true;
    }

    // Also claim support in specific cases where an ItemInfo exists.
    return GetItemInfo(command_id) != nullptr;
  }

  // Only called if IsCommandIdSupported() returns true.
  bool IsCommandIdEnabled(int command_id) override {
    // Always return true to use the SimpleMenuModel state.
    return true;
  }

  // Only called if IsCommandIdSupported() returns true.
  bool IsCommandIdChecked(int command_id) override {
    auto* info = GetItemInfo(command_id);
    return info ? info->checked : false;
  }

  // Only called if IsCommandIdSupported() returns true.
  bool GetAccelerator(int command_id, ui::Accelerator* accel) override {
    auto* info = GetItemInfo(command_id);
    if (info && info->accel) {
      *accel = *info->accel;
      return true;
    }
    return false;
  }

  void CommandWillBeExecuted(int command_id) override {
    if (handler_->OnContextMenuCommand(browser_, GetFrame(), params_,
                                       command_id, EVENTFLAG_NONE)) {
      // Create an ItemInfo so that we get the ExecuteCommand() callback
      // instead of the default handler.
      GetOrCreateItemInfo(command_id);
    }
  }

  // Only called if IsCommandIdSupported() returns true.
  void ExecuteCommand(int command_id) override {
    auto* info = GetItemInfo(command_id);
    if (info) {
      // In case it was added in CommandWillBeExecuted().
      MaybeDeleteItemInfo(command_id, info);
    }
  }

  void OnMenuClosed() override {
    // May be called multiple times. For example, if the menu runs and is
    // additionally reset via MaybeResetContextMenu.
    if (!handler_) {
      return;
    }

    handler_->OnContextMenuDismissed(browser_, GetFrame());
    model_->Detach();

    // Clear stored state because this object won't be deleted until a new
    // context menu is created or the associated browser is destroyed.
    browser_ = nullptr;
    handler_ = nullptr;
    params_ = nullptr;
    model_ = nullptr;
    iteminfomap_.clear();
  }

  // CefSimpleMenuModelImpl::StateDelegate methods:

  void SetChecked(int command_id, bool checked) override {
    // No-op if already at the default state.
    if (!checked && !GetItemInfo(command_id)) {
      return;
    }

    auto* info = GetOrCreateItemInfo(command_id);
    info->checked = checked;
    if (!checked) {
      MaybeDeleteItemInfo(command_id, info);
    }
  }

  void SetAccelerator(int command_id,
                      std::optional<ui::Accelerator> accel) override {
    // No-op if already at the default state.
    if (!accel && !GetItemInfo(command_id)) {
      return;
    }

    auto* info = GetOrCreateItemInfo(command_id);
    info->accel = accel;
    if (!accel) {
      MaybeDeleteItemInfo(command_id, info);
    }
  }

  bool HandleShow() {
    if (model_->GetCount() == 0) {
      return false;
    }

    CefRefPtr<CefRunContextMenuCallbackImpl> callbackImpl(
        new CefRunContextMenuCallbackImpl(
            base::BindOnce(&CefContextMenuObserver::ExecuteCommandCallback,
                           weak_ptr_factory_.GetWeakPtr())));

    is_handled_ = handler_->RunContextMenu(browser_, GetFrame(), params_,
                                           model_, callbackImpl.get());
    if (!is_handled_ && callbackImpl->IsDisconnected()) {
      LOG(ERROR) << "Should return true from RunContextMenu when executing the "
                    "callback";
      is_handled_ = true;
    }
    if (!is_handled_) {
      callbackImpl->Disconnect();
    }
    return is_handled_;
  }

  void MaybeResetContextMenu() {
    // Don't reset the menu when the client is using custom handling. It will be
    // reset via ExecuteCommandCallback instead.
    if (!is_handled_) {
      OnMenuClosed();
    }
  }

 private:
  struct ItemInfo {
    ItemInfo() = default;

    bool checked = false;
    std::optional<ui::Accelerator> accel;
  };

  ItemInfo* GetItemInfo(int command_id) {
    auto it = iteminfomap_.find(command_id);
    if (it != iteminfomap_.end()) {
      return &it->second;
    }
    return nullptr;
  }

  ItemInfo* GetOrCreateItemInfo(int command_id) {
    if (auto info = GetItemInfo(command_id)) {
      return info;
    }

    auto result = iteminfomap_.insert(std::make_pair(command_id, ItemInfo()));
    return &result.first->second;
  }

  void MaybeDeleteItemInfo(int command_id, ItemInfo* info) {
    // Remove if all info has reverted to the default state.
    if (!info->checked && !info->accel) {
      auto it = iteminfomap_.find(command_id);
      iteminfomap_.erase(it);
    }
  }

  CefRefPtr<CefFrame> GetFrame() const {
    CefRefPtr<CefFrame> frame;

    // May return nullptr if the frame is destroyed while the menu is pending.
    auto* rfh = context_menu_->GetRenderFrameHost();
    if (rfh) {
      // May return nullptr for excluded views.
      frame = browser_->GetFrameForHost(rfh);
    }
    if (!frame) {
      frame = browser_->GetMainFrame();
    }
    return frame;
  }

  void ExecuteCommandCallback(int command_id, int event_flags) {
    if (command_id != kInvalidCommandId) {
      context_menu_->ExecuteCommand(command_id, event_flags);
    }
    context_menu_->Cancel();
    OnMenuClosed();
  }

  const raw_ptr<RenderViewContextMenu> context_menu_;
  CefRefPtr<CefBrowserHostBase> browser_;
  CefRefPtr<CefContextMenuHandler> handler_;
  CefRefPtr<CefContextMenuParams> params_;
  CefRefPtr<CefSimpleMenuModelImpl> model_;

  // Map of command_id to ItemInfo.
  using ItemInfoMap = std::map<int, ItemInfo>;
  ItemInfoMap iteminfomap_;

  bool is_handled_ = false;

  base::WeakPtrFactory<CefContextMenuObserver> weak_ptr_factory_{this};
};

std::unique_ptr<RenderViewContextMenuObserver> MenuCreatedCallback(
    RenderViewContextMenu* context_menu) {
  auto browser = CefBrowserHostBase::GetBrowserForContents(
      context_menu->source_web_contents());
  if (browser) {
    if (auto client = browser->GetClient()) {
      if (auto handler = client->GetContextMenuHandler()) {
        return std::make_unique<CefContextMenuObserver>(context_menu, browser,
                                                        handler);
      }
    }

    // Don't leave the old pointer, if any.
    browser->set_context_menu_observer(nullptr);
  }

  return nullptr;
}

bool MenuShowHandlerCallback(RenderViewContextMenu* context_menu) {
  auto browser = CefBrowserHostBase::GetBrowserForContents(
      context_menu->source_web_contents());
  if (browser && browser->context_menu_observer()) {
    return static_cast<CefContextMenuObserver*>(
               browser->context_menu_observer())
        ->HandleShow();
  }
  return false;
}

}  // namespace

void RegisterCallbacks() {
  RenderViewContextMenu::RegisterMenuCreatedCallback(
      base::BindRepeating(&MenuCreatedCallback));
  RenderViewContextMenu::RegisterMenuShowHandlerCallback(
      base::BindRepeating(&MenuShowHandlerCallback));
}

bool HandleContextMenu(content::WebContents* opener,
                       const content::ContextMenuParams& params) {
  auto browser = CefBrowserHostBase::GetBrowserForContents(opener);
  if (browser && browser->IsAlloyStyle()) {
    AlloyBrowserHostImpl::FromBaseChecked(browser)->ShowContextMenu(params);
    return true;
  }

  // Continue with creating the RenderViewContextMenu.
  return false;
}

void MaybeResetContextMenu(content::WebContents* opener) {
  auto browser = CefBrowserHostBase::GetBrowserForContents(opener);
  if (browser && browser->context_menu_observer()) {
    return static_cast<CefContextMenuObserver*>(
               browser->context_menu_observer())
        ->MaybeResetContextMenu();
  }
}

}  // namespace context_menu
