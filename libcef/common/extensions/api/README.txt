This directory provides Mojo API definitions for CEF.

To add a new Mojo API:

  <api> is the name of the API definition (e.g. 'streams_private').
  <class> is the name of the class implementation (e.g. 'StreamsPrivateAPI').

1. Add libcef/common/extensions/api/<api>.idl file which defines the API.
2. Add <api>.idl to the 'schema_files' list in 
   libcef/common/extensions/api/schemas.gypi. Serialization code will be
   generated based on this list in step 5.
3. Add an entry to libcef/common/extensions/api/_api_features.json if
   necessary [1].
4. Add libcef/browser/extensions/api/<api>/<api>_api.[h|cc] class implementation
   files and associated entries to the 'libcef_static' target in cef.gyp.
5. Run the cef_create_projects script and build to generate the
   cef/libcef/common/extensions/api/<api>.h file and other serialization code
   required by the extensions system.
6. Call `<class>::GetInstance();` or `<class>Factory::GetFactoryInstance();` [2]
   from EnsureBrowserContextKeyedServiceFactoriesBuilt in
   libcef/browser/extensions/browser_context_keyed_service_factories.cc.
7. Call `DependsOn(<class>Factory::GetInstance());` from
   CefExtensionSystemFactory::CefExtensionSystemFactory in
   libcef/browser/extensions/extension_system_factory.cc if necessary [2].

See https://www.chromium.org/developers/design-documents/mojo for more
information.

[1] A feature can optionally express requirements for where it can be accessed.
    See the _api_features.json file for additional details.

[2] Some Mojo APIs use singleton Factory objects that create a one-to-one
    relationship between a service and a BrowserContext. This is used primarily
    to control shutdown/destruction order and implementors must explicitly state
    which services are depended on. See comments in
    components/keyed_service/content/browser_context_keyed_service_factory.h
    for additional details.
