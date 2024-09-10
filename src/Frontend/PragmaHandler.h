#ifndef PRAGMAHANDLER_H
#define PRAGMAHANDLER_H
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/TokenKinds.h"
#include "clang/Lex/Pragma.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Token.h"
#include "llvm/Support/raw_ostream.h"

class MyPragmaHandler : public clang::PragmaHandler {
    bool IsValidatorRegistered;
    llvm::SmallVector<clang::SourceLocation> srcLocBegin;
    llvm::SmallVector<clang::SourceLocation> srcLocEnd;

  public:
    MyPragmaHandler() : PragmaHandler("past"),
                        IsValidatorRegistered(false) {}

    void HandlePragma(clang::Preprocessor &PP, clang::PragmaIntroducer Introducer,
                      clang::Token &PragmaTok) override;

    bool ifBelongsToPragma(clang::SourceLocation Loc) {
        assert(srcLocBegin.size() == srcLocEnd.size());
        for (size_t idx = 0; idx < srcLocBegin.size(); idx++) {
            if (Loc >= srcLocBegin[idx] && Loc <= srcLocEnd[idx]) {
                return true;
            }
        }
        return false;
    }
};

#endif