# CEF C++20 Rewriter - Supported Transformations

This document lists all transformations supported by `cef_cpp_rewriter`. Each transformation can be enabled or disabled via command-line flags.

## Platform Requirements

CEF targets C++20 with the following minimum toolchain versions:

| Platform | Minimum Version | Notes |
|----------|-----------------|-------|
| GCC | 10 | CEF minimum supported |
| Xcode | 16 | Apple Clang |
| macOS deployment target | 12.0 (Monterey) | From `build/config/mac/mac_sdk.gni` |

All transformations in this document are compatible with these requirements.

## Command-Line Usage

```bash
# Run with all transformations enabled (default)
cef_cpp_rewriter file.cc --

# Disable specific transformations
cef_cpp_rewriter file.cc -- --contains=false

# Run only specific transformations
cef_cpp_rewriter file.cc -- --only=contains,structured-bindings
```

## Command-Line Flags

| Flag | Default | Description |
|------|---------|-------------|
| `--only=<list>` | (none) | Run only specified transforms (comma-separated) |
| `--contains` | true | Enable `.contains()` transformation |
| `--count-patterns` | true | Enable `count()` pattern transformation |
| `--structured-bindings` | true | Enable structured bindings transformation |
| `--iterator-loops` | true | Enable iterator loop to range-for transformation |
| `--disallow-copy` | true | Enable DISALLOW_COPY_AND_ASSIGN replacement |
| `--disable-path-filter` | false | Process all files (not just `/cef/` paths) |
| `--path-filter=<str>` | (empty) | Only process files whose path contains this substring |

All transformation flags are enabled by default. Use `--flag=false` to disable specific ones,
or use `--only=<list>` to run only specific transformations.

## Current Transformations

### `.contains()` for Associative Containers

**Flag:** `--contains` (default: true)
**Sub-flag:** `--count-patterns` (default: true)
**Compatibility:** GCC 9+, C++20

Transforms `find()/end()` and `count()` patterns to use the C++20 `.contains()` method.

**Supported Containers:**

- `std::map`, `std::multimap`
- `std::set`, `std::multiset`
- `std::unordered_map`, `std::unordered_multimap`
- `std::unordered_set`, `std::unordered_multiset`

#### Pattern: `find() != end()`

```cpp
// Before
if (map.find(key) != map.end()) { ... }
if (set.find(value) != set.end()) { ... }

// After
if (map.contains(key)) { ... }
if (set.contains(value)) { ... }
```

#### Pattern: `find() == end()` (negated)

```cpp
// Before
if (map.find(key) == map.end()) { ... }

// After
if (!map.contains(key)) { ... }
```

#### Pattern: `count() != 0` / `count() > 0`

```cpp
// Before
if (map.count(key) != 0) { ... }
if (set.count(value) > 0) { ... }

// After
if (map.contains(key)) { ... }
if (set.contains(value)) { ... }
```

#### Pattern: `count() == 0` (negated)

```cpp
// Before
if (map.count(key) == 0) { ... }

// After
if (!map.contains(key)) { ... }
```

#### Pattern: `count()` in boolean context

```cpp
// Before
if (map.count(key)) { ... }

// After
if (map.contains(key)) { ... }
```

#### Pattern: Reversed operand order

```cpp
// Before
if (map.end() != map.find(key)) { ... }
if (0 != map.count(key)) { ... }

// After
if (map.contains(key)) { ... }
if (map.contains(key)) { ... }
```

#### Pattern: Pointer access

```cpp
// Before
std::map<int, int>* ptr = &map;
if (ptr->find(key) != ptr->end()) { ... }

// After
if (ptr->contains(key)) { ... }
```

#### NOT Transformed (by design)

These patterns are intentionally **not** transformed because the iterator is used:

```cpp
// Iterator value is used - NOT transformed
auto it = map.find(key);
if (it != map.end()) {
    doSomething(it->second);  // Uses the iterator!
}

// Iterator used for erase - NOT transformed
auto it = set.find(value);
if (it != set.end()) {
    set.erase(it);  // Uses the iterator!
}

// std::string::find - NOT transformed (different semantics)
if (str.find("x") != std::string::npos) { ... }
```

#### Skipped: Code Inside Macros

The tool intentionally skips expressions inside macro expansions (e.g., `DCHECK`, `CHECK`, `LOG`). This is a conservative choice to avoid potential issues with:

- Source location complexity (spelling vs expansion locations)
- Edit offset accuracy in macro contexts
- Multiple translation unit edge cases

**Manual follow-up recommended:** After running the tool, search for remaining instances in macros and transform them manually if appropriate:

```bash
# Search for remaining find/end patterns in CEF code
grep -rn '\.find([^)]*)\s*[!=]=.*\.end()' cef/libcef cef/tests --include='*.cc' --include='*.mm'
```

---

### Structured Bindings for Range-For Loops

**Flag:** `--structured-bindings` (default: true)
**Compatibility:** GCC 7+, C++17

Transforms range-based for loops over map-like containers that access `.first`/`.second` to use structured bindings.

**Supported Containers:**

- `std::map`, `std::multimap`
- `std::unordered_map`, `std::unordered_multimap`

**Binding Names:** The tool uses `key` and `value` as binding names. Loops are skipped if the body declares local variables with these names to avoid shadowing conflicts.

#### Pattern: Basic `.first`/`.second` access

```cpp
// Before
for (const auto& pair : map) {
    use(pair.first, pair.second);
}

// After
for (const auto& [key, value] : map) {
    use(key, value);
}
```

#### Pattern: Non-const reference

```cpp
// Before
for (auto& entry : map) {
    entry.second = computeValue(entry.first);
}

// After
for (auto& [key, value] : map) {
    value = computeValue(key);
}
```

#### Pattern: Value copy

```cpp
// Before
for (auto item : map) {
    process(item.first, item.second);
}

// After
for (auto [key, value] : map) {
    process(key, value);
}
```

#### Pattern: Only `.first` or `.second` used

```cpp
// Before
for (const auto& pair : map) {
    use(pair.first);  // only .first used
}

// After (both bindings still generated)
for (const auto& [key, value] : map) {
    use(key);
}
```

#### NOT Transformed (by design)

These patterns are intentionally **not** transformed:

```cpp
// Pair used directly (not just .first/.second) - NOT transformed
for (const auto& pair : map) {
    consume(pair);  // Uses the pair object itself
}

// Iterating over vector of pairs - NOT transformed (not a map container)
std::vector<std::pair<int, int>> vec;
for (const auto& pair : vec) {
    use(pair.first, pair.second);
}

// Iterating over non-std containers - NOT transformed
base::Value::Dict dict;
for (const auto item : dict) {
    use(item.first);  // base::Value::Dict, not std::map
}

// Already using structured bindings - NOT transformed (no-op)
for (const auto& [k, v] : map) {
    use(k, v);
}

// Local variable named "key" or "value" would conflict - NOT transformed
for (const auto& pair : map) {
    std::string key = pair.first;  // Would shadow binding name
    if (key.empty()) {
        key = "default";  // Modifies local copy
    }
    use(key, pair.second);
}
```

#### Skipped: Code Inside Macros

The tool intentionally skips for-loops inside macro expansions. Manual follow-up recommended:

```bash
# Search for remaining .first/.second patterns in range-for loops
grep -rn 'for (.*auto.*:' cef/libcef cef/tests --include='*.cc' | xargs grep -l '\.first\|\.second'
```

---

### Iterator Loops to Range-For with Structured Bindings

**Flag:** `--iterator-loops` (default: true)
**Compatibility:** GCC 7+, C++17

Transforms traditional iterator-based for loops over containers with pair-like value types into range-for loops with structured bindings. This complements the structured bindings transformation above by handling iterator loops that weren't yet modernized to range-for.

**Supported Containers:**

- `std::map`, `std::multimap`
- `std::unordered_map`, `std::unordered_multimap`
- `std::vector<std::pair<K, V>>` and any container with `std::pair` as `value_type`

**Binding Names:** The tool uses `key` and `value` as binding names. Loops are skipped if the body declares local variables with these names to avoid shadowing conflicts.

#### Pattern: Basic iterator loop

```cpp
// Before
for (auto it = map.begin(); it != map.end(); ++it) {
    use(it->first, it->second);
}

// After
for (const auto& [key, value] : map) {
    use(key, value);
}
```

#### Pattern: Postfix increment

```cpp
// Before
for (auto it = map.begin(); it != map.end(); it++) {
    use(it->first, it->second);
}

// After
for (const auto& [key, value] : map) {
    use(key, value);
}
```

#### Pattern: Explicit iterator type

```cpp
// Before
for (std::map<int, int>::const_iterator it = map.begin(); it != map.end(); ++it) {
    use(it->first, it->second);
}

// After
for (const auto& [key, value] : map) {
    use(key, value);
}
```

#### Pattern: Vector of pairs

```cpp
// Before
std::vector<std::pair<int, int>> vec;
for (auto it = vec.begin(); it != vec.end(); ++it) {
    use(it->first, it->second);
}

// After
for (const auto& [key, value] : vec) {
    use(key, value);
}
```

#### Pattern: Only `->first` or `->second` used

```cpp
// Before
for (auto it = map.begin(); it != map.end(); ++it) {
    use(it->first);  // only ->first used
}

// After (both bindings still generated)
for (const auto& [key, value] : map) {
    use(key);
}
```

#### NOT Transformed (by design)

These patterns are intentionally **not** transformed:

```cpp
// Iterator used for erase - NOT transformed
for (auto it = map.begin(); it != map.end(); ) {
    if (it->first == 0) {
        it = map.erase(it);  // Uses the iterator!
    } else {
        ++it;
    }
}

// Iterator dereferenced directly - NOT transformed
for (auto it = map.begin(); it != map.end(); ++it) {
    consume(*it);  // Uses *it, not ->first/->second
}

// Iterator assigned to another variable - NOT transformed
for (auto it = map.begin(); it != map.end(); ++it) {
    auto copy = it;  // Iterator used in other ways
    use(copy->first, copy->second);
}

// Iterating over set (not pairs) - NOT transformed
for (auto it = set.begin(); it != set.end(); ++it) {
    use(*it);  // Set elements are not pairs
}

// begin() and end() on different containers - NOT transformed
for (auto it = map1.begin(); it != map2.end(); ++it) {
    use(it->first, it->second);
}

// Reverse iterator - NOT transformed
for (auto it = map.rbegin(); it != map.rend(); ++it) {
    use(it->first, it->second);
}

// Non-standard increment (e.g., std::advance) - NOT transformed
for (auto it = map.begin(); it != map.end(); std::advance(it, 1)) {
    use(it->first, it->second);
}

// No declaration in init - NOT transformed
std::map<int, int>::iterator it;
for (it = map.begin(); it != map.end(); ++it) {
    use(it->first, it->second);
}

// Local variable named "key" or "value" would conflict - NOT transformed
for (auto it = map.begin(); it != map.end(); ++it) {
    int key = it->first;  // Would shadow binding name
    use(key);
}
```

#### Skipped: Code Inside Macros

The tool intentionally skips for-loops inside macro expansions. Manual follow-up recommended:

```bash
# Search for remaining iterator loop patterns
grep -rn 'for (auto it = .*\.begin().*it != .*\.end()' cef/libcef cef/tests --include='*.cc'
```

---

### DISALLOW_COPY_AND_ASSIGN Replacement

**Flag:** `--disallow-copy` (default: true)
**Compatibility:** GCC 4.4+, C++11

Transforms the deprecated `DISALLOW_COPY_AND_ASSIGN(ClassName)` macro into explicit deleted copy constructor and assignment operator declarations. Per the Chromium style guide, deleted special members should be public.

#### Pattern: Basic replacement

```cpp
// Before
class Foo {
 public:
  Foo();
  ~Foo();

 private:
  int member_;
  DISALLOW_COPY_AND_ASSIGN(Foo);
};

// After
class Foo {
 public:
  Foo();
  ~Foo();

  Foo(const Foo&) = delete;
  Foo& operator=(const Foo&) = delete;

 private:
  int member_;
};
```

#### Pattern: Empty section removal

When the private/protected section contains ONLY the macro, the entire section is removed:

```cpp
// Before
class Bar {
 public:
  Bar();
  void Method();

 private:
  DISALLOW_COPY_AND_ASSIGN(Bar);
};

// After
class Bar {
 public:
  Bar();

  Bar(const Bar&) = delete;
  Bar& operator=(const Bar&) = delete;

  void Method();
};
```

#### Pattern: Class with no public constructors

When a class has no public constructor/destructor, the deleted declarations are inserted after `public:`:

```cpp
// Before
class Singleton {
 private:
  Singleton();
  DISALLOW_COPY_AND_ASSIGN(Singleton);
};

// After
class Singleton {
 public:
  Singleton(const Singleton&) = delete;
  Singleton& operator=(const Singleton&) = delete;

 private:
  Singleton();
};
```

#### Indentation Detection

The tool automatically detects the correct indentation by examining existing constructor/destructor declarations:

```cpp
// Nested class with 4-space indent
class Outer {
 public:
    class Inner {
     public:
        Inner();

        Inner(const Inner&) = delete;
        Inner& operator=(const Inner&) = delete;
    };
};
```

#### NOT Transformed (by design)

These patterns are intentionally **not** transformed:

```cpp
// Forward declarations (no class body) - NOT transformed
class ForwardDeclared;

// DISALLOW_IMPLICIT_CONSTRUCTORS - NOT transformed (different macro)
class NoConstruct {
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NoConstruct);
};

// Already has explicit deleted declarations - NOT transformed
class AlreadyExplicit {
 public:
  AlreadyExplicit(const AlreadyExplicit&) = delete;
  AlreadyExplicit& operator=(const AlreadyExplicit&) = delete;
};
```

#### Skipped: Platform-Specific Files

Files that aren't compiled on the current platform (e.g., `*_win.h` on macOS) won't be in the compilation database and are skipped. Run the tool on each platform or transform these manually.

---

## Transformations NOT Suitable for This Tool

These are better handled by Python scripts or manual review:

| Feature | Reason |
|---------|--------|
| `.starts_with()`/`.ends_with()` | Simple regex pattern matching |
| `std::erase`/`std::erase_if` | Recognizable pattern, no type analysis needed |
| `[[nodiscard]]` | Requires semantic judgment |
| Designated initializers | Opportunistic, context-dependent |

## Features to AVOID

These features are incompatible with CEF's minimum toolchain requirements (GCC 10, macOS 12.0):

| Feature | Version | Blocked By | Notes |
|---------|---------|------------|-------|
| `std::string::contains()` | C++23 | GCC 11+ | Use `.find() != npos` instead |
| `std::format` | C++20 | GCC 13+, macOS 13.3+ | Blocked by both GCC AND macOS target |
| `using enum` | C++20 | GCC 11+ | Not available in GCC 10 |
| `std::to_chars` (float) | C++17 | GCC 11+, macOS 13.3+ | Integer overloads are safe |
| `std::from_chars` (float) | C++17 | GCC 11+, macOS 13.3+ | Integer overloads are safe |
| `<memory_resource>` | C++17 | macOS 14.0+ | Blocked by macOS 12.0 target |

**Warning:** Do NOT confuse `std::string::contains()` (C++23) with associative container `.contains()` (C++20). Only the latter is safe to use.

### macOS Deployment Target Reference

Some C++ library features require a higher macOS version at runtime:

| macOS Version | Features Available |
|---------------|-------------------|
| 10.15+ | `<filesystem>` |
| 11.0+ | `std::barrier`, `std::latch`, `std::counting_semaphore` |
| 12.0+ | (current CEF target) |
| 13.3+ | `std::format`, `std::to_chars`/`std::from_chars` (float) |
| 14.0+ | `<memory_resource>` |

Source: [Apple C++ Language Support](https://developer.apple.com/xcode/cpp/)
