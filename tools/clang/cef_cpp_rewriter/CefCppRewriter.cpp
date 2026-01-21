// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "OutputHelper.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"

namespace {

using namespace clang;
using namespace clang::ast_matchers;

static llvm::cl::OptionCategory rewriter_category("cef_cpp_rewriter options");
llvm::cl::extrahelp common_help(
    clang::tooling::CommonOptionsParser::HelpMessage);

// Command-line flag to enable/disable contains transformation
static llvm::cl::opt<bool> EnableContains(
    "contains",
    llvm::cl::desc("Enable .contains() transformation (default: true)"),
    llvm::cl::init(true),
    llvm::cl::cat(rewriter_category));

// Command-line flag to enable/disable count() pattern transformation
static llvm::cl::opt<bool> EnableCountPatterns(
    "count-patterns",
    llvm::cl::desc("Enable count() pattern transformation (default: true)"),
    llvm::cl::init(true),
    llvm::cl::cat(rewriter_category));

// Command-line flag to disable path filtering (for testing)
static llvm::cl::opt<bool> DisablePathFilter(
    "disable-path-filter",
    llvm::cl::desc("Disable /cef/ path filtering (for testing)"),
    llvm::cl::init(false),
    llvm::cl::cat(rewriter_category));

// Command-line flag to enable/disable structured bindings transformation
static llvm::cl::opt<bool> EnableStructuredBindings(
    "structured-bindings",
    llvm::cl::desc("Enable structured bindings transformation (default: true)"),
    llvm::cl::init(true),
    llvm::cl::cat(rewriter_category));

// Command-line flag to enable/disable iterator loop to range-for transformation
static llvm::cl::opt<bool> EnableIteratorLoops(
    "iterator-loops",
    llvm::cl::desc(
        "Enable iterator loop to range-for transformation (default: true)"),
    llvm::cl::init(true),
    llvm::cl::cat(rewriter_category));

// Command-line flag to enable/disable DISALLOW_COPY_AND_ASSIGN transformation
static llvm::cl::opt<bool> EnableDisallowCopy(
    "disallow-copy",
    llvm::cl::desc(
        "Enable DISALLOW_COPY_AND_ASSIGN transformation (default: true)"),
    llvm::cl::init(true),
    llvm::cl::cat(rewriter_category));

// Command-line flag to run only specific transformations
// When specified, all other transformations are disabled
static llvm::cl::opt<std::string> OnlyTransforms(
    "only",
    llvm::cl::desc(
        "Run only the specified transformation(s). Comma-separated list of: "
        "contains, count-patterns, structured-bindings, iterator-loops, "
        "disallow-copy. Example: --only=contains,structured-bindings"),
    llvm::cl::init(""),
    llvm::cl::cat(rewriter_category));

// Helper to check if a transform is enabled via --only flag
bool isTransformEnabled(const std::string& name, bool default_value) {
  if (OnlyTransforms.empty()) {
    return default_value;
  }
  // Parse comma-separated list
  std::string only = OnlyTransforms;
  size_t pos = 0;
  while ((pos = only.find(',')) != std::string::npos || !only.empty()) {
    std::string token;
    if (pos != std::string::npos) {
      token = only.substr(0, pos);
      only = only.substr(pos + 1);
    } else {
      token = only;
      only.clear();
    }
    // Trim whitespace
    size_t start = token.find_first_not_of(" \t");
    size_t end = token.find_last_not_of(" \t");
    if (start != std::string::npos) {
      token = token.substr(start, end - start + 1);
    }
    if (token == name) {
      return true;
    }
  }
  return false;
}

// Helper to match associative container types (the underlying record type)
auto isAssociativeContainerType() {
  return hasUnqualifiedDesugaredType(
      recordType(hasDeclaration(classTemplateSpecializationDecl(anyOf(
          hasName("::std::map"), hasName("::std::set"),
          hasName("::std::unordered_map"), hasName("::std::unordered_set"),
          hasName("::std::multimap"), hasName("::std::multiset"),
          hasName("::std::unordered_multimap"),
          hasName("::std::unordered_multiset"))))));
}

// Helper to match expression that is an associative container
// (either direct access or pointer access)
auto isAssociativeContainer() {
  return anyOf(hasType(isAssociativeContainerType()),
               hasType(pointsTo(isAssociativeContainerType())));
}

// Helper function to get source text for an expression
std::string getSourceText(const Expr* expr, const SourceManager& sm,
                          const LangOptions& opts) {
  CharSourceRange range = CharSourceRange::getTokenRange(expr->getSourceRange());
  return Lexer::getSourceText(range, sm, opts).str();
}

// Helper function to get source text for a statement
std::string getSourceText(const Stmt* stmt, const SourceManager& sm,
                          const LangOptions& opts) {
  CharSourceRange range = CharSourceRange::getTokenRange(stmt->getSourceRange());
  return Lexer::getSourceText(range, sm, opts).str();
}

class ContainsRewriter : public MatchFinder::MatchCallback {
 public:
  explicit ContainsRewriter(OutputHelper* output) : output_helper_(*output) {}

  // Reset processed locations for each new file
  void reset() { processed_locations_.clear(); }

  void run(const MatchFinder::MatchResult& result) override {
    const SourceManager& sm = *result.SourceManager;
    const LangOptions& opts = result.Context->getLangOpts();

    // Handle find()/end() comparison pattern
    if (const auto* comparison =
            result.Nodes.getNodeAs<CXXOperatorCallExpr>("comparison")) {
      handleFindEndComparison(comparison, result, sm, opts);
      return;
    }

    // Handle count() comparison pattern (count != 0, count > 0, count == 0)
    if (const auto* count_comparison =
            result.Nodes.getNodeAs<BinaryOperator>("countComparison")) {
      handleCountComparison(count_comparison, result, sm, opts);
      return;
    }

    // Handle count() in boolean context (if (count(x)))
    if (const auto* count_boolean =
            result.Nodes.getNodeAs<ImplicitCastExpr>("countBoolean")) {
      handleCountBoolean(count_boolean, result, sm, opts);
      return;
    }
  }

 private:
  void handleFindEndComparison(const CXXOperatorCallExpr* comparison,
                               const MatchFinder::MatchResult& result,
                               const SourceManager& sm,
                               const LangOptions& opts) {
    // Skip if inside a macro expansion
    if (comparison->getBeginLoc().isMacroID() ||
        sm.isMacroBodyExpansion(comparison->getBeginLoc())) {
      return;
    }

    // Skip if in a system header (e.g., inside std::operator!= implementation)
    if (sm.isInSystemHeader(comparison->getBeginLoc())) {
      return;
    }

    // Skip if we've already processed this source location (deduplication)
    // This prevents issues where template instantiations create multiple matches
    unsigned offset = sm.getFileOffset(comparison->getBeginLoc());
    if (processed_locations_.count(offset)) {
      return;
    }
    processed_locations_.insert(offset);

    // Skip files not in /cef/ directory (unless path filtering is disabled)
    if (!DisablePathFilter) {
      StringRef filename = sm.getFilename(comparison->getBeginLoc());
      if (!filename.contains("/cef/")) {
        return;
      }
    }

    const auto* find_call =
        result.Nodes.getNodeAs<CXXMemberCallExpr>("findCall");
    const auto* end_call = result.Nodes.getNodeAs<CXXMemberCallExpr>("endCall");
    const auto* find_container = result.Nodes.getNodeAs<Expr>("findContainer");
    const auto* end_container = result.Nodes.getNodeAs<Expr>("endContainer");

    if (!find_call || !end_call || !find_container || !end_container) {
      return;
    }

    // Verify same container for find() and end()
    std::string find_container_text = getSourceText(find_container, sm, opts);
    std::string end_container_text = getSourceText(end_container, sm, opts);

    if (find_container_text != end_container_text) {
      return;  // Different containers, skip
    }

    // Determine if this is != or == by looking at the source text
    // This is more reliable than getOperator() for template-instantiated code
    std::string comparison_text = getSourceText(comparison, sm, opts);
    bool is_not_equal = comparison_text.find("!=") != std::string::npos;

    // Get the argument to find()
    if (find_call->getNumArgs() < 1) {
      return;
    }
    const Expr* find_arg = find_call->getArg(0);
    std::string find_arg_text = getSourceText(find_arg, sm, opts);

    // Determine if we need -> or . based on whether container is a pointer
    bool is_pointer = find_container->getType()->isPointerType();
    std::string access_op = is_pointer ? "->" : ".";

    // Generate replacement
    std::string replacement;
    if (is_not_equal) {
      // container.find(x) != container.end() -> container.contains(x)
      replacement = find_container_text + access_op + "contains(" + find_arg_text + ")";
    } else {
      // container.find(x) == container.end() -> !container.contains(x)
      replacement = "!" + find_container_text + access_op + "contains(" + find_arg_text + ")";
    }

    // Get the range of the entire comparison expression
    CharSourceRange range =
        CharSourceRange::getTokenRange(comparison->getSourceRange());

    output_helper_.Replace(range, replacement, sm, opts);
  }

  void handleCountComparison(const BinaryOperator* comparison,
                             const MatchFinder::MatchResult& result,
                             const SourceManager& sm,
                             const LangOptions& opts) {
    // Skip if inside a macro expansion
    if (comparison->getBeginLoc().isMacroID() ||
        sm.isMacroBodyExpansion(comparison->getBeginLoc())) {
      return;
    }

    // Skip files not in /cef/ directory (unless path filtering is disabled)
    if (!DisablePathFilter) {
      StringRef filename = sm.getFilename(comparison->getBeginLoc());
      if (!filename.contains("/cef/")) {
        return;
      }
    }

    const auto* count_call =
        result.Nodes.getNodeAs<CXXMemberCallExpr>("countCall");
    const auto* count_container =
        result.Nodes.getNodeAs<Expr>("countContainer");

    if (!count_call || !count_container) {
      return;
    }

    // Get the argument to count()
    if (count_call->getNumArgs() < 1) {
      return;
    }
    const Expr* count_arg = count_call->getArg(0);
    std::string container_text = getSourceText(count_container, sm, opts);
    std::string count_arg_text = getSourceText(count_arg, sm, opts);

    // Determine if we need -> or . based on whether container is a pointer
    bool is_pointer = count_container->getType()->isPointerType();
    std::string access_op = is_pointer ? "->" : ".";

    // Determine the operator and generate replacement
    BinaryOperator::Opcode op = comparison->getOpcode();
    std::string replacement;

    if (op == BO_NE || op == BO_GT) {
      // count(x) != 0 or count(x) > 0 -> contains(x)
      replacement = container_text + access_op + "contains(" + count_arg_text + ")";
    } else if (op == BO_EQ) {
      // count(x) == 0 -> !contains(x)
      replacement = "!" + container_text + access_op + "contains(" + count_arg_text + ")";
    } else {
      return;  // Unexpected operator
    }

    // Get the range of the entire comparison expression
    CharSourceRange range =
        CharSourceRange::getTokenRange(comparison->getSourceRange());

    output_helper_.Replace(range, replacement, sm, opts);
  }

  void handleCountBoolean(const ImplicitCastExpr* cast_expr,
                          const MatchFinder::MatchResult& result,
                          const SourceManager& sm,
                          const LangOptions& opts) {
    // Skip if inside a macro expansion
    if (cast_expr->getBeginLoc().isMacroID() ||
        sm.isMacroBodyExpansion(cast_expr->getBeginLoc())) {
      return;
    }

    // Skip files not in /cef/ directory (unless path filtering is disabled)
    if (!DisablePathFilter) {
      StringRef filename = sm.getFilename(cast_expr->getBeginLoc());
      if (!filename.contains("/cef/")) {
        return;
      }
    }

    // Verify this is an IntegralToBoolean cast
    if (cast_expr->getCastKind() != CK_IntegralToBoolean) {
      return;
    }

    const auto* count_call =
        result.Nodes.getNodeAs<CXXMemberCallExpr>("countCall");
    const auto* count_container =
        result.Nodes.getNodeAs<Expr>("countContainer");

    if (!count_call || !count_container) {
      return;
    }

    // Get the argument to count()
    if (count_call->getNumArgs() < 1) {
      return;
    }
    const Expr* count_arg = count_call->getArg(0);
    std::string container_text = getSourceText(count_container, sm, opts);
    std::string count_arg_text = getSourceText(count_arg, sm, opts);

    // Determine if we need -> or . based on whether container is a pointer
    bool is_pointer = count_container->getType()->isPointerType();
    std::string access_op = is_pointer ? "->" : ".";

    // if (count(x)) -> if (contains(x))
    std::string replacement =
        container_text + access_op + "contains(" + count_arg_text + ")";

    // Replace just the count() call, not the implicit cast
    CharSourceRange range =
        CharSourceRange::getTokenRange(count_call->getSourceRange());

    output_helper_.Replace(range, replacement, sm, opts);
  }

  OutputHelper& output_helper_;
  // Track processed source locations to avoid duplicate replacements
  std::set<unsigned> processed_locations_;
};

// Helper to check if a type is a pair-like type (std::pair)
bool isPairLikeType(QualType type) {
  type = type.getCanonicalType();
  if (type->isReferenceType()) {
    type = type.getNonReferenceType();
  }
  type = type.getUnqualifiedType();

  const auto* record = type->getAs<RecordType>();
  if (!record) {
    return false;
  }

  const auto* decl = record->getDecl();
  if (!decl) {
    return false;
  }

  // Check for std::pair
  std::string name = decl->getQualifiedNameAsString();
  return name.find("std::pair") != std::string::npos;
}

// Helper to check if a type is a map-like container (has key_type and mapped_type)
bool isMapLikeContainer(QualType type) {
  type = type.getCanonicalType();
  if (type->isReferenceType()) {
    type = type.getNonReferenceType();
  }
  type = type.getUnqualifiedType();

  const auto* record = type->getAs<RecordType>();
  if (!record) {
    return false;
  }

  const auto* decl = record->getDecl();
  if (!decl) {
    return false;
  }

  std::string name = decl->getQualifiedNameAsString();
  return name.find("std::map") != std::string::npos ||
         name.find("std::unordered_map") != std::string::npos ||
         name.find("std::multimap") != std::string::npos ||
         name.find("std::unordered_multimap") != std::string::npos;
}

// Visitor to collect all uses of a variable in an expression
class VarUseCollector : public RecursiveASTVisitor<VarUseCollector> {
 public:
  explicit VarUseCollector(const VarDecl* var) : target_var_(var) {}

  bool VisitDeclRefExpr(DeclRefExpr* ref) {
    if (ref->getDecl() == target_var_) {
      uses_.push_back(ref);
    }
    return true;
  }

  const std::vector<DeclRefExpr*>& getUses() const { return uses_; }

 private:
  const VarDecl* target_var_;
  std::vector<DeclRefExpr*> uses_;
};

// Visitor to collect all local variable declarations in a statement
class LocalVarDeclCollector : public RecursiveASTVisitor<LocalVarDeclCollector> {
 public:
  bool VisitVarDecl(VarDecl* decl) {
    // Only collect local variables (not parameters, etc.)
    if (decl->isLocalVarDecl()) {
      var_names_.insert(decl->getNameAsString());
    }
    return true;
  }

  bool hasConflict(const std::string& name) const {
    return var_names_.count(name) > 0;
  }

 private:
  std::set<std::string> var_names_;
};

class StructuredBindingsRewriter : public MatchFinder::MatchCallback {
 public:
  explicit StructuredBindingsRewriter(OutputHelper* output)
      : output_helper_(*output) {}

  void reset() { processed_locations_.clear(); }

  void run(const MatchFinder::MatchResult& result) override {
    const auto* for_stmt = result.Nodes.getNodeAs<CXXForRangeStmt>("forRange");
    if (!for_stmt) {
      return;
    }

    const SourceManager& sm = *result.SourceManager;
    const LangOptions& opts = result.Context->getLangOpts();

    // Skip if inside a macro expansion
    if (for_stmt->getBeginLoc().isMacroID() ||
        sm.isMacroBodyExpansion(for_stmt->getBeginLoc())) {
      return;
    }

    // Skip if in a system header
    if (sm.isInSystemHeader(for_stmt->getBeginLoc())) {
      return;
    }

    // Skip if we've already processed this source location
    unsigned offset = sm.getFileOffset(for_stmt->getBeginLoc());
    if (processed_locations_.count(offset)) {
      return;
    }

    // Skip files not in /cef/ directory (unless path filtering is disabled)
    if (!DisablePathFilter) {
      StringRef filename = sm.getFilename(for_stmt->getBeginLoc());
      if (!filename.contains("/cef/")) {
        return;
      }
    }

    // Get the loop variable
    const VarDecl* loop_var = for_stmt->getLoopVariable();
    if (!loop_var) {
      return;
    }

    // Check if the loop variable type is pair-like
    QualType var_type = loop_var->getType();
    if (!isPairLikeType(var_type)) {
      return;
    }

    // Get the range expression and check if it's a map-like container
    const Expr* range_expr = for_stmt->getRangeInit();
    if (!range_expr) {
      return;
    }

    QualType range_type = range_expr->getType();
    if (!isMapLikeContainer(range_type)) {
      return;
    }

    // Get the loop body
    const Stmt* body = for_stmt->getBody();
    if (!body) {
      return;
    }

    // Check for local variable declarations that would conflict with binding names
    // This prevents bugs like: for (const auto& [key, value] : m) { std::string key = key; }
    LocalVarDeclCollector local_var_collector;
    local_var_collector.TraverseStmt(const_cast<Stmt*>(body));
    if (local_var_collector.hasConflict("key") ||
        local_var_collector.hasConflict("value")) {
      // Log skipped instance for manual review (to stdout, not stderr)
      SourceLocation loc = for_stmt->getBeginLoc();
      llvm::outs() << "# SKIPPED (variable conflict): "
                   << sm.getFilename(loc).str() << ":"
                   << sm.getSpellingLineNumber(loc) << "\n";
      return;
    }

    // Collect all uses of the loop variable in the body
    VarUseCollector collector(loop_var);
    collector.TraverseStmt(const_cast<Stmt*>(body));

    const auto& uses = collector.getUses();
    if (uses.empty()) {
      return;  // No uses of the loop variable
    }

    // Check that ALL uses are via .first or .second member access
    // Also collect info about which members are used
    bool uses_first = false;
    bool uses_second = false;
    std::vector<std::pair<const MemberExpr*, bool>> member_accesses;  // (expr, is_first)

    for (const DeclRefExpr* use : uses) {
      // The use must be the base of a MemberExpr accessing .first or .second
      // Walk up to find the parent MemberExpr
      const auto& parents = result.Context->getParents(*use);
      if (parents.empty()) {
        return;  // Use without parent - can't be .first/.second access
      }

      const MemberExpr* member = parents[0].get<MemberExpr>();
      if (!member) {
        return;  // Not a member access - loop var used directly
      }

      // Check that the member is .first or .second
      const ValueDecl* member_decl = member->getMemberDecl();
      if (!member_decl) {
        return;
      }

      std::string member_name = member_decl->getNameAsString();
      if (member_name == "first") {
        uses_first = true;
        member_accesses.push_back({member, true});
      } else if (member_name == "second") {
        uses_second = true;
        member_accesses.push_back({member, false});
      } else {
        return;  // Accessing some other member
      }
    }

    if (!uses_first && !uses_second) {
      return;  // No .first/.second access
    }

    // Mark as processed
    processed_locations_.insert(offset);

    // Generate binding names based on what's used
    std::string first_name = "key";
    std::string second_name = "value";

    // Build the new binding declaration
    // e.g., "pair" -> "[key, value]"
    std::string new_binding = "[" + first_name + ", " + second_name + "]";

    // Replace just the variable name with the structured binding
    // This transforms "const auto& pair" -> "const auto& [key, value]"
    SourceLocation var_loc = loop_var->getLocation();
    unsigned var_len = loop_var->getNameAsString().length();
    CharSourceRange var_range = CharSourceRange::getCharRange(
        var_loc, var_loc.getLocWithOffset(var_len));
    output_helper_.Replace(var_range, new_binding, sm, opts);

    // Replace each .first/.second access with the binding name
    for (const auto& [member, is_first] : member_accesses) {
      std::string replacement = is_first ? first_name : second_name;
      CharSourceRange member_range =
          CharSourceRange::getTokenRange(member->getSourceRange());
      output_helper_.Replace(member_range, replacement, sm, opts);
    }
  }

 private:
  OutputHelper& output_helper_;
  std::set<unsigned> processed_locations_;
};

// Visitor to collect all uses of an iterator variable in a statement
class IteratorUseCollector : public RecursiveASTVisitor<IteratorUseCollector> {
 public:
  explicit IteratorUseCollector(const VarDecl* var) : target_var_(var) {}

  bool VisitDeclRefExpr(DeclRefExpr* ref) {
    if (ref->getDecl() == target_var_) {
      uses_.push_back(ref);
    }
    return true;
  }

  const std::vector<DeclRefExpr*>& getUses() const { return uses_; }

 private:
  const VarDecl* target_var_;
  std::vector<DeclRefExpr*> uses_;
};

// Rewriter for traditional iterator-based for loops
// Converts: for (auto it = m.begin(); it != m.end(); ++it) { use(it->first, it->second); }
// To:       for (const auto& [key, value] : m) { use(key, value); }
class IteratorLoopRewriter : public MatchFinder::MatchCallback {
 public:
  explicit IteratorLoopRewriter(OutputHelper* output) : output_helper_(*output) {}

  void reset() { processed_locations_.clear(); }

  void run(const MatchFinder::MatchResult& result) override {
    const auto* for_stmt = result.Nodes.getNodeAs<ForStmt>("forStmt");
    if (!for_stmt) {
      return;
    }

    const SourceManager& sm = *result.SourceManager;
    const LangOptions& opts = result.Context->getLangOpts();

    // Skip if inside a macro expansion
    if (for_stmt->getBeginLoc().isMacroID() ||
        sm.isMacroBodyExpansion(for_stmt->getBeginLoc())) {
      return;
    }

    // Skip if in a system header
    if (sm.isInSystemHeader(for_stmt->getBeginLoc())) {
      return;
    }

    // Skip if we've already processed this source location
    unsigned offset = sm.getFileOffset(for_stmt->getBeginLoc());
    if (processed_locations_.count(offset)) {
      return;
    }

    // Skip files not in /cef/ directory (unless path filtering is disabled)
    if (!DisablePathFilter) {
      StringRef filename = sm.getFilename(for_stmt->getBeginLoc());
      if (!filename.contains("/cef/")) {
        return;
      }
    }

    // Get the init statement: should be a DeclStmt with a VarDecl
    const Stmt* init = for_stmt->getInit();
    if (!init) {
      return;
    }

    const DeclStmt* decl_stmt = dyn_cast<DeclStmt>(init);
    if (!decl_stmt || !decl_stmt->isSingleDecl()) {
      return;
    }

    const VarDecl* iter_var = dyn_cast<VarDecl>(decl_stmt->getSingleDecl());
    if (!iter_var) {
      return;
    }

    // Check that the init expression is container.begin()
    const Expr* init_expr = iter_var->getInit();
    if (!init_expr) {
      return;
    }

    // Skip through any implicit casts/materialize temps
    init_expr = init_expr->IgnoreImplicit();

    // For explicit iterator type declarations like:
    //   std::map<int,int>::const_iterator it = m.begin();
    // The init is wrapped in a CXXConstructExpr for the implicit conversion
    if (auto* construct = dyn_cast<CXXConstructExpr>(init_expr)) {
      if (construct->getNumArgs() == 1) {
        init_expr = construct->getArg(0)->IgnoreImplicit();
      }
    }

    const CXXMemberCallExpr* begin_call = dyn_cast<CXXMemberCallExpr>(init_expr);
    if (!begin_call) {
      return;
    }

    const CXXMethodDecl* begin_method =
        dyn_cast_or_null<CXXMethodDecl>(begin_call->getMethodDecl());
    if (!begin_method || begin_method->getNameAsString() != "begin") {
      return;
    }

    // Get the container from the begin() call
    const Expr* begin_container = begin_call->getImplicitObjectArgument();
    if (!begin_container) {
      return;
    }

    // Check that the iterator dereferences to a pair-like type
    // This supports both map containers AND vector<pair<K,V>>, etc.
    QualType iter_type = iter_var->getType().getCanonicalType();

    // Get the type that the iterator dereferences to (value_type)
    // For iterators, we need to look at what operator* returns
    const CXXRecordDecl* iter_record = iter_type->getAsCXXRecordDecl();
    if (!iter_record) {
      return;
    }

    // Find the value_type or check if dereferencing gives a pair
    bool has_pair_value_type = false;

    // Check if it's a map-like container (which we know has pair value_type)
    QualType container_type = begin_container->getType();
    if (isMapLikeContainer(container_type)) {
      has_pair_value_type = true;
    } else {
      // For other containers, check if value_type is std::pair
      // Look for value_type typedef in the iterator or container
      for (const auto* decl : iter_record->decls()) {
        if (const auto* type_alias = dyn_cast<TypedefNameDecl>(decl)) {
          if (type_alias->getName() == "value_type") {
            QualType value_type = type_alias->getUnderlyingType().getCanonicalType();
            if (const auto* record = value_type->getAsCXXRecordDecl()) {
              std::string name = record->getQualifiedNameAsString();
              if (name.find("std::pair") == 0 ||
                  name.find("std::__1::pair") == 0) {
                has_pair_value_type = true;
                break;
              }
            }
          }
        }
      }
    }

    if (!has_pair_value_type) {
      return;
    }

    // Get the condition: should be it != container.end() or container.end() != it
    const Expr* cond = for_stmt->getCond();
    if (!cond) {
      return;
    }

    // The condition might be wrapped in ExprWithCleanups
    cond = cond->IgnoreImplicit();
    if (auto* ewc = dyn_cast<ExprWithCleanups>(cond)) {
      cond = ewc->getSubExpr();
    }

    // Try to match operator!= comparison
    // In C++20, some containers (like unordered_map) only define operator==,
    // and the compiler rewrites != as !(a == b) using CXXRewrittenBinaryOperator
    const CXXOperatorCallExpr* cond_op = nullptr;
    bool is_rewritten_not_equal = false;

    if (auto* rewritten = dyn_cast<CXXRewrittenBinaryOperator>(cond)) {
      // This is a rewritten != that becomes !(a == b)
      // Get the semantic form which contains the actual == comparison
      const Expr* semantic = rewritten->getSemanticForm();
      if (auto* unary = dyn_cast<UnaryOperator>(semantic)) {
        if (unary->getOpcode() == UO_LNot) {
          if (auto* inner_op = dyn_cast<CXXOperatorCallExpr>(unary->getSubExpr())) {
            if (inner_op->getOperator() == OO_EqualEqual) {
              cond_op = inner_op;
              is_rewritten_not_equal = true;
            }
          }
        }
      }
    } else {
      cond_op = dyn_cast<CXXOperatorCallExpr>(cond);
    }

    if (!cond_op) {
      return;
    }

    // For direct !=, check it's the right operator
    // For rewritten !=, we already verified it's == inside !()
    if (!is_rewritten_not_equal && cond_op->getOperator() != OO_ExclaimEqual) {
      return;
    }

    // Find which argument is the iterator and which is end()
    const Expr* arg0 = cond_op->getArg(0)->IgnoreImplicit();
    const Expr* arg1 = cond_op->getArg(1)->IgnoreImplicit();

    // Helper lambda to extract CXXMemberCallExpr, handling CXXConstructExpr wrapper
    auto extractMemberCall = [](const Expr* expr) -> const CXXMemberCallExpr* {
      if (auto* call = dyn_cast<CXXMemberCallExpr>(expr)) {
        return call;
      }
      // Handle CXXConstructExpr wrapper (for const_iterator conversion)
      if (auto* construct = dyn_cast<CXXConstructExpr>(expr)) {
        if (construct->getNumArgs() == 1) {
          return dyn_cast<CXXMemberCallExpr>(
              construct->getArg(0)->IgnoreImplicit());
        }
      }
      return nullptr;
    };

    const CXXMemberCallExpr* end_call = nullptr;
    const DeclRefExpr* iter_ref = nullptr;

    if (auto* call = extractMemberCall(arg0)) {
      if (auto* method = call->getMethodDecl()) {
        if (method->getNameAsString() == "end") {
          end_call = call;
          iter_ref = dyn_cast<DeclRefExpr>(arg1);
        }
      }
    }
    if (!end_call) {
      if (auto* call = extractMemberCall(arg1)) {
        if (auto* method = call->getMethodDecl()) {
          if (method->getNameAsString() == "end") {
            end_call = call;
            iter_ref = dyn_cast<DeclRefExpr>(arg0);
          }
        }
      }
    }

    if (!end_call || !iter_ref) {
      return;
    }

    // Verify the iterator reference refers to our iterator variable
    if (iter_ref->getDecl() != iter_var) {
      return;
    }

    // Get the end() container and verify it matches the begin() container
    const Expr* end_container = end_call->getImplicitObjectArgument();
    if (!end_container) {
      return;
    }

    std::string begin_container_text = getSourceText(begin_container, sm, opts);
    std::string end_container_text = getSourceText(end_container, sm, opts);

    if (begin_container_text != end_container_text) {
      return;  // Different containers
    }

    // Get the increment: should be ++it or it++
    const Expr* inc = for_stmt->getInc();
    if (!inc) {
      return;
    }

    const UnaryOperator* inc_op = dyn_cast<UnaryOperator>(inc);
    const CXXOperatorCallExpr* inc_call = nullptr;

    bool valid_increment = false;
    if (inc_op) {
      // Built-in ++it or it++
      if (inc_op->getOpcode() == UO_PreInc || inc_op->getOpcode() == UO_PostInc) {
        if (auto* ref = dyn_cast<DeclRefExpr>(inc_op->getSubExpr()->IgnoreImplicit())) {
          if (ref->getDecl() == iter_var) {
            valid_increment = true;
          }
        }
      }
    } else {
      // Overloaded operator++ (common for std::map iterators)
      inc_call = dyn_cast<CXXOperatorCallExpr>(inc);
      if (inc_call) {
        if (inc_call->getOperator() == OO_PlusPlus) {
          if (auto* ref = dyn_cast<DeclRefExpr>(
                  inc_call->getArg(0)->IgnoreImplicit())) {
            if (ref->getDecl() == iter_var) {
              valid_increment = true;
            }
          }
        }
      }
    }

    if (!valid_increment) {
      return;
    }

    // Get the loop body
    const Stmt* body = for_stmt->getBody();
    if (!body) {
      return;
    }

    // Check for local variable conflicts
    LocalVarDeclCollector local_var_collector;
    local_var_collector.TraverseStmt(const_cast<Stmt*>(body));
    if (local_var_collector.hasConflict("key") ||
        local_var_collector.hasConflict("value")) {
      SourceLocation loc = for_stmt->getBeginLoc();
      llvm::outs() << "# SKIPPED (variable conflict): " << sm.getFilename(loc).str()
                   << ":" << sm.getSpellingLineNumber(loc) << "\n";
      return;
    }

    // Collect all uses of the iterator in the body
    IteratorUseCollector collector(iter_var);
    collector.TraverseStmt(const_cast<Stmt*>(body));

    const auto& uses = collector.getUses();
    if (uses.empty()) {
      return;  // Iterator not used in body
    }

    // Check that ALL uses are via ->first or ->second member access
    bool uses_first = false;
    bool uses_second = false;
    std::vector<std::pair<const MemberExpr*, bool>> member_accesses;

    for (const DeclRefExpr* use : uses) {
      // Walk up the AST to find a MemberExpr for ->first or ->second
      // The AST structure for it->first is:
      //   MemberExpr (->first)
      //     CXXOperatorCallExpr (operator->)
      //       ImplicitCastExpr
      //         DeclRefExpr (it)
      //
      // We need to walk up through all the intermediate nodes.

      const MemberExpr* member = nullptr;
      const Stmt* current = use;
      int max_depth = 5;  // Prevent infinite loops

      while (max_depth-- > 0) {
        const auto& parents = result.Context->getParents(*current);
        if (parents.empty()) {
          break;
        }

        const Stmt* parent_stmt = parents[0].get<Stmt>();
        if (!parent_stmt) {
          break;
        }

        // Check if this parent is a MemberExpr
        if (auto* me = dyn_cast<MemberExpr>(parent_stmt)) {
          member = me;
          break;
        }

        // Continue walking up through ImplicitCastExpr, CXXOperatorCallExpr, etc.
        current = parent_stmt;
      }

      if (!member) {
        return;  // Iterator used in non-member-access context
      }

      // Verify it's using arrow operator (isArrow())
      if (!member->isArrow()) {
        return;  // Using dot operator on iterator (shouldn't happen normally)
      }

      // Check that the member is .first or .second
      const ValueDecl* member_decl = member->getMemberDecl();
      if (!member_decl) {
        return;
      }

      std::string member_name = member_decl->getNameAsString();
      if (member_name == "first") {
        uses_first = true;
        member_accesses.push_back({member, true});
      } else if (member_name == "second") {
        uses_second = true;
        member_accesses.push_back({member, false});
      } else {
        return;  // Accessing some other member
      }
    }

    if (!uses_first && !uses_second) {
      return;  // No ->first/->second access
    }

    // Mark as processed
    processed_locations_.insert(offset);

    // Generate binding names
    std::string first_name = "key";
    std::string second_name = "value";
    std::string new_binding = "[" + first_name + ", " + second_name + "]";

    // Build the new for loop header
    // Replace "for (auto it = m.begin(); it != m.end(); ++it)" with
    // "for (const auto& [key, value] : m)"
    std::string new_header = "for (const auto& " + new_binding + " : " +
                             begin_container_text + ")";

    // Get the range from 'for' to the closing paren of the for header
    SourceLocation for_loc = for_stmt->getForLoc();
    SourceLocation rparen_loc = for_stmt->getRParenLoc();

    CharSourceRange header_range =
        CharSourceRange::getTokenRange(for_loc, rparen_loc);
    output_helper_.Replace(header_range, new_header, sm, opts);

    // Replace each ->first/->second access with the binding name
    for (const auto& [member, is_first] : member_accesses) {
      std::string replacement = is_first ? first_name : second_name;
      CharSourceRange member_range =
          CharSourceRange::getTokenRange(member->getSourceRange());
      output_helper_.Replace(member_range, replacement, sm, opts);
    }
  }

 private:
  OutputHelper& output_helper_;
  std::set<unsigned> processed_locations_;
};

// Rewriter for DISALLOW_COPY_AND_ASSIGN macro
// Transforms the deprecated macro to explicit deleted declarations in public section
//
// Before:
//   class Foo {
//    public:
//     Foo();
//     ~Foo();
//    private:
//     DISALLOW_COPY_AND_ASSIGN(Foo);
//   };
//
// After:
//   class Foo {
//    public:
//     Foo();
//     ~Foo();
//
//     Foo(const Foo&) = delete;
//     Foo& operator=(const Foo&) = delete;
//   };
class DisallowCopyRewriter : public MatchFinder::MatchCallback {
 public:
  explicit DisallowCopyRewriter(OutputHelper* output) : output_helper_(*output) {}

  void reset() { processed_locations_.clear(); }

  void run(const MatchFinder::MatchResult& result) override {
    const auto* class_decl = result.Nodes.getNodeAs<CXXRecordDecl>("classDecl");
    if (!class_decl) {
      return;
    }

    // Skip non-definitions (forward declarations)
    if (!class_decl->isThisDeclarationADefinition()) {
      return;
    }

    const SourceManager& sm = *result.SourceManager;
    const LangOptions& opts = result.Context->getLangOpts();

    // Skip if in a system header
    if (sm.isInSystemHeader(class_decl->getBeginLoc())) {
      return;
    }

    // Skip if we've already processed this source location
    unsigned offset = sm.getFileOffset(class_decl->getBeginLoc());
    if (processed_locations_.count(offset)) {
      return;
    }

    // Skip files not in /cef/ directory (unless path filtering is disabled)
    if (!DisablePathFilter) {
      StringRef filename = sm.getFilename(class_decl->getBeginLoc());
      if (!filename.contains("/cef/")) {
        return;
      }
    }

    // Get the class name
    std::string class_name = class_decl->getNameAsString();
    if (class_name.empty()) {
      return;  // Anonymous class
    }

    // Get the source range of the entire class including braces
    SourceLocation start_loc = class_decl->getBeginLoc();
    SourceLocation end_loc = class_decl->getBraceRange().getEnd();
    if (end_loc.isInvalid()) {
      end_loc = class_decl->getEndLoc();
    }

    // Get the full source text of the class
    bool invalid = false;
    StringRef source_text = Lexer::getSourceText(
        CharSourceRange::getTokenRange(start_loc, end_loc),
        sm, opts, &invalid);
    if (invalid || source_text.empty()) {
      return;
    }

    // Search for DISALLOW_COPY_AND_ASSIGN(ClassName)
    std::string macro_pattern = "DISALLOW_COPY_AND_ASSIGN(" + class_name + ")";
    size_t macro_pos = source_text.find(macro_pattern);
    if (macro_pos == StringRef::npos) {
      return;  // Macro not found in this class
    }

    // Mark as processed
    processed_locations_.insert(offset);

    // Find the start of the line containing the macro
    size_t line_start = macro_pos;
    while (line_start > 0 && source_text[line_start - 1] != '\n') {
      --line_start;
    }

    // Find the end of the line (including semicolon and newline)
    size_t line_end = macro_pos + macro_pattern.size();
    while (line_end < source_text.size() &&
           (source_text[line_end] == ' ' || source_text[line_end] == '\t')) {
      ++line_end;
    }
    if (line_end < source_text.size() && source_text[line_end] == ';') {
      ++line_end;
    }
    while (line_end < source_text.size() && source_text[line_end] != '\n') {
      ++line_end;
    }
    if (line_end < source_text.size() && source_text[line_end] == '\n') {
      ++line_end;
    }

    // Check if this is the only thing in a section (private: or protected:)
    // If so, we should remove the entire section label too
    size_t remove_start = line_start;
    size_t remove_end = line_end;

    // Look backwards for an access specifier (private: or protected:) on its own line
    size_t search_pos = line_start;
    bool found_access_label = false;
    size_t access_line_start = 0;

    // Skip backwards over blank lines to find the access specifier
    while (search_pos > 0) {
      // Find start of previous line
      size_t prev_line_end = search_pos;
      if (prev_line_end > 0 && source_text[prev_line_end - 1] == '\n') {
        --prev_line_end;
      }
      size_t prev_line_start = prev_line_end;
      while (prev_line_start > 0 && source_text[prev_line_start - 1] != '\n') {
        --prev_line_start;
      }

      // Get the content of this line (trimmed)
      StringRef prev_line = source_text.substr(prev_line_start,
                                                prev_line_end - prev_line_start);
      StringRef trimmed = prev_line.trim();

      if (trimmed.empty()) {
        // Blank line, continue searching
        search_pos = prev_line_start;
        continue;
      } else if (trimmed == "private:" || trimmed == "protected:") {
        // Found access specifier label
        found_access_label = true;
        access_line_start = prev_line_start;
        break;
      } else {
        // Non-blank, non-access-specifier content - stop searching
        break;
      }
    }

    // Check if there's nothing after the macro line until the closing brace
    // or until the next access specifier
    bool nothing_after_macro = true;
    size_t after_macro = line_end;
    while (after_macro < source_text.size()) {
      char c = source_text[after_macro];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++after_macro;
        continue;
      }
      if (c == '}') {
        // Closing brace - nothing substantive after macro
        break;
      }
      // Found some other content
      nothing_after_macro = false;
      break;
    }

    // If we found an access label with only whitespace before macro,
    // and nothing after macro, remove the entire section including the label
    // and one preceding newline (to eliminate the blank line effect).
    bool removing_entire_section = false;
    if (found_access_label && nothing_after_macro) {
      removing_entire_section = true;
      remove_start = access_line_start;
      // Remove just ONE preceding newline to eliminate the blank line.
      // Don't remove two newlines as that would remove the line ending
      // of the previous content line.
      while (remove_start > 0 &&
             (source_text[remove_start - 1] == ' ' ||
              source_text[remove_start - 1] == '\t')) {
        --remove_start;
      }
      if (remove_start > 0 && source_text[remove_start - 1] == '\n') {
        --remove_start;
      }
    } else {
      // Not removing the entire section, but still remove any preceding blank
      // lines to avoid leaving trailing whitespace in the section.
      // Look backwards from line_start for blank lines.
      size_t check_pos = line_start;
      while (check_pos > 0) {
        // Find start of previous line
        size_t prev_line_end = check_pos;
        if (prev_line_end > 0 && source_text[prev_line_end - 1] == '\n') {
          --prev_line_end;
        }
        size_t prev_line_start = prev_line_end;
        while (prev_line_start > 0 && source_text[prev_line_start - 1] != '\n') {
          --prev_line_start;
        }

        // Check if this line is blank (only whitespace)
        StringRef prev_line = source_text.substr(prev_line_start,
                                                  prev_line_end - prev_line_start);
        if (prev_line.trim().empty()) {
          // Blank line - include it in the removal
          remove_start = prev_line_start;
          check_pos = prev_line_start;
        } else {
          // Non-blank line - stop
          break;
        }
      }
    }

    // Calculate absolute source locations for removal
    SourceLocation remove_start_loc = start_loc.getLocWithOffset(remove_start);
    SourceLocation remove_end_loc = start_loc.getLocWithOffset(remove_end);

    // Find insertion point in public section
    // Look for the last constructor or destructor in the public section
    SourceLocation insert_loc;
    SourceLocation public_label_loc;
    SourceLocation last_public_ctor_dtor_end;
    std::string indent = "  ";  // Default indentation

    for (const auto* decl : class_decl->decls()) {
      // Track public access specifier
      if (const auto* access = dyn_cast<AccessSpecDecl>(decl)) {
        if (access->getAccess() == AS_public) {
          public_label_loc = access->getEndLoc();
        }
      }

      // Track last constructor/destructor in public section
      // Skip implicit declarations (like the deleted copy ctor from the macro)
      if (decl->getAccess() == AS_public && !decl->isImplicit()) {
        if (isa<CXXConstructorDecl>(decl) || isa<CXXDestructorDecl>(decl)) {
          // Find the end of this declaration (including semicolon for declarations)
          SourceLocation decl_end = decl->getEndLoc();
          if (decl_end.isValid()) {
            // For declarations with bodies, use the end of the body
            // For declarations without bodies, find the semicolon
            SourceLocation potential_end = Lexer::getLocForEndOfToken(
                decl_end, 0, sm, opts);

            // Check if there's a semicolon
            Token tok;
            if (!Lexer::getRawToken(potential_end, tok, sm, opts, true)) {
              if (tok.is(tok::semi)) {
                potential_end = tok.getEndLoc();
              }
            }
            last_public_ctor_dtor_end = potential_end;

            // Detect indentation from this declaration
            SourceLocation decl_start = decl->getBeginLoc();
            if (decl_start.isValid()) {
              unsigned decl_offset = sm.getFileOffset(decl_start);
              unsigned start_offset = sm.getFileOffset(start_loc);
              if (decl_offset > start_offset) {
                size_t rel_offset = decl_offset - start_offset;
                // Find start of line containing this declaration
                size_t line_start = rel_offset;
                while (line_start > 0 && source_text[line_start - 1] != '\n') {
                  --line_start;
                }
                // Extract indentation
                indent = "";
                for (size_t i = line_start; i < rel_offset; ++i) {
                  char c = source_text[i];
                  if (c == ' ' || c == '\t') {
                    indent += c;
                  } else {
                    break;
                  }
                }
              }
            }
          }
        }
      }
    }

    // Determine insertion point and what to insert
    std::string deleted_decls;
    bool need_public_label = false;

    if (last_public_ctor_dtor_end.isValid()) {
      // Insert after last constructor/destructor in public section
      insert_loc = last_public_ctor_dtor_end;
      deleted_decls = "\n\n" + indent + class_name + "(const " + class_name +
                      "&) = delete;\n" + indent + class_name +
                      "& operator=(const " + class_name + "&) = delete;";
    } else if (public_label_loc.isValid()) {
      // Insert after public: label
      insert_loc = Lexer::getLocForEndOfToken(public_label_loc, 0, sm, opts);
      deleted_decls = "\n" + indent + class_name + "(const " + class_name +
                      "&) = delete;\n" + indent + class_name +
                      "& operator=(const " + class_name + "&) = delete;";
    } else {
      // No public section - create one after the opening brace
      SourceLocation brace_loc = class_decl->getBraceRange().getBegin();
      if (brace_loc.isInvalid()) {
        llvm::outs() << "# SKIPPED (no brace location): "
                     << sm.getFilename(class_decl->getBeginLoc()).str() << ":"
                     << sm.getSpellingLineNumber(class_decl->getBeginLoc())
                     << " class " << class_name << "\n";
        return;
      }
      insert_loc = Lexer::getLocForEndOfToken(brace_loc, 0, sm, opts);
      need_public_label = true;
      deleted_decls = "\n public:\n  " + class_name + "(const " + class_name +
                      "&) = delete;\n  " + class_name +
                      "& operator=(const " + class_name + "&) = delete;\n";
    }

    // Generate the removal edit
    CharSourceRange remove_range = CharSourceRange::getCharRange(
        remove_start_loc, remove_end_loc);
    output_helper_.Replace(remove_range, "", sm, opts);

    // Generate the insertion edit
    CharSourceRange insert_range = CharSourceRange::getCharRange(
        insert_loc, insert_loc);
    output_helper_.Replace(insert_range, deleted_decls, sm, opts);
  }

 private:
  OutputHelper& output_helper_;
  std::set<unsigned> processed_locations_;
};

}  // namespace

int main(int argc, const char* argv[]) {
  auto expected_parser = clang::tooling::CommonOptionsParser::create(
      argc, argv, rewriter_category);
  if (!expected_parser) {
    llvm::errs() << expected_parser.takeError();
    return 1;
  }
  clang::tooling::CommonOptionsParser& options = expected_parser.get();
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  OutputHelper output_helper;
  MatchFinder match_finder;
  ContainsRewriter contains_rewriter(&output_helper);
  StructuredBindingsRewriter structured_bindings_rewriter(&output_helper);
  IteratorLoopRewriter iterator_loop_rewriter(&output_helper);
  DisallowCopyRewriter disallow_copy_rewriter(&output_helper);

  if (isTransformEnabled("contains", EnableContains)) {
    // Matcher for find() call on associative container
    auto findCallMatcher =
        cxxMemberCallExpr(
            on(expr(isAssociativeContainer()).bind("findContainer")),
            callee(cxxMethodDecl(hasName("find"))))
            .bind("findCall");

    // Matcher for end() call on associative container
    auto endCallMatcher =
        cxxMemberCallExpr(
            on(expr(isAssociativeContainer()).bind("endContainer")),
            callee(cxxMethodDecl(hasName("end"))))
            .bind("endCall");

    // Match find() != end() or find() == end()
    // IMPORTANT: Iterator comparisons use overloaded operators, not built-in!
    auto comparisonMatcher = cxxOperatorCallExpr(
        anyOf(hasOverloadedOperatorName("!="),
              hasOverloadedOperatorName("==")),
        anyOf(hasArgument(0, findCallMatcher), hasArgument(1, findCallMatcher)),
        anyOf(hasArgument(0, endCallMatcher), hasArgument(1, endCallMatcher)));

    match_finder.addMatcher(comparisonMatcher.bind("comparison"),
                            &contains_rewriter);
  }

  if (isTransformEnabled("count-patterns", EnableCountPatterns)) {
    // Matcher for count() call on associative container
    auto countCallMatcher =
        cxxMemberCallExpr(
            on(expr(isAssociativeContainer()).bind("countContainer")),
            callee(cxxMethodDecl(hasName("count"))))
            .bind("countCall");

    // count() != 0 or count() > 0 or count() == 0
    // NOTE: count() comparisons use binaryOperator (not cxxOperatorCallExpr)
    // because count() returns size_type (unsigned long), and integer comparison
    // uses built-in ops.
    // IMPORTANT: Must use ignoringImpCasts() because the integer literal 0 is
    // implicitly cast from 'int' to 'size_type' (unsigned long).
    auto countComparisonMatcher = binaryOperator(
        anyOf(hasOperatorName("!="), hasOperatorName(">"),
              hasOperatorName("==")),
        hasEitherOperand(countCallMatcher),
        hasEitherOperand(ignoringImpCasts(integerLiteral(equals(0)))));

    match_finder.addMatcher(countComparisonMatcher.bind("countComparison"),
                            &contains_rewriter);

    // count() used directly as boolean (implicit != 0)
    // In C++ code, check for ImplicitCastExpr with CK_IntegralToBoolean cast
    // kind.
    auto countBooleanMatcher = implicitCastExpr(has(countCallMatcher));

    match_finder.addMatcher(countBooleanMatcher.bind("countBoolean"),
                            &contains_rewriter);
  }

  if (isTransformEnabled("structured-bindings", EnableStructuredBindings)) {
    // Match range-based for loops - we'll filter for pair-like types in the callback
    auto forRangeMatcher = cxxForRangeStmt().bind("forRange");

    match_finder.addMatcher(forRangeMatcher, &structured_bindings_rewriter);
  }

  if (isTransformEnabled("iterator-loops", EnableIteratorLoops)) {
    // Match traditional for loops - we'll filter for iterator patterns in the callback
    auto forStmtMatcher = forStmt().bind("forStmt");

    match_finder.addMatcher(forStmtMatcher, &iterator_loop_rewriter);
  }

  if (isTransformEnabled("disallow-copy", EnableDisallowCopy)) {
    // Match all class definitions - we'll search for the macro in the callback
    auto classDefMatcher = cxxRecordDecl(isDefinition()).bind("classDecl");

    match_finder.addMatcher(classDefMatcher, &disallow_copy_rewriter);
  }

  // Pass output_helper as SourceFileCallbacks to handle per-file setup/teardown
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder, &output_helper);
  return tool.run(factory.get());
}
