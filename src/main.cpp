#include "clang/Basic/Diagnostic.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/CommandLine.h"
// #include "llvm/Support/Path.h"
// #include "llvm/Support/TargetSelect.h"

#include "Frontend/PragmaHandler.h"
#include "Frontend/Scanner.h"
#include "common.h"

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("RACE options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::opt<std::string> IncludePath(
    "I", cl::desc("Add directory to include search path"),
    cl::value_desc("directory"), cl::cat(MyToolCategory));

static cl::opt<bool> noCommute("no-commute", 
    llvm::cl::desc("Disable optimizations that assume commutativity of operations."), 
    llvm::cl::cat(MyToolCategory), llvm::cl::init(false));
static cl::opt<bool> noContract("no-contraction", llvm::cl::desc("Prevent auxiliary array contraction.")
    , llvm::cl::cat(MyToolCategory), llvm::cl::init(false));
static cl::opt<bool> ESR("esr", llvm::cl::desc("Conduct ESR Algorithm instead of RACE."), 
    llvm::cl::cat(MyToolCategory), llvm::cl::init(false));
static cl::opt<bool> TSR("tsr", llvm::cl::desc("Replace temporal scalars."), 
    llvm::cl::cat(MyToolCategory), llvm::cl::init(false));
static cl::opt<bool> COMPIN("cmi", llvm::cl::desc("Compress innermost dimension if possible."), 
    llvm::cl::cat(MyToolCategory), llvm::cl::init(false));
static cl::opt<int> MAXLF("mlf", llvm::cl::desc("Set max leaf array references number in each auxiliary array."),
    llvm::cl::cat(MyToolCategory), llvm::cl::init(INT32_MAX));
    
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nThis tool uses RACE algorithm to automatically extract loop-carried redundant expressions.\n");

// Define the global_config variable, which is declared as extern in common.h
Config global_config;

int main(int argc, const char **argv) {
    // llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

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

    if (noCommute) {
        global_config.commutable = false;
    }
    if (noContract) {
        global_config.contraction = false;
    }
    if (ESR) {
        global_config.ESR = true;
        global_config.contraction = true; // ESR requires contraction
        global_config.vectorization = false; // ESR compress the innermost loop
        global_config.commutable = true; // ESR requires commutative law
    }
    if (TSR) {
        global_config.scalar_replacement = true;
    }
    if (COMPIN) {
        global_config.vectorization = false;
    }
    if (MAXLF < INT32_MAX) {
        global_config.max_leaves = MAXLF;
    }

    llvm::outs() << "Running tool.\n";
    return Tool.run(newFrontendActionFactory<Scanner>().get());
}
