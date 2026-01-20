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

## Implementation Approach

Different modernization patterns require different tools:

| Approach | When to Use | Examples |
|----------|-------------|----------|
| **Clang rewriter** | Need type info, AST context, semantic analysis | `.contains()`, structured bindings, `make_unique` |
| **Python script** | Fixed syntax patterns, unambiguous transforms | `.starts_with()`, `DISALLOW_COPY_AND_ASSIGN` |
| **Manual review** | Semantic decisions, few instances, opportunistic | `[[nodiscard]]`, designated initializers |

## Command-Line Usage

```bash
# Run with all transformations enabled (default)
cef_cpp_rewriter file.cc --

# Disable specific transformations
cef_cpp_rewriter file.cc -- --contains=false

# Enable only specific transformations
cef_cpp_rewriter file.cc -- --contains --count-patterns=false
```

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

## Planned Transformations

The following transformations are planned for future phases.

### Structured Bindings (Phase 8)
**Flag:** `--structured-bindings` (planned)
**Compatibility:** GCC 7+

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

### Default Special Members (Phase 9)
**Flag:** `--default-members` (planned)
**Compatibility:** GCC 4.4+

```cpp
// Before
Foo() {}
~Foo() {}

// After
Foo() = default;
~Foo() = default;
```

### String View Parameters (Phase 10)
**Flag:** `--string-view-params` (planned, opt-in)
**Compatibility:** GCC 7+

```cpp
// Before
void Process(const std::string& name);

// After
void Process(std::string_view name);
```

### Make Unique (Phase 11)
**Flag:** `--make-unique` (planned)
**Compatibility:** GCC 4.9+

```cpp
// Before
std::unique_ptr<Foo> ptr(new Foo(args));

// After
auto ptr = std::make_unique<Foo>(args);
```

---

## Transformations NOT Suitable for This Tool

These are better handled by Python scripts or manual review:

| Feature | Reason |
|---------|--------|
| `.starts_with()`/`.ends_with()` | Simple regex pattern matching |
| `= delete` | Macro replacement (`DISALLOW_COPY_AND_ASSIGN`) |
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
