#include "PragmaHandler.h"

using namespace clang;

void MyPragmaHandler::HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer,
                                   Token &PragmaTok) {
    Token Tok;
    PP.Lex(Tok);
    if (Tok.is(tok::identifier)) {
        auto *II = Tok.getIdentifierInfo();
        // save the location of #pragma past begin and #pragma past end
        
        if (II->isStr("begin")) {
            srcLocPragmaBegin.push_back(Tok.getLocation());
        } else if (II->isStr("end")) {
            srcLocPragmaEnd.push_back(Tok.getLocation());
            // Check if the #pragma race end is after #pragma race begin
            if (srcLocPragmaBegin.back().getRawEncoding() > srcLocPragmaEnd.back().getRawEncoding()) {
                llvm::errs() << "The #pragma race end is before #pragma race begin\n";
                return;
            }
            // Check if the #pragma race end is in the same file as #pragma race begin
            if (PP.getSourceManager().getFileID(srcLocPragmaBegin.back()) != PP.getSourceManager().getFileID(srcLocPragmaEnd.back())) {
                llvm::errs() << "The #pragma race end is in a different file from #pragma race begin\n";
                return;
            }
            // TODO: DO SOMETHING
            // print the range of the #pragma race begin and #pragma race end
            // llvm::errs() << "The range of the #pragma race begin and #pragma race end is from "
            //              << PP.getSourceManager().getSpellingLineNumber(srcLocPragmaBegin.back()) << ":"
            //              << PP.getSourceManager().getSpellingColumnNumber(srcLocPragmaBegin.back()) << " to "
            //              << PP.getSourceManager().getSpellingLineNumber(srcLocPragmaEnd.back()) << ":"
            //              << PP.getSourceManager().getSpellingColumnNumber(srcLocPragmaEnd.back()) << "\n";
        } else if (II->isStr("var")) {
            srcLocVar.push_back(Tok.getLocation());
        }
        else {
            llvm::errs() << "Unknown pragma: " << II->getName() << "\n";
        }
    } else {
        llvm::errs() << "Unknown pragma\n";
    }
}