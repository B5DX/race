#include "llvm/Config/llvm-config.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"

#include "Frontend/Scanner.h"
#include "common.h"

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory MyToolCategory("RACE options");

static cl::list<std::string> IncludePath(
    "I", cl::desc("Add directory to include search path"),
    cl::value_desc("directory"), cl::Prefix, cl::ZeroOrMore, cl::cat(MyToolCategory));

static cl::opt<bool> noCommute("no-commute",
    llvm::cl::desc("Disable expression reassociation and operand reordering."),
    llvm::cl::cat(MyToolCategory), llvm::cl::init(false));
static cl::opt<bool> noContract("no-contraction", llvm::cl::desc("Keep all auxiliary array dimensions (uses more storage).")
    , llvm::cl::cat(MyToolCategory), llvm::cl::init(false));
static cl::opt<bool> TSR("tsr", llvm::cl::desc("Inline temporary scalar expressions, preserving observable assignments."),
    llvm::cl::cat(MyToolCategory), llvm::cl::init(false));
static cl::opt<bool> COMPIN("cmi", llvm::cl::desc("Allow contraction of the innermost auxiliary array dimension."),
    llvm::cl::cat(MyToolCategory), llvm::cl::init(false));
static cl::opt<int> MAXLF("mlf", llvm::cl::desc("Limit leaf array references per auxiliary array (at least 2)."),
    llvm::cl::cat(MyToolCategory), llvm::cl::init(INT32_MAX));

static constexpr const char HelpExamples[] = "\nExamples:\n  race source.cpp -- -std=gnu++17 -Iinclude\n  race -p build source.cpp\nOutput: race_<source filename>, beside each input. Compiler errors remain visible.\n";
static cl::extrahelp CommonHelp(HelpExamples);

static cl::extrahelp MoreHelp("\nThis tool uses RACE algorithm to automatically extract loop-carried redundant expressions.\n");

Config global_config;

int main(int argc, const char **argv) {
    cl::SetVersionPrinter([](raw_ostream &out) { out << "RACE (LLVM " LLVM_VERSION_STRING ")\n"; });

    // CommonOptionsParser registers -p while parsing, and its built-in help
    // exits immediately. Register with no inputs before printing customized help.
    for (int i = 1; i < argc && std::string(argv[i]) != "--"; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "-help" ||
            arg == "--help-list" || arg == "--help-hidden" ||
            arg == "--help-list-hidden") {
            const char *help_args[] = {argv[0]};
            int help_argc = 1;
            auto parser = CommonOptionsParser::create(
                help_argc, help_args, MyToolCategory, cl::ZeroOrMore);
            if (!parser) {
                llvm::errs() << parser.takeError();
                return 1;
            }
            auto *build = cl::getRegisteredOptions().lookup("p");
            build->setDescription("Directory containing compile_commands.json");
            build->setValueStr("directory");
            cl::PrintHelpMessage(arg.find("hidden") != std::string::npos,
                                arg.find("list") == std::string::npos);
            llvm::outs() << HelpExamples;
            return 0;
        }
    }

    // Standalone files use an explicit fixed compilation database. Avoid the
    // noisy failed compilation-database search unless the user requests -p.
    vector<const char *> args(argv, argv + argc);
    bool compiler_args = false, build_path = false;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--") { compiler_args = true; break; }
        if (arg == "-p" || arg.rfind("-p=", 0) == 0 ||
            (arg.rfind("-p", 0) == 0 && arg.size() > 2)) build_path = true;
    }
    if (!compiler_args && !build_path) args.push_back("--");
    int tool_argc = args.size();
    auto ExpectedParser = CommonOptionsParser::create(tool_argc, args.data(), MyToolCategory);
    if (!ExpectedParser) {
        // Fail gracefully for unsupported options.
        llvm::errs() << ExpectedParser.takeError();
        return 1;
    }
    CommonOptionsParser &OptionsParser = ExpectedParser.get();
    if (MAXLF < 2) {
        llvm::errs() << "race: --mlf must be at least 2\n";
        return 1;
    }

    global_config.commutable = !noCommute;
    global_config.contraction = !noContract;
    global_config.scalar_replacement = TSR;
    global_config.vectorization = !COMPIN;
    global_config.max_leaves = MAXLF;

    int result = 0;
    for (const auto &source : OptionsParser.getSourcePathList()) {
        ClangTool Tool(OptionsParser.getCompilations(),
                       std::vector<std::string>{source});
        Tool.appendArgumentsAdjuster(clang::tooling::getClangSyntaxOnlyAdjuster());
#ifdef RACE_RESOURCE_DIR
        if (llvm::sys::fs::exists(RACE_RESOURCE_DIR)) {
            Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(
                "-resource-dir=" RACE_RESOURCE_DIR, ArgumentInsertPosition::BEGIN));
        }
#endif
        CommandLineArguments include_args;
        for (const auto &path : IncludePath) include_args.push_back("-I" + path);
        Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(include_args, ArgumentInsertPosition::BEGIN));
        Tool.setPrintErrorMessage(false);
        const int status = Tool.run(newFrontendActionFactory<Scanner>().get());
        if (status != 0) result = status;
    }
    return result;
}
