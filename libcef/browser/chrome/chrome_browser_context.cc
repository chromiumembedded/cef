// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/chrome_browser_context.h"

#include <memory>

#include "base/threading/thread_restrictions.h"
#include "cef/libcef/browser/prefs/browser_prefs.h"
#include "cef/libcef/browser/thread_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/off_the_record_profile_impl.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_service.h"

namespace {

// Match the default logic from ProfileManager::GetPrimaryUserProfile which was
// restricted in https://crbug.com/1264436.
Profile* GetPrimaryUserProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // From ProfileManager::GetActiveUserOrOffTheRecordProfile.
  base::FilePath default_profile_dir = profile_manager->user_data_dir().Append(
      profile_manager->GetInitialProfileDir());
  return profile_manager->GetProfile(default_profile_dir);
}

}  // namespace

ChromeBrowserContext::ChromeBrowserContext(
    const CefRequestContextSettings& settings)
    : CefBrowserContext(settings), weak_ptr_factory_(this) {}

ChromeBrowserContext::~ChromeBrowserContext() = default;

// static
ChromeBrowserContext* ChromeBrowserContext::GetOrCreateForProfile(
    Profile* profile) {
  DCHECK(profile);

  if (auto existing_context = FromProfile(profile)) {
    return static_cast<ChromeBrowserContext*>(existing_context);
  }

  CefRequestContextSettings settings;
  if (!profile->IsOffTheRecord()) {
    // Become the primary context associated with |cache_path|.
    CefString(&settings.cache_path) = profile->GetPath().value();
  }

  auto* new_context = new ChromeBrowserContext(settings);
  new_context->Initialize();
  new_context->ProfileCreated(CreateStatus::kInitialized, profile);
  return new_context;
}

content::BrowserContext* ChromeBrowserContext::AsBrowserContext() {
  CHECK(!destroyed_);
  return profile_;
}

Profile* ChromeBrowserContext::AsProfile() {
  CHECK(!destroyed_);
  return profile_;
}

bool ChromeBrowserContext::IsInitialized() const {
  CEF_REQUIRE_UIT();
  CHECK(!destroyed_);
  return !!profile_;
}

void ChromeBrowserContext::StoreOrTriggerInitCallback(
    base::OnceClosure callback) {
  CEF_REQUIRE_UIT();
  if (IsInitialized()) {
    std::move(callback).Run();
  } else {
    init_callbacks_.emplace_back(std::move(callback));
  }
}

void ChromeBrowserContext::InitializeAsync(base::OnceClosure initialized_cb) {
  init_callbacks_.emplace_back(std::move(initialized_cb));

  CefBrowserContext::Initialize();

  if (!cache_path_.empty()) {
    auto* profile_manager = g_browser_process->profile_manager();
    const auto& user_data_dir = profile_manager->user_data_dir();

    if (cache_path_ == user_data_dir) {
      // Use the default disk-based profile.
      auto profile = GetPrimaryUserProfile();
      ProfileCreated(CreateStatus::kInitialized, profile);
      return;
    } else if (cache_path_.DirName() == user_data_dir) {
      // Create or load a specific disk-based profile. May continue
      // synchronously or asynchronously.
      profile_manager->CreateProfileAsync(
          cache_path_,
          base::BindOnce(&ChromeBrowserContext::ProfileCreated,
                         weak_ptr_factory_.GetWeakPtr(),
                         CreateStatus::kInitialized),
          base::BindOnce(&ChromeBrowserContext::ProfileCreated,
                         weak_ptr_factory_.GetWeakPtr(),
                         CreateStatus::kCreated));
      return;
    } else {
      // All profile directories must be relative to |user_data_dir|.
      LOG(ERROR) << "Cannot create profile at path "
                 << cache_path_.AsUTF8Unsafe();
    }
  }

  // Default to creating a new/unique OffTheRecord profile.
  ProfileCreated(CreateStatus::kDefault, nullptr);
}

void ChromeBrowserContext::Shutdown() {
  CefBrowserContext::Shutdown();

  // Allow potential deletion of the Profile at some future point (controlled
  // by ProfileManager).
  profile_keep_alive_.reset();

  // |g_browser_process| may be nullptr during shutdown.
  if (g_browser_process) {
    if (should_destroy_) {
      GetPrimaryUserProfile()->DestroyOffTheRecordProfile(
          profile_.ExtractAsDangling());
    } else if (profile_) {
      OnProfileWillBeDestroyed(profile_);
    }
  }
}

void ChromeBrowserContext::AddVisitedURLs(
    const GURL& url,
    const std::vector<GURL>& redirect_chain,
    ui::PageTransition transition) {
  auto* profile = AsProfile();
  if (profile->IsOffTheRecord()) {
    // Don't persist state.
    return;
  }

  // Called from DidFinishNavigation by Alloy style browsers. Chrome style
  // browsers will handle this via HistoryTabHelper.
  if (auto history_service = HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS)) {
    history::HistoryAddPageArgs add_page_args;
    add_page_args.url = url;
    add_page_args.redirects = redirect_chain;
    add_page_args.transition = transition;
    add_page_args.time = base::Time::Now();
    history_service->AddPage(std::move(add_page_args));
  }
}

void ChromeBrowserContext::ProfileCreated(CreateStatus status,
                                          Profile* profile) {
  Profile* parent_profile = nullptr;
  OffTheRecordProfileImpl* otr_profile = nullptr;

  if (status != CreateStatus::kCreated &&
      status != CreateStatus::kInitialized) {
    CHECK(!profile);
    CHECK(!profile_);

    // Profile creation may access the filesystem.
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Creation of a disk-based profile failed for some reason. Create a
    // new/unique OffTheRecord profile instead.
    const auto& profile_id = Profile::OTRProfileID::CreateUniqueForCEF();
    parent_profile = GetPrimaryUserProfile();
    profile_ = parent_profile->GetOffTheRecordProfile(
        profile_id, /*create_if_needed=*/true);
    otr_profile = static_cast<OffTheRecordProfileImpl*>(profile_);
    status = CreateStatus::kInitialized;
    should_destroy_ = true;
  } else if (profile && !profile_) {
    // May be CREATE_STATUS_CREATED or CREATE_STATUS_INITIALIZED since
    // *CREATED isn't always sent for a disk-based profile that already
    // exists.
    profile_ = profile;
    profile_->AddObserver(this);
    if (!profile_->IsOffTheRecord()) {
      profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
          profile_, ProfileKeepAliveOrigin::kAppWindow);
    }
  }

  if (status == CreateStatus::kInitialized) {
    CHECK(profile_);

    // Must set |profile_| before Init() calls
    // ChromeContentBrowserClientCef::ConfigureNetworkContextParams so that
    // CefBrowserContext::FromBrowserContext can find us.
    if (otr_profile) {
      otr_profile->Init();
      parent_profile->NotifyOffTheRecordProfileCreated(otr_profile);
    }

    if (!profile_->IsOffTheRecord()) {
      // Configure the desired profile restore behavior for the next application
      // restart (checked via ProfileImpl::ShouldRestoreOldSessionCookies).
      profile_->GetPrefs()->SetInteger(
          prefs::kRestoreOnStartup, !!settings_.persist_session_cookies
                                        ? SessionStartupPref::kPrefValueLast
                                        : SessionStartupPref::kPrefValueNewTab);
    }

    browser_prefs::SetInitialProfilePrefs(profile_);

    if (!init_callbacks_.empty()) {
      for (auto& callback : init_callbacks_) {
        std::move(callback).Run();
      }
      init_callbacks_.clear();
    }
  }
}

void ChromeBrowserContext::OnProfileWillBeDestroyed(Profile* profile) {
  CHECK_EQ(profile_, profile);
  profile_->RemoveObserver(this);
  profile_ = nullptr;
  destroyed_ = true;
}
