#include "clang/Basic/Diagnostic.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/CommandLine.h"
// #include "llvm/Support/Path.h"
// #include "llvm/Support/TargetSelect.h"

#include "Frontend/PragmaHandler.h"
#include "Frontend/Scanner.h"

#include "IR/IR.h"

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::opt<std::string> IncludePath(
    "I", cl::desc("Add directory to include search path"),
    cl::value_desc("directory"), cl::cat(MyToolCategory));

static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

int main(int argc, const char **argv) {
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

    auto ExpectedParser = CommonOptionsParser::create(argc, argv, MyToolCategory);
    if (!ExpectedParser) {
        // Fail gracefully for unsupported options.
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    ClangTool Tool(OptionsParser.getCompilations(),
                   OptionsParser.getSourcePathList());
    Tool.appendArgumentsAdjuster(clang::tooling::getClangSyntaxOnlyAdjuster());
    // Add clang include path.
    // Or you can add path "/usr/lib/gcc/x86_64-linux-gnu/9/include" to fix error on "can't find stddef.h".
    Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-I/home/yuanliang/.local/lib/clang/19/include", ArgumentInsertPosition::BEGIN));

    if (!IncludePath.empty()) {
        // TODO: process ':' separated paths
        llvm::outs() << "Adding include path: " << IncludePath << "\n";
        Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(("-I"+IncludePath).c_str(), ArgumentInsertPosition::BEGIN));
    }

    llvm::outs() << "Running tool.\n";
    return Tool.run(newFrontendActionFactory<Scanner>().get());
}
