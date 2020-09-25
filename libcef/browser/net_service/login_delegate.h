// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_LOGIN_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_LOGIN_DELEGATE_H_

#include "include/cef_base.h"

#include "base/memory/weak_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/login_delegate.h"
#include "net/base/auth.h"

namespace content {
struct GlobalRequestID;
class WebContents;
}  // namespace content

class CefBrowserHostBase;
class GURL;

namespace net_service {

class LoginDelegate : public content::LoginDelegate {
 public:
  // This object will be deleted when |callback| is executed or the request is
  // canceled. |callback| should not be executed after this object is deleted.
  LoginDelegate(const net::AuthChallengeInfo& auth_info,
                content::WebContents* web_contents,
                const content::GlobalRequestID& request_id,
                const GURL& origin_url,
                LoginAuthRequiredCallback callback);

  void Continue(const CefString& username, const CefString& password);
  void Cancel();

 private:
  void Start(CefRefPtr<CefBrowserHostBase> browser,
             const net::AuthChallengeInfo& auth_info,
             const content::GlobalRequestID& request_id,
             const GURL& origin_url);

  LoginAuthRequiredCallback callback_;
  base::WeakPtrFactory<LoginDelegate> weak_ptr_factory_;
};

}  // namespace net_service

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_LOGIN_DELEGATE_H_
