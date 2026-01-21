// Test file for structured bindings transformation
// Path contains /cef/ to pass path filter

#include <map>
#include <string>
#include <unordered_map>

void use(int, int) {}
void use(const std::string&, int) {}

void test_basic_map() {
  std::map<int, int> m;

  // Basic pattern: const auto& pair with .first/.second
  for (const auto& [key, value] : m) {
    use(key, value);
  }

  // Non-const reference
  for (auto& [key, value] : m) {
    use(key, value);
  }

  // Value copy
  for (auto [key, value] : m) {
    use(key, value);
  }
}

void test_unordered_map() {
  std::unordered_map<std::string, int> um;

  for (const auto& [key, value] : um) {
    use(key, value);
  }
}

void test_only_first() {
  std::map<int, int> m;

  // Only uses .first
  for (const auto& [key, value] : m) {
    use(key, 0);
  }
}

void test_only_second() {
  std::map<int, int> m;

  // Only uses .second
  for (const auto& [key, value] : m) {
    use(0, value);
  }
}

void test_multiple_uses() {
  std::map<int, int> m;

  // Multiple uses of .first and .second
  for (const auto& [key, value] : m) {
    use(key, value);
    use(key + 1, value + 1);
  }
}
