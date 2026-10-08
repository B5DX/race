#ifndef SCANNER_H
#define SCANNER_H
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/FrontendAction.h"
#include "Frontend/PragmaHandler.h"

namespace clang {
struct Scanner : public ASTFrontendAction {
    bool BeginSourceFileAction(CompilerInstance &CI) override;
    std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override;

  private:
    std::unique_ptr<ast_matchers::MatchFinder> ForStmtFinder;
    std::unique_ptr<ast_matchers::MatchFinder::MatchCallback> MatchCB;
    
    // Owned and deleted by the preprocessor; do not wrap in a smart pointer.
    MyPragmaHandler *avnPragmaHandler;
};
} // end namespace clang

#endif
