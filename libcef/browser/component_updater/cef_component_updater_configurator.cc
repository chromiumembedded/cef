// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/component_updater/cef_component_updater_configurator.h"
#include "include/cef_version.h"

#include "base/version.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/configurator_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/component_patcher_operation.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {

namespace {

class CefConfigurator : public update_client::Configurator {
 public:
  CefConfigurator(const base::CommandLine* cmdline,
                  net::URLRequestContextGetter* url_request_getter,
                  PrefService* pref_service);

  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int StepDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  std::string ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  net::URLRequestContextGetter* RequestContext() const override;
  scoped_refptr<update_client::OutOfProcessPatcher>
      CreateOutOfProcessPatcher() const override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const override;
  PrefService* GetPrefService() const override;

 private:
  friend class base::RefCountedThreadSafe<CefConfigurator>;

  ~CefConfigurator() override {}

  ConfiguratorImpl configurator_impl_;
  PrefService* pref_service_;
};

CefConfigurator::CefConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter,
    PrefService* pref_service)
    : configurator_impl_(cmdline, url_request_getter, false),
      pref_service_(pref_service) {
  DCHECK(pref_service_);
}

int CefConfigurator::InitialDelay() const {
  return configurator_impl_.InitialDelay();
}

int CefConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

int CefConfigurator::StepDelay() const {
  return configurator_impl_.StepDelay();
}

int CefConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

int CefConfigurator::UpdateDelay() const {
  return configurator_impl_.UpdateDelay();
}

std::vector<GURL> CefConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> CefConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

base::Version CefConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string CefConfigurator::GetChannel() const {
  return std::string();
}

std::string CefConfigurator::GetBrand() const {
  return std::string();
}

std::string CefConfigurator::GetLang() const {
  return std::string();
}

std::string CefConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

std::string CefConfigurator::ExtraRequestParams() const {
  return configurator_impl_.ExtraRequestParams();
}

std::string CefConfigurator::GetDownloadPreference() const {
  return std::string();
}

net::URLRequestContextGetter* CefConfigurator::RequestContext() const {
  return configurator_impl_.RequestContext();
}

scoped_refptr<update_client::OutOfProcessPatcher>
CefConfigurator::CreateOutOfProcessPatcher() const {
  return nullptr;
}

bool CefConfigurator::EnabledDeltas() const {
  return configurator_impl_.EnabledDeltas();
}

bool CefConfigurator::EnabledComponentUpdates() const {
  return pref_service_->GetBoolean(prefs::kComponentUpdatesEnabled);
}

bool CefConfigurator::EnabledBackgroundDownloader() const {
  return configurator_impl_.EnabledBackgroundDownloader();
}

bool CefConfigurator::EnabledCupSigning() const {
  return configurator_impl_.EnabledCupSigning();
}

// Returns a task runner to run blocking tasks. The task runner continues to run
// after the browser shuts down, until the OS terminates the process. This
// imposes certain requirements for the code using the task runner, such as
// not accessing any global browser state while the code is running.
scoped_refptr<base::SequencedTaskRunner>
CefConfigurator::GetSequencedTaskRunner() const {
  return content::BrowserThread::GetBlockingPool()
      ->GetSequencedTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::GetSequenceToken(),
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
}

PrefService* CefConfigurator::GetPrefService() const {
  return pref_service_;
}

}  // namespace

void RegisterPrefsForCefComponentUpdaterConfigurator(
    PrefRegistrySimple* registry) {
  // The component updates are enabled by default, if the preference is not set.
  registry->RegisterBooleanPref(prefs::kComponentUpdatesEnabled, true);
}

scoped_refptr<update_client::Configurator>
MakeCefComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter,
    PrefService* pref_service) {
  return new CefConfigurator(cmdline, context_getter, pref_service);
}

}  // namespace component_updater
