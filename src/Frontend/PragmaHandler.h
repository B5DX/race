#ifndef PRAGMAHANDLER_H
#define PRAGMAHANDLER_H
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Lex/Pragma.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Token.h"
#include "llvm/Support/raw_ostream.h"

class MyPragmaHandler : public clang::PragmaHandler {
    llvm::SmallVector<clang::SourceLocation> srcLocPragmaBegin;
    llvm::SmallVector<clang::SourceLocation> srcLocPragmaEnd;
    llvm::SmallVector<clang::SourceLocation> srcLocVar;

  public:
    MyPragmaHandler() : PragmaHandler("race") {}

    bool hasUnclosedRegion() const { return srcLocPragmaBegin.size() != srcLocPragmaEnd.size(); }

    void HandlePragma(clang::Preprocessor &PP, clang::PragmaIntroducer Introducer,
                      clang::Token &PragmaTok) override;

    bool ifBelongsToPragma(clang::SourceLocation Loc) {
        if (hasUnclosedRegion()) return false;
        for (size_t idx = 0; idx < srcLocPragmaBegin.size(); idx++) {
            if (Loc >= srcLocPragmaBegin[idx] && Loc <= srcLocPragmaEnd[idx]) {
                return true;
            }
        }
        return false;
    }

    clang::SourceLocation getMatchedVarLoc(clang::SourceLocation Loc) {
        clang::SourceLocation matchedLoc(clang::SourceLocation::getFromRawEncoding(0));
        for (size_t idx = 0; idx < srcLocVar.size(); idx++) {
            if (Loc > srcLocVar[idx] && matchedLoc < srcLocVar[idx]) {
                matchedLoc = srcLocVar[idx];
            }
        }
        return matchedLoc;
    }

    const llvm::SmallVector<clang::SourceLocation> &getSrcLocPragmaBegin() const {
        return srcLocPragmaBegin;
    }

    const llvm::SmallVector<clang::SourceLocation> &getSrcLocPragmaEnd() const {
        return srcLocPragmaEnd;
    }

    const llvm::SmallVector<clang::SourceLocation> &getSrcLocVar() const {
        return srcLocVar;
    }
};

#endif