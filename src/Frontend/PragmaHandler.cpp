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
            srcLocBegin.push_back(Tok.getLocation());
        } else if (II->isStr("end")) {
            srcLocEnd.push_back(Tok.getLocation());
            // Check if the #pragma race end is after #pragma race begin
            if (srcLocBegin.back().getRawEncoding() > srcLocEnd.back().getRawEncoding()) {
                llvm::errs() << "The #pragma race end is before #pragma race begin\n";
                return;
            }
            // Check if the #pragma race end is in the same file as #pragma race begin
            if (PP.getSourceManager().getFileID(srcLocBegin.back()) != PP.getSourceManager().getFileID(srcLocEnd.back())) {
                llvm::errs() << "The #pragma race end is in a different file from #pragma race begin\n";
                return;
            }
            // TODO: DO SOMETHING
            // print the range of the #pragma race begin and #pragma race end
            // llvm::errs() << "The range of the #pragma race begin and #pragma race end is from "
            //              << PP.getSourceManager().getSpellingLineNumber(srcLocBegin.back()) << ":"
            //              << PP.getSourceManager().getSpellingColumnNumber(srcLocBegin.back()) << " to "
            //              << PP.getSourceManager().getSpellingLineNumber(srcLocEnd.back()) << ":"
            //              << PP.getSourceManager().getSpellingColumnNumber(srcLocEnd.back()) << "\n";
        } else {
            llvm::errs() << "Unknown pragma: " << II->getName() << "\n";
        }
    } else {
        llvm::errs() << "Unknown pragma\n";
    }
}