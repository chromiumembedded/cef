// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <set>
#include <string>

#include "OutputHelper.h"
#include "clang/AST/ASTContext.h"
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

  // Pass output_helper as SourceFileCallbacks to handle per-file setup/teardown
  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder, &output_helper);
  return tool.run(factory.get());
}
