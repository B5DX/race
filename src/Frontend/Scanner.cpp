#include "PragmaHandler.h"
#include "common.h"
#include "Frontend/Scanner.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

struct MatchForStmtCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
    MatchForStmtCallback(unsigned DiagWarnPotentialFor, MyPragmaHandler *avnPragmaHandler)
        : diag_warn_potential_for(DiagWarnPotentialFor), avnPragmaHandler(avnPragmaHandler) {}
    void run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;

  private:
    MyPragmaHandler *avnPragmaHandler;
    std::shared_ptr<IR::ForStmt> for_stmt;
    unsigned diag_warn_potential_for;
};

bool Scanner::BeginSourceFileAction(CompilerInstance &CI) {
    auto &PP = CI.getPreprocessor();
    avnPragmaHandler = new MyPragmaHandler();
    PP.AddPragmaHandler(avnPragmaHandler);

    // print include paths
    // const auto &headerSearchOpts = CI.getHeaderSearchOpts();
    // for (const auto &path : headerSearchOpts.UserEntries) {
    //     outs() << "Include path: " << path.Path << "\n";
    // }

    return true;
}

void MatchForStmtCallback::run(const MatchFinder::MatchResult &Result) {
    const auto &Nodes = Result.Nodes;
    auto &Diag = Result.Context->getDiagnostics();

    const auto *for_stmt = Nodes.getNodeAs<ForStmt>("for_stmt");
    if (for_stmt) {
        if (avnPragmaHandler->ifBelongsToPragma(for_stmt->getBeginLoc())) {
            Diag.Report(for_stmt->getBeginLoc(), diag_warn_potential_for);
        }
    }
}

std::unique_ptr<ASTConsumer>
Scanner::CreateASTConsumer(CompilerInstance &CI,
                                          StringRef InFile) {
    // Create custom diagnostic message
    assert(CI.hasASTContext() && "No ASTContext??");
    auto &Diag = CI.getASTContext().getDiagnostics();
    auto DiagWarnMain = Diag.getCustomDiagID(
        DiagnosticsEngine::Warning,
        "this for statement is in past pragma:");

    ForStmtFinder = std::make_unique<MatchFinder>();

    MatchCB = std::make_unique<MatchForStmtCallback>(DiagWarnMain, avnPragmaHandler);
    ForStmtFinder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                    forStmt().bind("for_stmt")),
                            MatchCB.get());

    return std::move(ForStmtFinder->newASTConsumer());
}
