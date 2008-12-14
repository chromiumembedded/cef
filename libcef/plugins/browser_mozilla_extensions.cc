// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "browser_mozilla_extensions.h"
#include "browser_plugin_instance.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_service.h"
#include "net/proxy/proxy_resolver_winhttp.h"
#include "third_party/npapi/bindings/npapi.h"
#include "webkit/glue/webkit_glue.h"

#define QI_SUPPORTS_IID(iid, iface)                                           \
  QI_SUPPORTS_IID_(iid, iface::GetIID(), iface)

#define QI_SUPPORTS_IID_(src_iid, iface_iid, iface)                           \
  if (iid.Equals(iface_iid)) {                                                \
    AddRef();                                                                 \
    *result = static_cast<iface*>(this);                                      \
    return NS_OK;                                                             \
  }

namespace NPAPI
{

void BrowserMozillaExtensionApi::DetachFromInstance() {
  plugin_instance_ = NULL;
}

bool BrowserMozillaExtensionApi::FindProxyForUrl(const char* url,
                                                 std::string* proxy) {
  bool result = false;

  if ((!url) || (!proxy)) {
    NOTREACHED();
    return result;
  }

  scoped_ptr<net::ProxyService> proxy_service(net::ProxyService::Create(NULL));
  if (!proxy_service.get()) {
    NOTREACHED();
    return result;
  }

  net::ProxyInfo proxy_info;
  if (proxy_service->ResolveProxy(GURL(std::string(url)),
                                 &proxy_info,
                                 NULL,
                                 NULL) == net::OK) {
    if (!proxy_info.is_direct()) {
      std::string winhttp_proxy = proxy_info.proxy_server();

      // Winhttp returns proxy in the the following format:
      // - HTTP proxy: "111.111.111.111:11"
      // -.SOCKS proxy: "socks=111.111.111.111:11"
      // - Mixed proxy: "http=111.111.111.111:11; socks=222.222.222.222:22"
      // 
      // We need to translate this into the following format:
      // i)   "DIRECT"  -- no proxy
      // ii)  "PROXY xxx.xxx.xxx.xxx"   -- use proxy
      // iii) "SOCKS xxx.xxx.xxx.xxx"  -- use SOCKS
      // iv)  Mixed. e.g. "PROXY 111.111.111.111;PROXY 112.112.112.112",
      //                  "PROXY 111.111.111.111;SOCKS 112.112.112.112"....
      StringToLowerASCII(winhttp_proxy);
      if (std::string::npos == winhttp_proxy.find('=')) {
        // Proxy is in the form: "111.111.111.111:11"
        winhttp_proxy.insert(0, "http ");
      } else {
        // Proxy is in the following form. 
        // -.SOCKS proxy: "socks=111.111.111.111:11"
        // - Mixed proxy: "http=111.111.111.111:11; socks=222.222.222.222:22"
        // in this case just replace the '=' with a space
        std::replace_if(winhttp_proxy.begin(),
                        winhttp_proxy.end(),
                        std::bind2nd(std::equal_to<char>(), '='), ' ');
      }

      *proxy = winhttp_proxy;
      result = true;
    }
  }

  return result;
}

// nsISupports implementation
NS_IMETHODIMP BrowserMozillaExtensionApi::QueryInterface(REFNSIID iid,
                                                         void** result) {
  static const nsIID knsISupportsIID = NS_ISUPPORTS_IID;
  QI_SUPPORTS_IID_(iid, knsISupportsIID, nsIServiceManager)
  QI_SUPPORTS_IID(iid, nsIServiceManager)
  QI_SUPPORTS_IID(iid, nsIPluginManager)
  QI_SUPPORTS_IID(iid, nsIPluginManager2)
  QI_SUPPORTS_IID(iid, nsICookieStorage)

  NOTREACHED();
  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP_(nsrefcnt) BrowserMozillaExtensionApi::AddRef(void) {
  return InterlockedIncrement(reinterpret_cast<LONG*>(&ref_count_));
}

NS_IMETHODIMP_(nsrefcnt) BrowserMozillaExtensionApi::Release(void) {
  DCHECK(static_cast<int>(ref_count_) > 0);
  if (InterlockedDecrement(reinterpret_cast<LONG*>(&ref_count_)) == 0) {
    delete this;
    return 0;
  }

  return ref_count_;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::GetService(REFNSIID class_guid,
                                                     REFNSIID iid,
                                                     void** result) {

  static const nsIID kPluginManagerCID = NS_PLUGINMANAGER_CID;
  static const nsIID kCookieStorageCID = NS_COOKIESTORAGE_CID;

  nsresult rv = NS_ERROR_FAILURE;

  if ((class_guid.Equals(kPluginManagerCID)) ||
      (class_guid.Equals(kCookieStorageCID))) {
    rv = QueryInterface(iid, result);
  }

  DCHECK(rv == NS_OK);
  return rv;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::GetServiceByContractID(
    const char* contract_id,
    REFNSIID iid,
    void** result) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::IsServiceInstantiated(REFNSIID class_guid,
                                                                REFNSIID iid,
                                                                PRBool* result) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::IsServiceInstantiatedByContractID(
    const char* contract_id,
    REFNSIID iid,
    PRBool* result) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP BrowserMozillaExtensionApi::GetValue(nsPluginManagerVariable variable,
                                                   void * value) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::ReloadPlugins(PRBool reloadPages) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::UserAgent(
    const char** resultingAgentString) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::GetURL(
    nsISupports* pluginInst,
    const char* url,
    const char* target,
    nsIPluginStreamListener* streamListener,
    const char* altHost,
    const char* referrer,
    PRBool forceJSEnabled) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::PostURL(
    nsISupports* pluginInst,
    const char* url,
    unsigned int postDataLen,
    const char* postData,
    PRBool isFile,
    const char* target,
    nsIPluginStreamListener* streamListener,
    const char* altHost,
    const char* referrer,
    PRBool forceJSEnabled ,
    unsigned int postHeadersLength,
    const char* postHeaders) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::RegisterPlugin(
    REFNSIID aCID,
    const char *aPluginName,
    const char *aDescription,
    const char * * aMimeTypes,
    const char * * aMimeDescriptions,
    const char * * aFileExtensions,
    PRInt32 aCount) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::UnregisterPlugin(REFNSIID aCID) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::GetURLWithHeaders(
    nsISupports* pluginInst,
    const char* url,
    const char* target /* = NULL */,
    nsIPluginStreamListener* streamListener /* = NULL */,
    const char* altHost /* = NULL */,
    const char* referrer /* = NULL */,
    PRBool forceJSEnabled /* = PR_FALSE */,
    PRUint32 getHeadersLength /* = 0 */,
    const char* getHeaders /* = NULL */){
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

// nsIPluginManager2
NS_IMETHODIMP BrowserMozillaExtensionApi::BeginWaitCursor() {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::EndWaitCursor() {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::SupportsURLProtocol(const char* aProtocol,
                                                              PRBool* aResult) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::NotifyStatusChange(nsIPlugin* aPlugin,
                                                             nsresult aStatus) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::FindProxyForURL(
    const char* aURL,
    char** aResult) {
  std::string proxy = "DIRECT";
  FindProxyForUrl(aURL, &proxy);

  // Allocate this using the NPAPI allocator. The plugin will call 
  // NPN_Free to free this.
  char* result = static_cast<char*>(NPN_MemAlloc(proxy.length() + 1));
  strncpy(result, proxy.c_str(), proxy.length() + 1);

  *aResult = result;
  return NS_OK;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::RegisterWindow(
    nsIEventHandler* handler,
    nsPluginPlatformWindowRef window) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::UnregisterWindow(
    nsIEventHandler* handler,
    nsPluginPlatformWindowRef win) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::AllocateMenuID(nsIEventHandler* aHandler,
                                                         PRBool aIsSubmenu,
                                                         PRInt16 *aResult) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::DeallocateMenuID(nsIEventHandler* aHandler,
                                                           PRInt16 aMenuID) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::HasAllocatedMenuID(nsIEventHandler* aHandler,
                                                             PRInt16 aMenuID,
                                                             PRBool* aResult) {
  NOTREACHED();
  return NS_ERROR_FAILURE;
}

// nsICookieStorage
NS_IMETHODIMP BrowserMozillaExtensionApi::GetCookie(
    const char* url,
    void* cookie_buffer,
    PRUint32& buffer_size) {
  if ((!url) || (!cookie_buffer)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!plugin_instance_)
    return NS_ERROR_FAILURE;

  WebPlugin* webplugin = plugin_instance_->webplugin();
  if (!webplugin)
    return NS_ERROR_FAILURE;

  // Bypass third-party cookie blocking by using the url as the policy_url.
  GURL cookies_url((std::string(url)));
  std::string cookies = webplugin->GetCookies(cookies_url, cookies_url);

  if (cookies.empty())
    return NS_ERROR_FAILURE;

  if(cookies.length() >= buffer_size)
    return NS_ERROR_FAILURE;

  strncpy(static_cast<char*>(cookie_buffer),
          cookies.c_str(),
          cookies.length() + 1);

  buffer_size = cookies.length();
  return NS_OK;
}

NS_IMETHODIMP BrowserMozillaExtensionApi::SetCookie(
    const char* url,
    const void* cookie_buffer,
    PRUint32 buffer_size){
  if ((!url) || (!cookie_buffer) || (!buffer_size)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!plugin_instance_)
    return NS_ERROR_FAILURE;

  WebPlugin* webplugin = plugin_instance_->webplugin();
  if (!webplugin)
    return NS_ERROR_FAILURE;

  std::string cookie(static_cast<const char*>(cookie_buffer), 
                     buffer_size);
  GURL cookies_url((std::string(url)));
  webplugin->SetCookie(cookies_url,
                       cookies_url,
                       cookie);
  return NS_OK;
}


} // namespace NPAPI

