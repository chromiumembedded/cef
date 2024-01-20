// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/menu_manager.h"

#include <tuple>
#include <utility>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/context_menu_params_impl.h"
#include "libcef/browser/menu_runner.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/app_manager.h"

#include "base/logging.h"
#include "cef/grit/cef_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

namespace {

CefString GetLabel(int message_id) {
  std::u16string label =
      CefAppManager::Get()->GetContentClient()->GetLocalizedString(message_id);
  DCHECK(!label.empty());
  return label;
}

const int kInvalidCommandId = -1;
const cef_event_flags_t kEmptyEventFlags = static_cast<cef_event_flags_t>(0);

class CefRunContextMenuCallbackImpl : public CefRunContextMenuCallback {
 public:
  using Callback = base::OnceCallback<void(int, cef_event_flags_t)>;

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

  void Disconnect() { callback_.Reset(); }

 private:
  static void RunNow(Callback callback,
                     int command_id,
                     cef_event_flags_t event_flags) {
    CEF_REQUIRE_UIT();
    std::move(callback).Run(command_id, event_flags);
  }

  Callback callback_;

  IMPLEMENT_REFCOUNTING(CefRunContextMenuCallbackImpl);
};

}  // namespace

CefMenuManager::CefMenuManager(AlloyBrowserHostImpl* browser,
                               std::unique_ptr<CefMenuRunner> runner)
    : content::WebContentsObserver(browser->web_contents()),
      browser_(browser),
      runner_(std::move(runner)),

      weak_ptr_factory_(this) {
  DCHECK(web_contents());
  model_ = new CefMenuModelImpl(this, nullptr, false);
}

CefMenuManager::~CefMenuManager() {
  // The model may outlive the delegate if the context menu is visible when the
  // application is closed.
  model_->set_delegate(nullptr);
}

void CefMenuManager::Destroy() {
  CancelContextMenu();
  if (runner_) {
    runner_.reset(nullptr);
  }
}

bool CefMenuManager::IsShowingContextMenu() {
  if (!web_contents()) {
    return false;
  }
  return web_contents()->IsShowingContextMenu();
}

bool CefMenuManager::CreateContextMenu(
    const content::ContextMenuParams& params) {
  // The renderer may send the "show context menu" message multiple times, one
  // for each right click mouse event it receives. Normally, this doesn't happen
  // because mouse events are not forwarded once the context menu is showing.
  // However, there's a race - the context menu may not yet be showing when
  // the second mouse event arrives. In this case, |HandleContextMenu()| will
  // get called multiple times - if so, don't create another context menu.
  // TODO(asvitkine): Fix the renderer so that it doesn't do this.
  if (IsShowingContextMenu()) {
    return true;
  }

  params_ = params;
  model_->Clear();

  // Create the default menu model.
  CreateDefaultModel();

  bool custom_menu = false;
  DCHECK(!custom_menu_callback_);

  // Give the client a chance to modify the model.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefContextMenuHandler> handler = client->GetContextMenuHandler();
    if (handler.get()) {
      CefRefPtr<CefContextMenuParamsImpl> paramsPtr(
          new CefContextMenuParamsImpl(&params_));
      CefRefPtr<CefFrame> frame = browser_->GetFocusedFrame();

      handler->OnBeforeContextMenu(browser_, frame, paramsPtr.get(),
                                   model_.get());

      MenuWillShow(model_);

      if (model_->GetCount() > 0) {
        CefRefPtr<CefRunContextMenuCallbackImpl> callbackImpl(
            new CefRunContextMenuCallbackImpl(
                base::BindOnce(&CefMenuManager::ExecuteCommandCallback,
                               weak_ptr_factory_.GetWeakPtr())));

        // This reference will be cleared when the callback is executed or
        // the callback object is deleted.
        custom_menu_callback_ = callbackImpl.get();

        if (handler->RunContextMenu(browser_, frame, paramsPtr.get(),
                                    model_.get(), callbackImpl.get())) {
          custom_menu = true;
        } else {
          // Callback should not be executed if the handler returns false.
          DCHECK(custom_menu_callback_);
          custom_menu_callback_ = nullptr;
          callbackImpl->Disconnect();
        }
      }

      // Do not keep references to the parameters in the callback.
      std::ignore = paramsPtr->Detach(nullptr);
      DCHECK(paramsPtr->HasOneRef());
      DCHECK(model_->VerifyRefCount());

      // Menu is empty so notify the client and return.
      if (model_->GetCount() == 0 && !custom_menu) {
        MenuClosed(model_);
        return true;
      }
    }
  }

  if (custom_menu) {
    return true;
  }

  if (!runner_ || !runner_->RunContextMenu(browser_, model_.get(), params_)) {
    LOG(ERROR) << "Default context menu implementation is not available; "
                  "canceling the menu";
    return false;
  }
  return true;
}

void CefMenuManager::CancelContextMenu() {
  if (IsShowingContextMenu()) {
    if (custom_menu_callback_) {
      custom_menu_callback_->Cancel();
    } else if (runner_) {
      runner_->CancelContextMenu();
    }
  }
}

void CefMenuManager::ExecuteCommand(CefRefPtr<CefMenuModelImpl> source,
                                    int command_id,
                                    cef_event_flags_t event_flags) {
  // Give the client a chance to handle the command.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefContextMenuHandler> handler = client->GetContextMenuHandler();
    if (handler.get()) {
      CefRefPtr<CefContextMenuParamsImpl> paramsPtr(
          new CefContextMenuParamsImpl(&params_));

      bool handled = handler->OnContextMenuCommand(
          browser_, browser_->GetFocusedFrame(), paramsPtr.get(), command_id,
          event_flags);

      // Do not keep references to the parameters in the callback.
      std::ignore = paramsPtr->Detach(nullptr);
      DCHECK(paramsPtr->HasOneRef());

      if (handled) {
        return;
      }
    }
  }

  // Execute the default command handling.
  ExecuteDefaultCommand(command_id);
}

void CefMenuManager::MenuWillShow(CefRefPtr<CefMenuModelImpl> source) {
  // May be called for sub-menus as well.
  if (source.get() != model_.get()) {
    return;
  }

  if (!web_contents()) {
    return;
  }

  // May be called multiple times.
  if (IsShowingContextMenu()) {
    return;
  }

  // Notify the host before showing the context menu.
  web_contents()->SetShowingContextMenu(true);
}

void CefMenuManager::MenuClosed(CefRefPtr<CefMenuModelImpl> source) {
  // May be called for sub-menus as well.
  if (source.get() != model_.get()) {
    return;
  }

  if (!web_contents()) {
    return;
  }

  DCHECK(IsShowingContextMenu());

  // Notify the client.
  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefContextMenuHandler> handler = client->GetContextMenuHandler();
    if (handler.get()) {
      handler->OnContextMenuDismissed(browser_, browser_->GetFocusedFrame());
    }
  }

  // Notify the host after closing the context menu.
  web_contents()->SetShowingContextMenu(false);
  web_contents()->NotifyContextMenuClosed(params_.link_followed);
}

bool CefMenuManager::FormatLabel(CefRefPtr<CefMenuModelImpl> source,
                                 std::u16string& label) {
  if (!runner_) {
    return false;
  }
  return runner_->FormatLabel(label);
}

void CefMenuManager::ExecuteCommandCallback(int command_id,
                                            cef_event_flags_t event_flags) {
  DCHECK(IsShowingContextMenu());
  DCHECK(custom_menu_callback_);
  if (command_id != kInvalidCommandId) {
    ExecuteCommand(model_, command_id, event_flags);
  }
  MenuClosed(model_);
  custom_menu_callback_ = nullptr;
}

void CefMenuManager::CreateDefaultModel() {
  if (!params_.custom_items.empty()) {
    // Custom menu items originating from the renderer process. For example,
    // plugin placeholder menu items.
    for (auto& item : params_.custom_items) {
      auto new_item = item->Clone();
      new_item->action += MENU_ID_CUSTOM_FIRST;
      DCHECK_LE(static_cast<int>(new_item->action), MENU_ID_CUSTOM_LAST);
      model_->AddMenuItem(*new_item);
    }
    return;
  }

  if (params_.is_editable) {
    // Editable node.
    model_->AddItem(MENU_ID_UNDO, GetLabel(IDS_CONTENT_CONTEXT_UNDO));
    model_->AddItem(MENU_ID_REDO, GetLabel(IDS_CONTENT_CONTEXT_REDO));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_CUT, GetLabel(IDS_CONTENT_CONTEXT_CUT));
    model_->AddItem(MENU_ID_COPY, GetLabel(IDS_CONTENT_CONTEXT_COPY));
    model_->AddItem(MENU_ID_PASTE, GetLabel(IDS_CONTENT_CONTEXT_PASTE));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_SELECT_ALL,
                    GetLabel(IDS_CONTENT_CONTEXT_SELECTALL));

    if (!(params_.edit_flags & CM_EDITFLAG_CAN_UNDO)) {
      model_->SetEnabled(MENU_ID_UNDO, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_REDO)) {
      model_->SetEnabled(MENU_ID_REDO, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_CUT)) {
      model_->SetEnabled(MENU_ID_CUT, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_COPY)) {
      model_->SetEnabled(MENU_ID_COPY, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_PASTE)) {
      model_->SetEnabled(MENU_ID_PASTE, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_DELETE)) {
      model_->SetEnabled(MENU_ID_DELETE, false);
    }
    if (!(params_.edit_flags & CM_EDITFLAG_CAN_SELECT_ALL)) {
      model_->SetEnabled(MENU_ID_SELECT_ALL, false);
    }

    if (!params_.misspelled_word.empty()) {
      // Always add a separator before the list of dictionary suggestions or
      // "No spelling suggestions".
      model_->AddSeparator();

      if (!params_.dictionary_suggestions.empty()) {
        for (size_t i = 0; i < params_.dictionary_suggestions.size() &&
                           MENU_ID_SPELLCHECK_SUGGESTION_0 + i <=
                               MENU_ID_SPELLCHECK_SUGGESTION_LAST;
             ++i) {
          model_->AddItem(MENU_ID_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
                          params_.dictionary_suggestions[i]);
        }

        // When there are dictionary suggestions add a separator before "Add to
        // dictionary".
        model_->AddSeparator();
      } else {
        model_->AddItem(MENU_ID_NO_SPELLING_SUGGESTIONS,
                        GetLabel(IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
        model_->SetEnabled(MENU_ID_NO_SPELLING_SUGGESTIONS, false);
      }

      model_->AddItem(MENU_ID_ADD_TO_DICTIONARY,
                      GetLabel(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY));
    }
  } else if (!params_.selection_text.empty()) {
    // Something is selected.
    model_->AddItem(MENU_ID_COPY, GetLabel(IDS_CONTENT_CONTEXT_COPY));
  } else if (!params_.page_url.is_empty() || !params_.frame_url.is_empty()) {
    // Page or frame.
    model_->AddItem(MENU_ID_BACK, GetLabel(IDS_CONTENT_CONTEXT_BACK));
    model_->AddItem(MENU_ID_FORWARD, GetLabel(IDS_CONTENT_CONTEXT_FORWARD));

    model_->AddSeparator();
    model_->AddItem(MENU_ID_PRINT, GetLabel(IDS_CONTENT_CONTEXT_PRINT));
    model_->AddItem(MENU_ID_VIEW_SOURCE,
                    GetLabel(IDS_CONTENT_CONTEXT_VIEWPAGESOURCE));

    if (!browser_->CanGoBack()) {
      model_->SetEnabled(MENU_ID_BACK, false);
    }
    if (!browser_->CanGoForward()) {
      model_->SetEnabled(MENU_ID_FORWARD, false);
    }
  }
}

void CefMenuManager::ExecuteDefaultCommand(int command_id) {
  if (IsCustomContextMenuCommand(command_id)) {
    if (web_contents()) {
      web_contents()->ExecuteCustomContextMenuCommand(
          command_id - MENU_ID_CUSTOM_FIRST, params_.link_followed);
    }
    return;
  }

  // If the user chose a replacement word for a misspelling, replace it here.
  if (command_id >= MENU_ID_SPELLCHECK_SUGGESTION_0 &&
      command_id <= MENU_ID_SPELLCHECK_SUGGESTION_LAST) {
    const size_t suggestion_index =
        static_cast<size_t>(command_id) - MENU_ID_SPELLCHECK_SUGGESTION_0;
    if (suggestion_index < params_.dictionary_suggestions.size()) {
      browser_->ReplaceMisspelling(
          params_.dictionary_suggestions[suggestion_index]);
    }
    return;
  }

  switch (command_id) {
    // Navigation.
    case MENU_ID_BACK:
      browser_->GoBack();
      break;
    case MENU_ID_FORWARD:
      browser_->GoForward();
      break;
    case MENU_ID_RELOAD:
      browser_->Reload();
      break;
    case MENU_ID_RELOAD_NOCACHE:
      browser_->ReloadIgnoreCache();
      break;
    case MENU_ID_STOPLOAD:
      browser_->StopLoad();
      break;

    // Editing.
    case MENU_ID_UNDO:
      browser_->GetFocusedFrame()->Undo();
      break;
    case MENU_ID_REDO:
      browser_->GetFocusedFrame()->Redo();
      break;
    case MENU_ID_CUT:
      browser_->GetFocusedFrame()->Cut();
      break;
    case MENU_ID_COPY:
      browser_->GetFocusedFrame()->Copy();
      break;
    case MENU_ID_PASTE:
      browser_->GetFocusedFrame()->Paste();
      break;
    case MENU_ID_DELETE:
      browser_->GetFocusedFrame()->Delete();
      break;
    case MENU_ID_SELECT_ALL:
      browser_->GetFocusedFrame()->SelectAll();
      break;

    // Miscellaneous.
    case MENU_ID_FIND:
      // TODO(cef): Implement.
      NOTIMPLEMENTED();
      break;
    case MENU_ID_PRINT:
      browser_->Print();
      break;
    case MENU_ID_VIEW_SOURCE:
      browser_->GetFocusedFrame()->ViewSource();
      break;

    // Spell checking.
    case MENU_ID_ADD_TO_DICTIONARY:
      browser_->GetHost()->AddWordToDictionary(params_.misspelled_word);
      break;

    default:
      break;
  }
}

bool CefMenuManager::IsCustomContextMenuCommand(int command_id) {
  // Verify that the command ID is in the correct range.
  if (command_id < MENU_ID_CUSTOM_FIRST || command_id > MENU_ID_CUSTOM_LAST) {
    return false;
  }

  command_id -= MENU_ID_CUSTOM_FIRST;

  // Verify that the specific command ID was passed from the renderer process.
  if (!params_.custom_items.empty()) {
    for (const auto& custom_item : params_.custom_items) {
      if (static_cast<int>(custom_item->action) == command_id) {
        return true;
      }
    }
  }
  return false;
}
