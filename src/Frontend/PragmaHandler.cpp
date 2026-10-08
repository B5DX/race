#include "PragmaHandler.h"
#include "clang/Basic/Diagnostic.h"
using namespace clang;

void MyPragmaHandler::HandlePragma(Preprocessor &PP, PragmaIntroducer, Token &) {
    Token tok;
    PP.Lex(tok);
    auto &sm = PP.getSourceManager();
    if (!sm.isWrittenInMainFile(tok.getLocation())) return;
    auto report = [&](const char *message) {
        auto &diag = PP.getDiagnostics();
        diag.Report(tok.getLocation(), diag.getCustomDiagID(DiagnosticsEngine::Error, "%0")) << message;
    };
    if (!tok.is(tok::identifier)) { report("Expected #pragma race begin, end, or var"); return; }
    auto name = tok.getIdentifierInfo();
    if (name->isStr("begin")) {
        if (hasUnclosedRegion()) { report("Nested #pragma race regions are not supported"); return; }
        srcLocPragmaBegin.push_back(tok.getLocation());
    } else if (name->isStr("end")) {
        if (!hasUnclosedRegion()) { report("#pragma race end has no matching begin"); return; }
        srcLocPragmaEnd.push_back(tok.getLocation());
    } else if (name->isStr("var")) {
        if (hasUnclosedRegion()) { report("Place #pragma race var before the region"); return; }
        srcLocVar.push_back(tok.getLocation());
    } else report("Unknown #pragma race directive");
    Token trailing;
    PP.Lex(trailing);
    if (!trailing.is(tok::eod)) report("Unexpected tokens after #pragma race directive");
}
