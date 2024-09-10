#ifndef TERNARY_CONVERTER_H
#define TERNARY_CONVERTER_H
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/FrontendAction.h"
#include "common.h"

namespace clang {
struct FAction : public ASTFrontendAction {
    bool BeginSourceFileAction(CompilerInstance &CI) override;
    std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override;

  private:
    std::unique_ptr<ast_matchers::MatchFinder> ASTFinder;
    std::unique_ptr<ast_matchers::MatchFinder::MatchCallback> ReturnMatchCB,
        AssignMatchCB;
};
} // end namespace clang
#endif
