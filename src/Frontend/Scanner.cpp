#include "Frontend/Scanner.h"
#include "PragmaHandler.h"
#include "common.h"
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

const ForStmt *outter_for_stmt = nullptr;

void MatchForStmtCallback::run(const MatchFinder::MatchResult &Result) {
    const auto &Nodes = Result.Nodes;
    auto &Diag = Result.Context->getDiagnostics();

    const auto *for_stmt = Nodes.getNodeAs<ForStmt>("for_stmt");
    if (for_stmt) {
        if (avnPragmaHandler->ifBelongsToPragma(for_stmt->getBeginLoc())) {
            if (!outter_for_stmt) {
                outter_for_stmt = for_stmt;
            } else {
                // Escape the inner for statement.
                if (for_stmt->getBeginLoc() > outter_for_stmt->getBeginLoc() ||
                    for_stmt->getEndLoc() < outter_for_stmt->getEndLoc()) {
                    return; 
                } else {
                    outter_for_stmt = for_stmt;
                }
            }
            Diag.Report(for_stmt->getBeginLoc(), diag_warn_potential_for);
            this->for_stmt = std::make_shared<IR::ForStmt>(for_stmt);
            llvm::outs() << this->for_stmt->to_string() << "\n";
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
        "This for statement is in avn pragma: ");

    ForStmtFinder = std::make_unique<MatchFinder>();

    MatchCB = std::make_unique<MatchForStmtCallback>(DiagWarnMain, avnPragmaHandler);
    ForStmtFinder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                       forStmt().bind("for_stmt")),
                              MatchCB.get());

    return std::move(ForStmtFinder->newASTConsumer());
}
