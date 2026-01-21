// Negative test cases - these should NOT be transformed
// Path contains /cef/ to pass path filter

#include <map>
#include <set>
#include <string>
#include <vector>

void use(int) {}
void use(int, int) {}
template <typename T>
void consume(const T&) {}

void test_pair_used_directly() {
  std::map<int, int> m;

  // Pair is passed directly to a function - DO NOT TRANSFORM
  for (const auto& pair : m) {
    consume(pair);
  }
}

void test_set_iteration() {
  std::set<int> s;

  // Set elements are not pairs - DO NOT TRANSFORM
  for (const auto& elem : s) {
    use(elem);
  }
}

void test_vector_of_pairs() {
  std::vector<std::pair<int, int>> v;

  // Vector is not a map-like container - DO NOT TRANSFORM
  for (const auto& pair : v) {
    use(pair.first, pair.second);
  }
}

void test_non_pair_struct() {
  struct Point {
    int x;
    int y;
  };
  std::vector<Point> points;

  // Not a pair - DO NOT TRANSFORM
  for (const auto& p : points) {
    use(p.x, p.y);
  }
}

void test_no_member_access() {
  std::map<int, int> m;

  // Loop variable not used - DO NOT TRANSFORM (no benefit)
  for (const auto& pair : m) {
    (void)pair;  // Suppress unused variable warning
    use(42);
  }
}

void test_already_structured_bindings() {
  std::map<int, int> m;

  // Already uses structured bindings - DO NOT TRANSFORM (or should be no-op)
  for (const auto& [k, v] : m) {
    use(k, v);
  }
}

void test_local_var_named_key() {
  std::map<int, int> m;

  // Local variable named "key" would conflict with binding - DO NOT TRANSFORM
  for (const auto& pair : m) {
    int key = pair.first;
    use(key);
  }
}

void test_local_var_named_value() {
  std::map<int, int> m;

  // Local variable named "value" would conflict with binding - DO NOT TRANSFORM
  for (const auto& pair : m) {
    int value = pair.second;
    use(value);
  }
}

void test_local_var_key_modified() {
  std::map<std::string, std::string> m;

  // Local variable "key" is modified - DO NOT TRANSFORM
  for (const auto& pair : m) {
    std::string key = pair.first;
    if (key.empty()) {
      key = "default";
    }
    consume(key);
  }
}
