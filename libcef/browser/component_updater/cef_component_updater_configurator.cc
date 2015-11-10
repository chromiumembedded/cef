// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/component_updater/cef_component_updater_configurator.h"
#include "include/cef_version.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/component_updater/component_updater_url_constants.h"
#include "components/update_client/configurator.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif  // OS_WIN

using update_client::Configurator;

namespace component_updater {

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

// Debug values you can pass to --component-updater=value1,value2.
// Speed up component checking.
const char kSwitchFastUpdate[] = "fast-update";

// Add "testrequest=1" attribute to the update check request.
const char kSwitchRequestParam[] = "test-request";

// Disables pings. Pings are the requests sent to the update server that report
// the success or the failure of component install or update attempts.
extern const char kSwitchDisablePings[] = "disable-pings";

// Sets the URL for updates.
const char kSwitchUrlSource[] = "url-source";

// Disables differential updates.
const char kSwitchDisableDeltaUpdates[] = "disable-delta-updates";

#if defined(OS_WIN)
// Disables background downloads.
const char kSwitchDisableBackgroundDownloads[] = "disable-background-downloads";
#endif  // defined(OS_WIN)

// Returns true if and only if |test| is contained in |vec|.
bool HasSwitchValue(const std::vector<std::string>& vec, const char* test) {
  if (vec.empty())
    return 0;
  return (std::find(vec.begin(), vec.end(), test) != vec.end());
}

// Returns true if falling back on an alternate, unsafe, service URL is
// allowed. In the fallback case, the security of the component update relies
// only on the integrity of the CRX payloads, which is self-validating.
// This is allowed only for some of the pre-Windows Vista versions not including
// Windows XP SP3. As a side note, pings could be sent to the alternate URL too.
bool CanUseAltUrlSource() {
#if defined(OS_WIN)
  return !base::win::MaybeHasSHA256Support();
#else
  return false;
#endif  // OS_WIN
}

// If there is an element of |vec| of the form |test|=.*, returns the right-
// hand side of that assignment. Otherwise, returns an empty string.
// The right-hand side may contain additional '=' characters, allowing for
// further nesting of switch arguments.
std::string GetSwitchArgument(const std::vector<std::string>& vec,
                              const char* test) {
  if (vec.empty())
    return std::string();
  for (std::vector<std::string>::const_iterator it = vec.begin();
       it != vec.end();
       ++it) {
    const std::size_t found = it->find("=");
    if (found != std::string::npos) {
      if (it->substr(0, found) == test) {
        return it->substr(found + 1);
      }
    }
  }
  return std::string();
}

class CefConfigurator : public Configurator {
 public:
  CefConfigurator(const base::CommandLine* cmdline,
                     net::URLRequestContextGetter* url_request_getter);

  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int StepDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  std::string ExtraRequestParams() const override;
  net::URLRequestContextGetter* RequestContext() const override;
  scoped_refptr<update_client::OutOfProcessPatcher>
      CreateOutOfProcessPatcher() const override;
  bool DeltasEnabled() const override;
  bool UseBackgroundDownloader() const override;
  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const override;

 private:
  friend class base::RefCountedThreadSafe<CefConfigurator>;

  ~CefConfigurator() override {}

  net::URLRequestContextGetter* url_request_getter_;
  std::string extra_info_;
  GURL url_source_override_;
  bool fast_update_;
  bool pings_enabled_;
  bool deltas_enabled_;
  bool background_downloads_enabled_;
  bool fallback_to_alt_source_url_enabled_;
};

CefConfigurator::CefConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* url_request_getter)
    : url_request_getter_(url_request_getter),
      fast_update_(false),
      pings_enabled_(false),
      deltas_enabled_(false),
      background_downloads_enabled_(false),
      fallback_to_alt_source_url_enabled_(false) {
  // Parse comma-delimited debug flags.
  std::vector<std::string> switch_values = base::SplitString(
      cmdline->GetSwitchValueASCII(switches::kComponentUpdater),
      ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  fast_update_ = HasSwitchValue(switch_values, kSwitchFastUpdate);
  pings_enabled_ = !HasSwitchValue(switch_values, kSwitchDisablePings);
  deltas_enabled_ = !HasSwitchValue(switch_values, kSwitchDisableDeltaUpdates);

  // TODO(dberger): Pull this (and possibly the various hard-coded
  // delay params in this file) from cef settings.
  fast_update_ = true;

#if defined(OS_WIN)
  background_downloads_enabled_ =
      !HasSwitchValue(switch_values, kSwitchDisableBackgroundDownloads);
#else
  background_downloads_enabled_ = false;
#endif

  const std::string switch_url_source =
      GetSwitchArgument(switch_values, kSwitchUrlSource);
  if (!switch_url_source.empty()) {
    url_source_override_ = GURL(switch_url_source);
    DCHECK(url_source_override_.is_valid());
  }

  if (HasSwitchValue(switch_values, kSwitchRequestParam))
    extra_info_ += "testrequest=\"1\"";

  fallback_to_alt_source_url_enabled_ = CanUseAltUrlSource();
}

int CefConfigurator::InitialDelay() const {
  return fast_update_ ? 10 : (6 * kDelayOneMinute);
}

int CefConfigurator::NextCheckDelay() const {
  return fast_update_ ? 60 : (6 * kDelayOneHour);
}

int CefConfigurator::StepDelay() const {
  return fast_update_ ? 1 : 1;
}

int CefConfigurator::OnDemandDelay() const {
  return fast_update_ ? 2 : (30 * kDelayOneMinute);
}

int CefConfigurator::UpdateDelay() const {
  return fast_update_ ? 10 : (15 * kDelayOneMinute);
}

std::vector<GURL> CefConfigurator::UpdateUrl() const {
  std::vector<GURL> urls;
  if (url_source_override_.is_valid()) {
    urls.push_back(GURL(url_source_override_));
  } else {
    urls.push_back(GURL(kUpdaterDefaultUrl));
    if (fallback_to_alt_source_url_enabled_) {
      urls.push_back(GURL(kUpdaterAltUrl));
    }
  }
  return urls;
}

std::vector<GURL> CefConfigurator::PingUrl() const {
  return pings_enabled_ ? UpdateUrl() : std::vector<GURL>();
}

base::Version CefConfigurator::GetBrowserVersion() const {
  return base::Version(base::StringPrintf("%d.%d.%d.%d",
                                          CHROME_VERSION_MAJOR,
                                          CHROME_VERSION_MINOR,
                                          CHROME_VERSION_BUILD,
                                          CHROME_VERSION_PATCH));
}

std::string CefConfigurator::GetChannel() const {
  return "";
}

std::string CefConfigurator::GetLang() const {
  return "";
}

std::string CefConfigurator::GetOSLongName() const {
#if defined(OS_WIN)
  return "Windows";
#elif defined(OS_MACOSX)
  return "Mac OS X";
#elif defined(OS_CHROMEOS)
  return "Chromium OS";
#elif defined(OS_ANDROID)
  return "Android";
#elif defined(OS_LINUX)
  return "Linux";
#elif defined(OS_FREEBSD)
  return "FreeBSD";
#elif defined(OS_OPENBSD)
  return "OpenBSD";
#elif defined(OS_SOLARIS)
  return "Solaris";
#else
  return "Unknown";
#endif
}

std::string CefConfigurator::ExtraRequestParams() const {
  return extra_info_;
}

net::URLRequestContextGetter* CefConfigurator::RequestContext() const {
  return url_request_getter_;
}

scoped_refptr<update_client::OutOfProcessPatcher>
CefConfigurator::CreateOutOfProcessPatcher() const {
  return NULL;
}

bool CefConfigurator::DeltasEnabled() const {
  return deltas_enabled_;
}

bool CefConfigurator::UseBackgroundDownloader() const {
  return background_downloads_enabled_;
}

scoped_refptr<base::SequencedTaskRunner>
CefConfigurator::GetSequencedTaskRunner() const {
  return content::BrowserThread::GetBlockingPool()
      ->GetSequencedTaskRunnerWithShutdownBehavior(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

}  // namespace

scoped_refptr<update_client::Configurator>
MakeCefComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter) {
  return new CefConfigurator(cmdline, context_getter);
}

}  // namespace component_updater
