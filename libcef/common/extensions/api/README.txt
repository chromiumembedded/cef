This directory provides API definitions for CEF. Some extensions are implemented
using Mojo and others use an older JSON-based format.

  <api> is the name of the API definition (e.g. 'streams_private').
  <class> is the name of the class implementation (e.g. 'StreamsPrivateAPI').

To add a new extension API implemented only in CEF ***:

1. Add libcef/common/extensions/api/<api>.idl or .json file which defines the
   API.
2. Add <api>.idl or .json to the 'schema_sources' list in
   libcef/common/extensions/api/BUILD.gn. Serialization code will be
   generated based on this list in step 4.
3. Add libcef/browser/extensions/api/<api>/<api>_api.[h|cc] class implementation
   files and associated entries to the 'libcef_static' target in BUILD.gn.
4. Run the cef_create_projects script and build to generate the
   cef/libcef/common/extensions/api/<api>.h file and other serialization code
   required by the extensions system.
5. Add an entry in the libcef/common/extensions/api/_*_features.json files if
   necessary [1].
6. Add an entry in the libcef/common/extensions/api/*_manifest_overlay.json
   files if necessary [2].
7. Call `<class>::GetInstance();` or `<class>Factory::GetFactoryInstance();` [3]
   from EnsureBrowserContextKeyedServiceFactoriesBuilt in
   libcef/browser/extensions/browser_context_keyed_service_factories.cc.
8. Call `DependsOn(<class>Factory::GetInstance());` from
   CefExtensionSystemFactory::CefExtensionSystemFactory in
   libcef/browser/extensions/extension_system_factory.cc if necessary [3].

*** Note that CEF does not currently expose its own Mojo APIs. Related code is
commented out in:
  cef/BUILD.gn
  cef/libcef/common/extensions/api/BUILD.gn
  CefExtensionsBrowserClient::RegisterExtensionFunctions
  CefExtensionsClient::IsAPISchemaGenerated
  CefExtensionsClient::GetAPISchema

To add a new extension API implemented in Chrome:

1. Register the API in libcef/browser/extensions/chrome_api_registration.cc
2. Perform steps 5 through 8 above.

See https://www.chromium.org/developers/design-documents/mojo for more
information.

[1] A feature can optionally express requirements for where it can be accessed.
    See the _api_features.json and _permission_features.json files for
    additional details. For Chrome extensions this should match the definitions
    in the chrome/common/extensions/api/_*_features.json files.

[2] Service Manifest InterfaceProviderSpecs control interfaces exposed between
    processes. Mojo interfaces exposed at the frame level are controlled by the
    "navigation:frame" dictionary. Those exposed at the process level are
    controlled by the "service_manager:connector" dictionary. Failure to specify
    this correctly may result in a console error like the following:

      InterfaceProviderSpec "navigation:frame" prevented service:
      service:content_renderer from binding interface:
      mojom::Foo exposed by: service:content_browser

[3] Some Mojo APIs use singleton Factory objects that create a one-to-one
    relationship between a service and a BrowserContext. This is used primarily
    to control shutdown/destruction order and implementors must explicitly state
    which services are depended on. See comments in
    components/keyed_service/content/browser_context_keyed_service_factory.h
    for additional details.
