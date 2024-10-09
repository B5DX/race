#include "Frontend/Scanner.h"
#include "PragmaHandler.h"
#include "common.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"
#include "Solver/Extractor.h"

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

struct MatchForStmtCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
    MatchForStmtCallback(unsigned DiagWarnPotentialFor, MyPragmaHandler *avnPragmaHandler)
        : diag_warn_potential_for(DiagWarnPotentialFor), avnPragmaHandler(avnPragmaHandler) {}
    void run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;

  private:
    MyPragmaHandler *avnPragmaHandler;
    unsigned diag_warn_potential_for;
    std::shared_ptr<ir::ForStmt> for_stmt = nullptr;
    int avn_cnt = 0;
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

const clang::ForStmt *outer_for_stmt = nullptr;

void MatchForStmtCallback::run(const MatchFinder::MatchResult &Result) {
    const auto &Nodes = Result.Nodes;
    auto &Diag = Result.Context->getDiagnostics();

    const auto *for_stmt = Nodes.getNodeAs<clang::ForStmt>("for_stmt");
    if (for_stmt) {
        if (avnPragmaHandler->ifBelongsToPragma(for_stmt->getBeginLoc())) {
            if (!outer_for_stmt) {
                outer_for_stmt = for_stmt;
            } else {
                // Escape the inner for statement.
                if (for_stmt->getBeginLoc() > outer_for_stmt->getBeginLoc() ||
                    for_stmt->getEndLoc() < outer_for_stmt->getEndLoc()) {
                    return; 
                } else {
                    outer_for_stmt = for_stmt;
                }
            }
            Diag.Report(for_stmt->getBeginLoc(), diag_warn_potential_for);

            shared_ptr<ir::Block> outer_block = std::make_shared<ir::Block>(nullptr);
            this->for_stmt = std::make_shared<ir::ForStmt>(for_stmt);
            outer_block->push_back(this->for_stmt);
            llvm::outs() << this->for_stmt->to_string() << "\n";
            llvm::outs() << "----------------------\n";
            vector<ir::Block> blocks = this->for_stmt->getInnermostBlocks();
            
            for (const auto &block : blocks) {
                extract::extract(outer_block, block, this->avn_cnt++);
            }
            llvm::outs() << outer_block->to_string() << "\n";
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
