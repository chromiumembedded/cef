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

  if (EnableContains) {
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

  if (EnableCountPatterns) {
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

  if (EnableStructuredBindings) {
    // Match range-based for loops - we'll filter for pair-like types in the callback
    auto forRangeMatcher = cxxForRangeStmt().bind("forRange");

    match_finder.addMatcher(forRangeMatcher, &structured_bindings_rewriter);
  }

  // Pass output_helper as SourceFileCallbacks to handle per-file setup/teardown
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder, &output_helper);
  return tool.run(factory.get());
}
