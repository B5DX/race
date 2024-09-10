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
            // Check if the #pragma past end is after #pragma past begin
            if (srcLocBegin.back().getRawEncoding() > srcLocEnd.back().getRawEncoding()) {
                llvm::errs() << "The #pragma past end is before #pragma past begin\n";
                return;
            }
            // Check if the #pragma past end is in the same file as #pragma past begin
            // if (PP.getSourceManager().getFileID(srcLocBeginEnd.back().first) != PP.getSourceManager().getFileID(srcLocBeginEnd.back().second)) {
            //     llvm::errs() << "The #pragma past end is in a different file from #pragma past begin\n";
            //     return;
            // }
            // TODO: DO SOMETHING
            // print the range of the #pragma past begin and #pragma past end
            // llvm::errs() << "The range of the #pragma past begin and #pragma past end is from "
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