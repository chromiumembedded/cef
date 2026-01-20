set(LLVM_LINK_COMPONENTS
  BitReader
  MCParser
  Option
  X86AsmParser
  X86CodeGen
  )

add_llvm_executable(cef_cpp_rewriter
  CefCppRewriter.cpp
  OutputHelper.h
  OutputHelper.cpp
  )

target_link_libraries(cef_cpp_rewriter
  clangAST
  clangASTMatchers
  clangAnalysis
  clangBasic
  clangDriver
  clangEdit
  clangFrontend
  clangLex
  clangParse
  clangSema
  clangSerialization
  clangTooling
  )

cr_install(TARGETS cef_cpp_rewriter RUNTIME DESTINATION bin)
