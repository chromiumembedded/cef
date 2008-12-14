// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_MOZILLA_EXTENSIONS_H
#define _BROWSER_MOZILLA_EXTENSIONS_H

#include <string>

// Include npapi first to avoid definition clashes due to 
// XP_WIN
#include "third_party/npapi/bindings/npapi.h"

#include "third_party/mozilla/include/nsIServiceManager.h"
#include "third_party/mozilla/include/nsIPluginManager2.h"
#include "third_party/mozilla/include/nsICookieStorage.h"
#include "third_party/mozilla/include/nsError.h"

#include "base/ref_counted.h"

// NS_DECL_NSIPLUGINMANAGER doesn't include methods described as "C++" in the
// nsIPluginManager.idl. 
#define NS_DECL_NSIPLUGINMANAGER_FIXED                                        \
  NS_DECL_NSIPLUGINMANAGER                                                    \
    NS_IMETHOD                                                                \
    GetURL(nsISupports* pluginInst,                                           \
           const char* url,                                                   \
           const char* target = NULL,                                         \
           nsIPluginStreamListener* streamListener = NULL,                    \
           const char* altHost = NULL,                                        \
           const char* referrer = NULL,                                       \
           PRBool forceJSEnabled = PR_FALSE);                                 \
    NS_IMETHOD                                                                \
    PostURL(nsISupports* pluginInst,                                          \
            const char* url,                                                  \
            PRUint32 postDataLen,                                             \
            const char* postData,                                             \
            PRBool isFile = PR_FALSE,                                         \
            const char* target = NULL,                                        \
            nsIPluginStreamListener* streamListener = NULL,                   \
            const char* altHost = NULL,                                       \
            const char* referrer = NULL,                                      \
            PRBool forceJSEnabled = PR_FALSE,                                 \
            PRUint32 postHeadersLength = 0,                                   \
            const char* postHeaders = NULL);                                  \
    NS_IMETHOD                                                                \
    GetURLWithHeaders(nsISupports* pluginInst,                                \
                      const char* url,                                        \
                      const char* target = NULL,                              \
                      nsIPluginStreamListener* streamListener = NULL,         \
                      const char* altHost = NULL,                             \
                      const char* referrer = NULL,                            \
                      PRBool forceJSEnabled = PR_FALSE,                       \
                      PRUint32 getHeadersLength = 0,                          \
                      const char* getHeaders = NULL);

// Avoid dependence on the nsIsupportsImpl.h and so on.
#ifndef NS_DECL_ISUPPORTS
#define NS_DECL_ISUPPORTS                                                     \
  NS_IMETHOD QueryInterface(REFNSIID aIID,                                    \
                            void** aInstancePtr);                             \
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                         \
  NS_IMETHOD_(nsrefcnt) Release(void);
#endif // NS_DECL_ISUPPORTS

namespace NPAPI
{

class BrowserPluginInstance;

// Implementation of extended Mozilla interfaces needed to support 
// Sun's new Java plugin.
class BrowserMozillaExtensionApi : public nsIServiceManager,
                                   public nsIPluginManager2,
                                   public nsICookieStorage {
 public:
  BrowserMozillaExtensionApi(BrowserPluginInstance* plugin_instance) :
      plugin_instance_(plugin_instance), ref_count_(0) {
  }

  void DetachFromInstance();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEMANAGER
  NS_DECL_NSIPLUGINMANAGER_FIXED
  NS_DECL_NSIPLUGINMANAGER2
  NS_DECL_NSICOOKIESTORAGE

 protected:
  bool FindProxyForUrl(const char* url, std::string* proxy);

 protected:
  scoped_refptr<NPAPI::BrowserPluginInstance> plugin_instance_;
  unsigned long ref_count_;
};

} // namespace NPAPI

#endif  // _BROWSER_MOZILLA_EXTENSIONS_H

