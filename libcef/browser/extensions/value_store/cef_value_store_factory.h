// Copyright 2017 The Chromium Embedded Framework Authors.
// Portions copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_VALUE_STORE_CEF_VALUE_STORE_FACTORY_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_VALUE_STORE_CEF_VALUE_STORE_FACTORY_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "components/value_store/value_store_factory.h"

namespace value_store {

class ValueStore;

// Based on TestValueStoreFactory. Will either open a database on disk (if path
// provided) returning a |LeveldbValueStore|. Otherwise a new |CefingValueStore|
// instance will be returned.
class CefValueStoreFactory : public ValueStoreFactory {
 public:
  CefValueStoreFactory();
  explicit CefValueStoreFactory(const base::FilePath& db_path);
  CefValueStoreFactory(const CefValueStoreFactory&) = delete;
  CefValueStoreFactory& operator=(const CefValueStoreFactory&) = delete;

  // ValueStoreFactory
  std::unique_ptr<ValueStore> CreateValueStore(
      const base::FilePath& directory,
      const std::string& uma_client_name) override;
  void DeleteValueStore(const base::FilePath& directory) override;
  bool HasValueStore(const base::FilePath& directory) override;

  // Return the last created |ValueStore|. Use with caution as this may return
  // a dangling pointer since the creator now owns the ValueStore which can be
  // deleted at any time.
  ValueStore* LastCreatedStore() const;
  // Return the previously created |ValueStore| in the given directory.
  ValueStore* GetExisting(const base::FilePath& directory) const;
  // Reset this class (as if just created).
  void Reset();

 private:
  ~CefValueStoreFactory() override;

  std::unique_ptr<ValueStore> CreateStore();

  base::FilePath db_path_;
  ValueStore* last_created_store_ = nullptr;

  // A mapping from directories to their ValueStore. None of these value
  // stores are owned by this factory, so care must be taken when calling
  // GetExisting.
  std::map<base::FilePath, ValueStore*> value_store_map_;
};

}  // namespace value_store

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_VALUE_STORE_CEF_VALUE_STORE_FACTORY_H_
