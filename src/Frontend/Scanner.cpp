#include "Frontend/Scanner.h"
#include "PragmaHandler.h"
#include "common.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"
#include "Solver/Extractor.h"
#include <vector>
#include <string>
#include <algorithm>
#include <tuple>
#include <fstream>

using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;
using std::pair, std::tuple, std::get;

struct MatchForStmtCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
    MatchForStmtCallback(unsigned DiagWarnPotentialFor, MyPragmaHandler *avnPragmaHandler, clang::CompilerInstance &CI)
        : diag_warn_potential_for(DiagWarnPotentialFor), avnPragmaHandler(avnPragmaHandler), CI(CI) {
        const auto &sourceManager = CI.getSourceManager();
        const auto &fileID = sourceManager.getMainFileID();
        const auto &file = sourceManager.getFileEntryForID(fileID);
        if (file) {
            file_path = file->tryGetRealPathName();
        }
    }
    void run(const clang::ast_matchers::MatchFinder::MatchResult &Result) override;

    void onEndOfTranslationUnit() override;

  private:
    MyPragmaHandler *avnPragmaHandler;
    clang::CompilerInstance &CI;
    string file_path;
    unsigned diag_warn_potential_for;
    std::shared_ptr<ir::ForStmt> for_stmt = nullptr;
    int avn_cnt = 0;
    vector<pair<SourceLocation, SourceLocation>> processed_for_loc;
    vector<string> processed_outputs;
    std::map<SourceLocation, string> var_decls;
};

struct Location {
    unsigned int line;
    unsigned int col;
};

string replaceRegions(const string& str,
                      const vector<pair<Location, Location>>& regions,
                      const vector<string>& replace_strs) {
    if (regions.size() != replace_strs.size()) {
        return "";
    }
    vector<int> line_offsets;
    line_offsets.push_back(0);
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\n') {
            line_offsets.push_back(i + 1);
        }
    }
    vector<tuple<int, int, string>> replacements;
    for (size_t i = 0; i < regions.size(); ++i) {
        const auto& region = regions[i];
        const Location& start_loc = region.first;
        const Location& end_loc = region.second;

        int start_line = start_loc.line - 1;
        int start_col = start_loc.col - 1;
        int end_line = end_loc.line - 1;
        int end_col = end_loc.col - 1;

        if (start_line >= static_cast<int>(line_offsets.size())) return "";
        int s = line_offsets[start_line] + start_col;
        int line_end = (start_line + 1 < line_offsets.size()) ? line_offsets[start_line + 1] : str.size();
        if (s >= line_end) return "";

        if (end_line >= static_cast<int>(line_offsets.size())) return "";
        int e = line_offsets[end_line] + end_col;
        line_end = (end_line + 1 < line_offsets.size()) ? line_offsets[end_line + 1] : str.size();
        if (e >= line_end) {
            // If end_col exceeds the max length of the line, truncate the length and replace the whole line.
            e = line_end - 1;
        }

        if (s > e) return "";

        replacements.emplace_back(s, e, replace_strs[i]);
    }
    std::sort(replacements.begin(), replacements.end(),
        [](const tuple<int, int, string>& a, const tuple<int, int, string>& b) {
            return get<0>(a) < get<0>(b);
        });

    string result;
    size_t pos = 0;
    for (const auto& repl : replacements) {
        int s = get<0>(repl);
        int e = get<1>(repl);
        const string& repl_str = get<2>(repl);

        if (s < static_cast<int>(pos)) return "";

        result += str.substr(pos, s - pos);
        result += repl_str;
        pos = e + 1;
    }

    result += str.substr(pos);

    return result;
}

// Process RACE extraction and generation here.
void MatchForStmtCallback::run(const MatchFinder::MatchResult &Result) {
    const auto &Nodes = Result.Nodes;
    auto &Diag = Result.Context->getDiagnostics();

    const auto *for_stmt = Nodes.getNodeAs<clang::ForStmt>("for_stmt");
    if (for_stmt) {
        if (avnPragmaHandler->ifBelongsToPragma(for_stmt->getBeginLoc())) {
            //! Clang scan the AST in a layer-by-layer way, so the nested for are not scanned consequently.
            //! The first level loops will be scanned first, and the nested loops will be scanned after all the first level loops are scanned.
            // Check if the for statement is already processed.
            for (const auto &loc_pair : processed_for_loc) {
                if (for_stmt->getBeginLoc() >= loc_pair.first && for_stmt->getEndLoc() <= loc_pair.second) {
                    return;
                }
            }
            Diag.Report(for_stmt->getBeginLoc(), diag_warn_potential_for);

            this->for_stmt = std::make_shared<ir::ForStmt>(for_stmt);
            auto avn_block = make_shared<ir::AVNBlock>(this->for_stmt);
            extract::extract(avn_block, this->avn_cnt++);
            
            processed_outputs.push_back(avn_block->to_string());
            processed_for_loc.push_back({for_stmt->getBeginLoc(), for_stmt->getEndLoc()});
            SourceLocation matchedVarLoc = avnPragmaHandler->getMatchedVarLoc(for_stmt->getBeginLoc());
            string concat_str;
            for (const auto &var_decl : avn_block->getAVNVarDecl()) {
                concat_str += var_decl + "\n";
            }
            var_decls[matchedVarLoc] += concat_str;
        }
    }
}

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

inline Location parseSourceLoc(const SourceLocation &loc, const SourceManager &SM) {
    auto locInfo = SM.getDecomposedLoc(loc);
    auto lineAndCol = SM.getPresumedLoc(loc);
    return {lineAndCol.getLine(), lineAndCol.getColumn()};
}

void MatchForStmtCallback::onEndOfTranslationUnit() {
    auto &SM = CI.getSourceManager();
    // string replaceRegions(const string& str,
    //     const vector<pair<Location, Location>>& regions,
    //     const vector<string>& replace_strs) {

    vector<pair<Location, Location>> regions;
    vector<string> replace_strs;
    // Collect var declarations' locations and strings.
    for (auto iter : var_decls) {
        const auto &loc = parseSourceLoc(iter.first, SM);
        Location start = {loc.line, 1};
        Location end = {loc.line, INT32_MAX >> 2};
        const auto &decl_str = iter.second;
        regions.push_back({start, end});
        replace_strs.push_back(decl_str);
    }
    // Collect for loop locations and strings.
    for (size_t i = 0; i < processed_for_loc.size(); ++i) {
        const auto &loc_pair = processed_for_loc[i];
        auto loc1 = parseSourceLoc(loc_pair.first, SM);
        auto loc2 = parseSourceLoc(loc_pair.second, SM);
        loc1.col = 1;
        regions.push_back({loc1, loc2});
        replace_strs.push_back(processed_outputs[i]);
    }
    // print regions and strs
    // for (size_t i = 0; i < regions.size(); ++i) {
    //     const auto &region = regions[i];
    //     const auto &replace_str = replace_strs[i];
    //     outs() << "Region: (" << region.first.line << ", " << region.first.col << ") - ("
    //            << region.second.line << ", " << region.second.col << ")\n";
    //     outs() << "Replace string: " << replace_str << "\n";
    // }
    // Read the file content.
    const auto &fileID = SM.getMainFileID();
    const auto &file = SM.getFileEntryForID(fileID);
    if (!file) {
        return;
    }
    const auto &file_path = file->tryGetRealPathName().str();
    const auto &file_buf = SM.getBufferData(fileID);
    const auto &file_str = file_buf.str();
    // Replace the regions.
    const auto &new_file_str = replaceRegions(file_str, regions, replace_strs);
    // Output the new file content.
    const auto &new_file_path 
        = file_path.substr(0, file_path.find_last_of("/")) +
          "/race_" + file_path.substr(file_path.find_last_of("/") + 1);
    std::ofstream new_file(new_file_path);
    new_file << new_file_str;
    new_file.close();
    outs() << "New file is saved to " << new_file_path << "\n";
}

std::unique_ptr<ASTConsumer>
Scanner::CreateASTConsumer(CompilerInstance &CI,
                           StringRef InFile) {
    // Create custom diagnostic message
    assert(CI.hasASTContext() && "No ASTContext??");
    auto &Diag = CI.getASTContext().getDiagnostics();
    auto DiagWarnMain = Diag.getCustomDiagID(
        DiagnosticsEngine::Warning,
        "This for statement is in race pragma: ");

    ForStmtFinder = std::make_unique<MatchFinder>();

    MatchCB = std::make_unique<MatchForStmtCallback>(DiagWarnMain, avnPragmaHandler, CI);
    ForStmtFinder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                       forStmt().bind("for_stmt")),
                              MatchCB.get());

    return std::move(ForStmtFinder->newASTConsumer());
}
