# CEF Installer Contributor Guidance

This file is a lightweight guide for changes under this directory. Do not
duplicate installer contracts here; use the following documents as the
authoritative references:

- [README.md](README.md) describes installer behavior, client integration,
  configuration, commands, result contracts, and build integration.
- [SECURITY.md](SECURITY.md) defines trust boundaries, security invariants,
  required validation, and guards for test-only behavior.
- [TESTING.md](TESTING.md) describes unit, integration, E2E, fuzz, manual, and
  coverage testing.
- [ADMIN_POLICY.md](ADMIN_POLICY.md) is the normative reference for enterprise
  policy, provisioning, precedence, and policy errors.

When behavior changes, update the owning document and its tests in the same
change. Security-sensitive changes must be checked against `SECURITY.md`,
including its testing-mode guard requirements.

## Code organization

- Production installer code belongs in `cef_installer`. Keep the interface
  exposed outside this directory small and task-oriented.
- `cef_installer::internal` is for implementation details and narrowly scoped
  test seams. It is not an API boundary. Use it only within installer
  implementation files and installer tests.
- `cef_installer::test` is exclusively for test helpers, fixtures, fakes, and
  reference implementations. Its files belong in test-only targets. Production
  code and production targets must not include, call, or depend on it.
- Do not put production behavior in `*_test_support.*`, and do not put test
  infrastructure in production files merely to make it reachable by tests.
  Keep production seams minimal; place the fake implementation and assertions
  in test-only code.
- A function named `*ForTesting` is still production code when it is compiled
  into the installer target. Put such a seam in `internal`, apply the official
  build guards required by `SECURITY.md`, and avoid using it from normal
  production control flow.
- Do not add environment-variable, command-line, registry, filesystem, timing,
  network, or process-launch bypasses ad hoc. Centralize typed test controls,
  fail closed in official builds, and document new bypasses in the Testing Mode
  Guards table in `SECURITY.md`.
- Prefer testing through public behavior. Expose an `internal` helper only when
  direct coverage is materially clearer than exercising the owning public
  operation.

Before adding a source or symbol, classify its ownership:

- Production behavior belongs in a normal installer source and in
  `cef_installer` or `cef_installer::internal` according to its consumers.
- A seam consulted by production code belongs in `internal`, uses the required
  official-build guard, and exposes only the minimum `*ForTesting` control.
- A helper, fake, fixture, or reference implementation used only by tests
  belongs in `cef_installer::test`, a clearly named test-support source, and a
  test-only target.

Check `BUILD.gn` ownership whenever adding or moving test support. A filename or
preprocessor guard does not make a source test-only: production targets must
not list `*_test_support.*` or depend on `cef_installer::test`.
