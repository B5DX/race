#include "Frontend/Scanner.h"
#include "common.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "Solver/Extractor.h"
#include <algorithm>

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

namespace {
void validateExpression(const Expr *expr, bool index = false) {
    if (auto ref = dyn_cast<DeclRefExpr>(expr)) {
        if (auto var = dyn_cast<VarDecl>(ref->getDecl())) {
            if (var->getType()->isReferenceType())
                throw std::runtime_error("Reference aliases are not supported in RACE regions");
        }
    }
    if (expr->getType().isVolatileQualified())
        throw std::runtime_error("Volatile accesses are not supported");
    if (auto array = dyn_cast<ArraySubscriptExpr>(expr)) {
        validateExpression(array->getBase(), true);
        validateExpression(array->getIdx(), true);
        return;
    }
    if (auto op = dyn_cast<BinaryOperator>(expr)) {
        if (op->isAssignmentOp() || op->isCommaOp())
            throw std::runtime_error("Side effects inside expressions are not supported");
        if (!index && !expr->getType()->isSpecificBuiltinType(BuiltinType::Double))
            throw std::runtime_error("Arithmetic in RACE regions must use double precision");
    }
    if (auto call = dyn_cast<CallExpr>(expr)) {
        auto callee = call->getDirectCallee();
        if (index || !callee || !callee->getBuiltinID() || call->getNumArgs() != 1 ||
            (callee->getNameAsString() != "sin" && callee->getNameAsString() != "cos") ||
            !call->getType()->isSpecificBuiltinType(BuiltinType::Double))
            throw std::runtime_error("Only pure double sin/cos calls are supported");
        validateExpression(call->getArg(0));
        return;
    }
    if (auto cast = dyn_cast<ImplicitCastExpr>(expr)) {
        if (cast->getCastKind() != CK_LValueToRValue &&
            cast->getCastKind() != CK_ArrayToPointerDecay &&
            cast->getCastKind() != CK_FunctionToPointerDecay &&
            cast->getCastKind() != CK_NoOp &&
            !((cast->getCastKind() == CK_IntegralToFloating || cast->getCastKind() == CK_FloatingCast) &&
              cast->getType()->isSpecificBuiltinType(BuiltinType::Double)))
            throw std::runtime_error("This implicit conversion is not supported in a RACE region");
    }
    for (auto child : expr->children())
        if (auto sub = dyn_cast_or_null<Expr>(child)) validateExpression(sub, index);
}

void validateLoop(const clang::ForStmt *loop, string &declarations, std::set<string> &loop_vars) {
    const Expr *init_value = nullptr;
    const DeclRefExpr *loop_ref = nullptr;
    const VarDecl *loop_decl = nullptr;
    if (auto init = dyn_cast_or_null<DeclStmt>(loop->getInit())) {
        if (!init->isSingleDecl()) throw std::runtime_error("Use one loop variable per for statement");
        auto var = dyn_cast<VarDecl>(init->getSingleDecl());
        if (!var || !var->getType()->isSignedIntegerType() || !var->hasInit())
            throw std::runtime_error("Use an initialized integer loop variable");
        loop_decl = var;
        init_value = var->getInit();
        declarations += var->getType().getAsString() + " " + var->getNameAsString() + ";\n";
    }
    if (!loop_decl) {
        auto init = dyn_cast_or_null<BinaryOperator>(loop->getInit());
        if (!init || init->getOpcode() != BO_Assign)
            throw std::runtime_error("Use an initialized signed integer loop variable");
        loop_ref = dyn_cast<DeclRefExpr>(init->getLHS()->IgnoreParenImpCasts());
        if (!loop_ref || !loop_ref->getType()->isSignedIntegerType())
            throw std::runtime_error("Use a signed integer loop variable");
        loop_decl = dyn_cast<VarDecl>(loop_ref->getDecl());
        init_value = init->getRHS();
    }
    if (!loop_decl || !loop_vars.insert(loop_decl->getNameAsString()).second)
        throw std::runtime_error("Loop variables must have distinct names");
    auto cond = dyn_cast_or_null<BinaryOperator>(loop->getCond());
    if (!cond || !cond->getLHS()->getType()->isSignedIntegerType() ||
        !cond->getRHS()->getType()->isSignedIntegerType())
        throw std::runtime_error("Loop bounds must use signed integer comparisons");
    validateExpression(init_value, true);
    validateExpression(cond->getRHS(), true);
    std::function<void(const Stmt *)> check_bounds = [&](const Stmt *stmt) {
        if (auto ref = dyn_cast<DeclRefExpr>(stmt)) {
            if (loop_vars.count(ref->getNameInfo().getAsString()))
                throw std::runtime_error("Loop bounds must be independent of loop variables");
        }
        for (auto child : stmt->children()) if (child) check_bounds(child);
    };
    check_bounds(init_value);
    check_bounds(cond->getRHS());
    vector<const Stmt *> body;
    if (auto compound = dyn_cast<CompoundStmt>(loop->getBody()))
        body.assign(compound->body_begin(), compound->body_end());
    else body.push_back(loop->getBody());
    if (body.empty()) throw std::runtime_error("Empty RACE loops are not supported");
    for (auto stmt : body) {
        if (auto nested = dyn_cast<clang::ForStmt>(stmt)) {
            if (body.size() != 1) throw std::runtime_error("RACE requires a perfect loop nest");
            validateLoop(nested, declarations, loop_vars);
        } else if (auto assign = dyn_cast<BinaryOperator>(stmt)) {
            if (assign->getOpcode() != BO_Assign ||
                !assign->getLHS()->getType()->isSpecificBuiltinType(BuiltinType::Double))
                throw std::runtime_error("Loop bodies must contain plain assignments to double values");
            validateExpression(assign->getLHS());
            validateExpression(assign->getRHS());
        } else throw std::runtime_error("Unsupported loop-body statement; use a perfect nest of assignments");
    }
}

struct MatchForStmtCallback : MatchFinder::MatchCallback {
    MatchForStmtCallback(MyPragmaHandler *handler, CompilerInstance &ci)
        : handler(handler), ci(ci), rewrite(ci.getSourceManager(), ci.getLangOpts()) {}

    void error(SourceLocation loc, const string &message) {
        auto &diag = ci.getDiagnostics();
        diag.Report(loc, diag.getCustomDiagID(DiagnosticsEngine::Error, "RACE: %0")) << message;
    }

    void run(const MatchFinder::MatchResult &result) override {
        auto loop = result.Nodes.getNodeAs<clang::ForStmt>("for_stmt");
        auto &sm = ci.getSourceManager();
        if (ci.getDiagnostics().hasErrorOccurred() || !sm.isWrittenInMainFile(loop->getBeginLoc()) ||
            !handler->ifBelongsToPragma(loop->getBeginLoc())) return;
        for (auto range : processed)
            if (loop->getBeginLoc() >= range.getBegin() && loop->getEndLoc() <= range.getEnd()) return;
        try {
            auto var = handler->getMatchedVarLoc(loop->getBeginLoc());
            if (var.isInvalid() || !sm.isWrittenInMainFile(var))
                throw std::runtime_error("Place #pragma race var before the region in the same scope");
            // Require the declaration marker in the enclosing lexical scope.
            auto parents = ci.getASTContext().getParents(*loop);
            while (!parents.empty() && !parents[0].get<CompoundStmt>())
                parents = ci.getASTContext().getParents(parents[0]);
            if (parents.empty() || var <= parents[0].get<CompoundStmt>()->getBeginLoc())
                throw std::runtime_error("#pragma race var must be in the loop's enclosing scope");
            auto scope = parents[0].get<CompoundStmt>();
            std::function<bool(const Stmt *)> marker_in_nested_scope = [&](const Stmt *stmt) {
                if (auto nested = dyn_cast<CompoundStmt>(stmt))
                    if (nested != scope && var > nested->getBeginLoc() && var < nested->getEndLoc()) return true;
                for (auto child : stmt->children())
                    if (child && marker_in_nested_scope(child)) return true;
                return false;
            };
            if (marker_in_nested_scope(scope))
                throw std::runtime_error("#pragma race var is in a different lexical scope");
            string declarations;
            std::set<string> loop_vars;
            validateLoop(loop, declarations, loop_vars);
            // Avoid colliding with identifiers already present in the input.
            auto source = sm.getBufferData(sm.getMainFileID());
            while (source.contains("iv_" + std::to_string(count) + "_")) ++count;
            auto block = make_shared<ir::AVNBlock>(make_shared<ir::ForStmt>(loop, ci.getASTContext()));
            extract::extract(block, count++);
            for (const auto &decl : block->getAVNVarDecl()) var_decls[var] += decl + "\n";
            auto range = Lexer::makeFileCharRange(CharSourceRange::getTokenRange(loop->getSourceRange()), sm, ci.getLangOpts());
            if (range.isInvalid()) throw std::runtime_error("Cannot rewrite a loop expanded from a macro");
            // An expression body's end location excludes its trailing semicolon.
            auto text = sm.getBufferData(sm.getMainFileID());
            unsigned end = sm.getFileOffset(range.getEnd());
            unsigned next = end;
            while (next < text.size() && (text[next] == ' ' || text[next] == '\t')) ++next;
            if (!text.slice(sm.getFileOffset(range.getBegin()), end).rtrim().ends_with("}") && next < text.size() && text[next] == ';') range.setEnd(sm.getLocForStartOfFile(sm.getMainFileID()).getLocWithOffset(next + 1));
            if (rewrite.ReplaceText(range, "{\n" + declarations + block->to_string() + "\n}"))
                throw std::runtime_error("Cannot replace this source range");
            processed.push_back(loop->getSourceRange());
        } catch (const std::exception &e) { error(loop->getBeginLoc(), e.what()); }
    }

    void onEndOfTranslationUnit() override {
        auto &sm = ci.getSourceManager();
        if (handler->hasUnclosedRegion()) error(handler->getSrcLocPragmaBegin().back(), "Missing #pragma race end");
        if (ci.getDiagnostics().hasErrorOccurred()) return;
        auto id = sm.getMainFileID();
        auto source = sm.getBufferData(id);
        for (const auto &entry : var_decls) {
            unsigned pos = sm.getFileOffset(entry.first);
            auto start = source.rfind('\n', pos);
            start = start == StringRef::npos ? 0 : start + 1;
            auto end = source.find('\n', pos);
            if (end == StringRef::npos) end = source.size();
            else ++end;
            if (rewrite.ReplaceText(sm.getLocForStartOfFile(id).getLocWithOffset(start), end - start, entry.second)) {
                error(entry.first, "Cannot replace #pragma race var"); return;
            }
        }
        auto file = sm.getFileEntryForID(id);
        if (!file) return;
        string path = file->tryGetRealPathName().str();
        if (path.empty()) path = sm.getFileEntryRefForID(id)->getName().str();
        auto slash = path.find_last_of("/\\");
        const string output = path.substr(0, slash == string::npos ? 0 : slash + 1) +
                              "race_" + path.substr(slash == string::npos ? 0 : slash + 1);
        int fd;
        SmallString<256> temporary;
        auto ec = sys::fs::createUniqueFile(output + ".tmp-%%%%%%", fd, temporary);
        if (ec) { error({}, "Cannot create output: " + ec.message()); return; }
        raw_fd_ostream stream(fd, true);
        if (auto buffer = rewrite.getRewriteBufferFor(id)) buffer->write(stream);
        else stream << source;
        stream.close();
        if (stream.has_error()) {
            stream.clear_error();
            sys::fs::remove(temporary);
            error({}, "Cannot write output file"); return;
        }
        ec = sys::fs::rename(temporary, output);
        if (ec) {
            sys::fs::remove(temporary);
            error({}, "Cannot save output: " + ec.message()); return;
        }
        outs() << "Saved " << output << "\n";
    }

    MyPragmaHandler *handler;
    CompilerInstance &ci;
    Rewriter rewrite;
    int count = 0;
    vector<SourceRange> processed;
    std::map<SourceLocation, string> var_decls;
};
} // namespace

bool Scanner::BeginSourceFileAction(CompilerInstance &ci) {
    avnPragmaHandler = new MyPragmaHandler();
    ci.getPreprocessor().AddPragmaHandler(avnPragmaHandler);
    return true;
}

std::unique_ptr<ASTConsumer> Scanner::CreateASTConsumer(CompilerInstance &ci, StringRef) {
    ForStmtFinder = std::make_unique<MatchFinder>();
    MatchCB = std::make_unique<MatchForStmtCallback>(avnPragmaHandler, ci);
    ForStmtFinder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource, forStmt().bind("for_stmt")), MatchCB.get());
    return ForStmtFinder->newASTConsumer();
}
