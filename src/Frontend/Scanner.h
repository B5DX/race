#ifndef SCANNER_H
#define SCANNER_H
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/FrontendAction.h"
#include "Frontend/PragmaHandler.h"
#include "IR/IR.h"

namespace clang {
struct Scanner : public ASTFrontendAction {
    bool BeginSourceFileAction(CompilerInstance &CI) override;
    std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override;

  private:
    std::unique_ptr<ast_matchers::MatchFinder> ForStmtFinder;
    std::unique_ptr<ast_matchers::MatchFinder::MatchCallback> MatchCB;
    
    /**
     * @brief Pragma handler used to handle the pragma while save the pragma region location. 
     * 
     * @warning This pointer can't be wrapped by smart pointer.
     * 
     * @details This pointer can't be wrapped by smart pointer, because the pragma handler will be deleted 
     * by the preprocessor automatically at some time (Using gdb backtrace, we found that the preprocessor might 
     * be destructed by ~CompilerInstance. It won't affect our permission before it is deleted).
     * If it is wrapped by smart pointer, the destructor of the smart pointer will cause the double free error.
     */
    MyPragmaHandler *avnPragmaHandler;
};
} // end namespace clang

#endif
