// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/browser_security_policy_impl.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

CefRefPtr<CefBrowserSecurityPolicyImpl> CreatePolicyWithDomains(
    const char* domains) {
  CefRefPtr<CefBrowserSecurityPolicyImpl> policy =
      new CefBrowserSecurityPolicyImpl();
  CefBrowserSecuritySettings settings;
  CefString(&settings.allowed_domains).FromASCII(domains);
  policy->SetSettings(settings, nullptr);
  return policy;
}

}  // namespace

TEST(SecurityPolicy, NoDomainsAllowsAll) {
  CefRefPtr<CefBrowserSecurityPolicyImpl> policy =
      new CefBrowserSecurityPolicyImpl();

  // With no allowed_domains set, nothing should be blocked.
  EXPECT_FALSE(policy->ShouldBlockRequest(GURL("https://example.com")));
  EXPECT_FALSE(policy->ShouldBlockRequest(GURL("https://any-site.org/path")));
  EXPECT_FALSE(
      policy->ShouldBlockRequest(GURL("http://localhost:8080/api")));
}

TEST(SecurityPolicy, ExactDomainMatch) {
  auto policy = CreatePolicyWithDomains("example.com");

  // Exact match should be allowed (not blocked).
  EXPECT_FALSE(
      policy->ShouldBlockRequest(GURL("https://example.com/page")));

  // Different domain should be blocked.
  EXPECT_TRUE(
      policy->ShouldBlockRequest(GURL("https://other.com/page")));

  // Subdomain should also be blocked (exact match, not wildcard).
  EXPECT_TRUE(
      policy->ShouldBlockRequest(GURL("https://sub.example.com/page")));
}

TEST(SecurityPolicy, WildcardSubdomain) {
  auto policy = CreatePolicyWithDomains(".example.com");

  // The bare domain should be allowed.
  EXPECT_FALSE(
      policy->ShouldBlockRequest(GURL("https://example.com/page")));

  // Subdomains should be allowed.
  EXPECT_FALSE(
      policy->ShouldBlockRequest(GURL("https://sub.example.com/page")));
  EXPECT_FALSE(policy->ShouldBlockRequest(
      GURL("https://deep.sub.example.com/page")));

  // Unrelated domain should be blocked.
  EXPECT_TRUE(
      policy->ShouldBlockRequest(GURL("https://other.com/page")));

  // A domain that merely ends with the same suffix but is different.
  EXPECT_TRUE(
      policy->ShouldBlockRequest(GURL("https://notexample.com/page")));
}

TEST(SecurityPolicy, MultipleDomains) {
  auto policy = CreatePolicyWithDomains("a.com,b.com");

  EXPECT_FALSE(policy->ShouldBlockRequest(GURL("https://a.com/")));
  EXPECT_FALSE(policy->ShouldBlockRequest(GURL("https://b.com/")));
  EXPECT_TRUE(policy->ShouldBlockRequest(GURL("https://c.com/")));
}

TEST(SecurityPolicy, IsDomainAllowed) {
  auto policy = CreatePolicyWithDomains("allowed.com,.wild.org");

  EXPECT_TRUE(policy->IsDomainAllowed("allowed.com"));
  EXPECT_FALSE(policy->IsDomainAllowed("blocked.com"));

  // Wildcard domain checks.
  EXPECT_TRUE(policy->IsDomainAllowed("wild.org"));
  EXPECT_TRUE(policy->IsDomainAllowed("sub.wild.org"));
  EXPECT_FALSE(policy->IsDomainAllowed("notwild.org"));

  // No domains configured: everything should be allowed.
  CefRefPtr<CefBrowserSecurityPolicyImpl> open_policy =
      new CefBrowserSecurityPolicyImpl();
  EXPECT_TRUE(open_policy->IsDomainAllowed("anything.com"));
}

TEST(SecurityPolicy, RecompileOnSetSettings) {
  CefRefPtr<CefBrowserSecurityPolicyImpl> policy =
      new CefBrowserSecurityPolicyImpl();

  // Initially no restrictions.
  EXPECT_FALSE(
      policy->ShouldBlockRequest(GURL("https://example.com/")));

  // Set allowed domains to restrict.
  {
    CefBrowserSecuritySettings settings;
    CefString(&settings.allowed_domains).FromASCII("only-this.com");
    policy->SetSettings(settings, nullptr);
  }

  EXPECT_FALSE(
      policy->ShouldBlockRequest(GURL("https://only-this.com/")));
  EXPECT_TRUE(
      policy->ShouldBlockRequest(GURL("https://example.com/")));

  // Change to a different set of allowed domains.
  {
    CefBrowserSecuritySettings settings;
    CefString(&settings.allowed_domains).FromASCII("example.com");
    policy->SetSettings(settings, nullptr);
  }

  EXPECT_FALSE(
      policy->ShouldBlockRequest(GURL("https://example.com/")));
  EXPECT_TRUE(
      policy->ShouldBlockRequest(GURL("https://only-this.com/")));

  // Clear all domain restrictions.
  {
    CefBrowserSecuritySettings settings;
    policy->SetSettings(settings, nullptr);
  }

  EXPECT_FALSE(
      policy->ShouldBlockRequest(GURL("https://example.com/")));
  EXPECT_FALSE(
      policy->ShouldBlockRequest(GURL("https://only-this.com/")));
}
