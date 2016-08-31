// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCEF_BROWSER_COMPONENT_UPDATER_CEF_COMPONENT_UPDATER_CONFIGURATOR_H_
#define LIBCEF_BROWSER_COMPONENT_UPDATER_CEF_COMPONENT_UPDATER_CONFIGURATOR_H_

#include "base/memory/ref_counted.h"
#include "components/update_client/configurator.h"

namespace base {
class CommandLine;
}

namespace net {
class URLRequestContextGetter;
}

class PrefRegistrySimple;
class PrefService;

namespace component_updater {

// Registers preferences associated with the component updater configurator
// for CEF. The preferences must be registered with the local pref store
// before they can be queried by the configurator instance.
// This function is called before MakeCefComponentUpdaterConfigurator.
void RegisterPrefsForCefComponentUpdaterConfigurator(
    PrefRegistrySimple* registry);

scoped_refptr<update_client::Configurator>
MakeCefComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    net::URLRequestContextGetter* context_getter,
    PrefService* pref_service);

}  // namespace component_updater

#endif  // LIBCEF_BROWSER_COMPONENT_UPDATER_CEF_COMPONENT_UPDATER_CONFIGURATOR_H_
