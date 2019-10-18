#ifdef _MSC_VER
#pragma warning(disable:4091)
#pragma warning(disable:4100)
#pragma warning(disable:4127)
#pragma warning(disable:4141)
#pragma warning(disable:4146)
#pragma warning(disable:4180)
#pragma warning(disable:4204)
#pragma warning(disable:4244)
#pragma warning(disable:4245)
#pragma warning(disable:4258)
#pragma warning(disable:4267)
#pragma warning(disable:4291)
#pragma warning(disable:4310)
#pragma warning(disable:4319)
#pragma warning(disable:4324)
#pragma warning(disable:4345)
#pragma warning(disable:4351)
#pragma warning(disable:4355)
#pragma warning(disable:4456)
#pragma warning(disable:4457)
#pragma warning(disable:4458)
#pragma warning(disable:4459)
#pragma warning(disable:4503)
#pragma warning(disable:4505)
#pragma warning(disable:4510)
#pragma warning(disable:4512)
#pragma warning(disable:4577)
#pragma warning(disable:4592)
#pragma warning(disable:4610)
#pragma warning(disable:4624)
#pragma warning(disable:4702)
#pragma warning(disable:4706)
#pragma warning(disable:4709)
#pragma warning(disable:4722)
#pragma warning(disable:4800)
#pragma warning(disable:4701)
#pragma warning(disable:4703)
#pragma warning(disable:4389)
#pragma warning(disable:4611)
#pragma warning(disable:4805)
#endif

#include <sstream>

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/FileSystemOptions.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/MemoryBufferCache.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Sema/Sema.h>

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/InitializePasses.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Linker/Linker.h>

#include "jit.h"

static void InitializeLLVM()
{
    static bool LLVMinit = false;
    if (LLVMinit) {
        return;
    }
    // For All Targets
    // llvm::InitializeAllTargets();
    // llvm::InitializeAllTargetMCs();
    // llvm::InitializeAllAsmPrinters();
    // llvm::InitializeAllAsmParsers();

    // For Native Only
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // For Indivisual Targets
    // #define LLVM_TARGET(TargetName) LLVMInitialize##TargetName##Target();
    // LLVM_TARGET(AArch64)
    // LLVM_TARGET(ARM)
    // LLVM_TARGET(PowerPC)
    // LLVM_TARGET(Sparc)
    // LLVM_TARGET(WebAssembly)
    // LLVM_TARGET(X86)
    // #undef LLVM_TARGET
    // #define LLVM_TARGET(TargetName) LLVMInitialize##TargetName##TargetMC();
    // LLVM_TARGET(AArch64)
    // LLVM_TARGET(ARM)
    // LLVM_TARGET(PowerPC)
    // LLVM_TARGET(Sparc)
    // LLVM_TARGET(WebAssembly)
    // LLVM_TARGET(X86)
    // #undef LLVM_TARGET
    // #define LLVM_TARGET(TargetName) LLVMInitialize##TargetName##AsmPrinter();
    // LLVM_TARGET(AArch64)
    // LLVM_TARGET(ARM)
    // LLVM_TARGET(PowerPC)
    // LLVM_TARGET(Sparc)
    // LLVM_TARGET(WebAssembly)
    // LLVM_TARGET(X86)
    // #undef LLVM_TARGET
    // #define LLVM_TARGET(TargetName) LLVMInitialize##TargetName##AsmParser();
    // LLVM_TARGET(AArch64)
    // LLVM_TARGET(ARM)
    // LLVM_TARGET(PowerPC)
    // LLVM_TARGET(Sparc)
    // LLVM_TARGET(WebAssembly)
    // LLVM_TARGET(X86)
    // #undef LLVM_TARGET

    auto& Registry = *llvm::PassRegistry::getPassRegistry();
    llvm::initializeCore(Registry);
    llvm::initializeScalarOpts(Registry);
    llvm::initializeVectorization(Registry);
    llvm::initializeIPO(Registry);
    llvm::initializeAnalysis(Registry);
    llvm::initializeTransformUtils(Registry);
    llvm::initializeInstCombine(Registry);
    llvm::initializeInstrumentation(Registry);
    llvm::initializeTarget(Registry);

    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
    LLVMinit = true;
}

class DiagnosticConsumerHandlerCall : public clang::DiagnosticConsumer
{
public:
    DiagnosticConsumerHandlerCall(error_handler_t handler) :
        handler_(handler),
        aborted_(false),
        fatals_(0),
        errors_(0),
        warns_(0)
    {
    }

    virtual void HandleDiagnostic(clang::DiagnosticsEngine::Level Level, const clang::Diagnostic &Info)
    {
        if (aborted_) return;

        auto& sm = Info.getSourceManager();
        auto& sl = Info.getLocation();
        auto loc = sm.getPresumedLoc(sl);
        auto filename = loc.getFilename();
        unsigned int line = loc.getLine();
        auto column = loc.getColumn();
        clang::SmallString<256> OutStr;
        Info.FormatDiagnostic(OutStr);
        switch (Level) {
        case clang::DiagnosticsEngine::Level::Fatal:
            ++fatals_;
            break;
        case clang::DiagnosticsEngine::Level::Error:
            ++errors_;
            break;
        case clang::DiagnosticsEngine::Level::Warning:
            ++warns_;
        default:
            ;
        }
        if (handler_) {
            if (!handler_(Level, filename, line, column, OutStr.c_str())) {
                aborted_ = true;
            }
        }
    }

    bool isAborted()
    {
        return aborted_;
    }

    int getTotalErrorCount()
    {
        return fatals_ + errors_ + warns_;
    }

    int getFatalCount()
    {
        return errors_;
    }

    int getErrorCount()
    {
        return errors_;
    }

    int getWarnCount()
    {
        return warns_;
    }

private:
    error_handler_t handler_;
    bool aborted_;
    int  fatals_;
    int  errors_;
    int  warns_;
};

struct ClangJitContext {
    ClangJitContext() : llvm(0), engine(0), options()
    {
        options[ClangJitOption_OptimizeLevel] = 2;
        options[ClangJitOption_ErrorLimit] = 100;
        options[ClangJitOption_DegugMode] = false;
        triple = llvm::sys::getDefaultTargetTriple();
    }

    ~ClangJitContext()
    {
        delete engine;
        module.reset();
        delete llvm;
    }

    llvm::LLVMContext *llvm;
    llvm::ExecutionEngine* engine;
    std::string triple;
    int options[ClangJitOption_MaxOptionCount];
    std::unique_ptr<llvm::Module> module;
};

extern "C" {

void *clang_JitAllocContext()
{
    InitializeLLVM();
    ClangJitContext *jitContext = new ClangJitContext();
    return jitContext;
}

void clang_JitFreeContext(void *ctx)
{
    if (!ctx) return;
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    delete jitContext;
}

const char *clang_JitLoadSharedFile(const char *name)
{
    InitializeLLVM();
    std::string errMsg;
    if (llvm::sys::DynamicLibrary::LoadLibraryPermanently(name, &errMsg)) {
        static char errorMessage[256] = {0};
        snprintf(errorMessage, 255, "%s", errMsg.c_str());
        return errorMessage;
    }
    return nullptr;
}

void clang_JitAddSymbol(const char *name, void *value)
{
    InitializeLLVM();
    llvm::sys::DynamicLibrary::AddSymbol(name, value);
}

void* clang_JitSearchSymbol(const char *name)
{
    InitializeLLVM();
    return llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(name);
}

void clang_JitIrDump(void *ctx, const char *filename)
{
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext || !jitContext->module) return;
    if (!filename) {
        jitContext->module->print(llvm::errs(), nullptr);
    } else {
        std::error_code ec;
        llvm::raw_fd_ostream os(filename, ec, llvm::sys::fs::OpenFlags::OF_None);
        jitContext->module->print(os, nullptr);
        os.close();
    }
}

int clang_JitIrSave(void *ctx, const char *filename)
{
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext || !jitContext->module) return 0;

    std::error_code ec;
    llvm::raw_fd_ostream os(filename, ec, llvm::sys::fs::OpenFlags::OF_None);
    llvm::WriteBitcodeToFile(*(jitContext->module.get()), os);
    os.close();
    return 1;
}

const char *clang_JitIrLoad(void *ctx, const char *filename)
{
    static char errorMessage[256] = {0};
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext || jitContext->llvm || jitContext->module) {
        snprintf(errorMessage, 255, "Invalid context.");
        return errorMessage;
    }

    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(filename);
    if (std::error_code ec = fileOrErr.getError()) {
        snprintf(errorMessage, 255, "%s", ec.message().c_str());
        return errorMessage;
    }
    jitContext->llvm = new llvm::LLVMContext();
    llvm::Expected<std::unique_ptr<llvm::Module>> moduleExpected = llvm::parseBitcodeFile(fileOrErr.get()->getMemBufferRef(), *(jitContext->llvm));
    if (!moduleExpected) {
        snprintf(errorMessage, 255, "Parsing bitcode failed.");
        return errorMessage;
    }
    jitContext->module = std::move(moduleExpected.get());
    return nullptr;
}

const char *clang_JitIrMergeFile(void *ctx, const char *filename)
{
    static char errorMessage[256] = {0};
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext) {
        snprintf(errorMessage, 255, "Invalid context.");
        return errorMessage;
    }

    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(filename);
    if (std::error_code ec = fileOrErr.getError()) {
        snprintf(errorMessage, 255, "%s", ec.message().c_str());
        return errorMessage;
    }
    if (!jitContext->module) {
        if (!jitContext->llvm) {
            jitContext->llvm = new llvm::LLVMContext();
        }
        llvm::Expected<std::unique_ptr<llvm::Module>> moduleExpected = llvm::parseBitcodeFile(fileOrErr.get()->getMemBufferRef(), *(jitContext->llvm));
        jitContext->module = std::move(moduleExpected.get());
        return nullptr;
    }
    llvm::Expected<std::unique_ptr<llvm::Module>> moduleExpected = llvm::parseBitcodeFile(fileOrErr.get()->getMemBufferRef(), jitContext->module->getContext());
    if (!llvm::Linker::linkModules(*jitContext->module.get(), std::move(moduleExpected.get()))) {
        // successful to copy.
        return nullptr;
    }
    snprintf(errorMessage, 255, "Failed to merge modules.");
    return errorMessage;
}

const char *clang_JitIrMerge(void *ctx, void *another)
{
    static char errorMessage[256] = {0};
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    ClangJitContext *anotherContext = (ClangJitContext*)another;
    if (!jitContext || !anotherContext || !anotherContext->llvm || !anotherContext->module) {
        snprintf(errorMessage, 255, "Invalid context of source.");
        return errorMessage;
    }
    if (jitContext->llvm && !jitContext->module) {
        snprintf(errorMessage, 255, "Invalid context of destination.");
        return errorMessage;
    }
    if (!jitContext->module) {
        assert(!jitContext->llvm);
        jitContext->module = std::move(anotherContext->module);
        jitContext->llvm = anotherContext->llvm;    // move.
        anotherContext->llvm = nullptr;             // reset because it was moved.
        // successful.
        return nullptr;
    }

    std::string source;
    llvm::raw_string_ostream os(source);
    llvm::WriteBitcodeToFile(*anotherContext->module.get(), os);
    std::unique_ptr<llvm::MemoryBuffer> buffer = llvm::MemoryBuffer::getMemBufferCopy(os.str(), "SourceBitcodeStringBuffer");
    llvm::Expected<std::unique_ptr<llvm::Module>> moduleExpected = llvm::parseBitcodeFile(buffer->getMemBufferRef(), jitContext->module->getContext());
    if (!moduleExpected) {
        snprintf(errorMessage, 255, "Parsing source bitcode failed.");
        return errorMessage;
    }

    if (!llvm::Linker::linkModules(*jitContext->module.get(), std::move(moduleExpected.get()))) {
        // successful to copy.
        return nullptr;
    }
    snprintf(errorMessage, 255, "Failed to merge modules.");
    return errorMessage;
}

void clang_JitSetOptionInt(void *ctx, int key, int value)
{
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext) return;
    jitContext->options[key] = value;
}

void clang_JitSetTriple(void *ctx, const char *triple)
{
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext) return;
    jitContext->triple = triple;
}

void *clang_JitGetFunctionAddress(void *ctx, const char *name)
{
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext) return nullptr;
    if (!jitContext->engine) return nullptr;
    return (void *)(jitContext->engine->getFunctionAddress(name));
}

const char *clang_JitLoadObjectFile(void *ctx, const char *name)
{
    static char errorMessage[256] = {0};
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext) {
        snprintf(errorMessage, 255, "Invalid context of source.");
        return errorMessage;
    }
    if (!jitContext->engine) {
        jitContext->llvm = new llvm::LLVMContext();
        llvm::EngineBuilder builder(std::make_unique<llvm::Module>("objectLoader", *(jitContext->llvm)));
        builder.setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>());
        builder.setOptLevel(llvm::CodeGenOpt::Level::Aggressive);
        auto executionEngine = builder.create();
        if (!executionEngine) {
            snprintf(errorMessage, 255, "Cannot create execution engine.");
            return errorMessage;
        }
        jitContext->engine = executionEngine;
    }

    llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer = llvm::MemoryBuffer::getFile(name);
    if (!buffer) {
        snprintf(errorMessage, 255, "Cannot allocate the buffer.");
        return errorMessage;
    }

    llvm::Expected<std::unique_ptr<llvm::object::ObjectFile>> objectOrError =
        llvm::object::ObjectFile::createObjectFile(buffer.get()->getMemBufferRef());

    if (!objectOrError) {
        snprintf(errorMessage, 255, "Cannot load object file: %s", name);
        return errorMessage;
    }

    std::unique_ptr<llvm::object::ObjectFile> objectFile(std::move(objectOrError.get()));
    auto owningObject = llvm::object::OwningBinary<llvm::object::ObjectFile>(std::move(objectFile), std::move(buffer.get()));
    jitContext->engine->addObjectFile(std::move(owningObject));

    return nullptr;
}

const char *clang_JitOutputObjectFile(void *ctx, const char *name)
{
    static char errorMessage[256] = {0};
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext || !jitContext->module) {
        snprintf(errorMessage, 255, "Invalid context of source.");
        return errorMessage;
    }

    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    jitContext->module->setTargetTriple(targetTriple);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        snprintf(errorMessage, 255, "%s", error.c_str());
        return errorMessage;
    }

    auto cpu = "generic";
    auto Features = "";
    llvm::TargetOptions opt;
    auto rm = llvm::Optional<llvm::Reloc::Model>();
    auto theTargetMachine = target->createTargetMachine(targetTriple, cpu, Features, opt, rm);
    jitContext->module->setDataLayout(theTargetMachine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(name, ec, llvm::sys::fs::F_None);
    if (ec) {
        snprintf(errorMessage, 255, "Cannot open file: %s", name);
        return errorMessage;
    }

    llvm::legacy::PassManager pass;
    auto fileType = llvm::TargetMachine::CGFT_ObjectFile;
    if (theTargetMachine->addPassesToEmitFile(pass, dest, nullptr, fileType)) {
        snprintf(errorMessage, 255, "Cannot emit a file of this type.");
        return errorMessage;
    }

    pass.run(*(jitContext->module));
    dest.flush();

    return nullptr;
}

void *clang_JitIrCompile(void *ctx, const char *source, int type, error_handler_t handler)
{
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext || jitContext->llvm) return nullptr;

    // setting up the compiler diagnostics.
    llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs(new clang::DiagnosticIDs());
    llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagnosticOptions(new clang::DiagnosticOptions());
    diagnosticOptions->ErrorLimit = jitContext->options[ClangJitOption_ErrorLimit];
    std::unique_ptr<DiagnosticConsumerHandlerCall> diagHandler = std::make_unique<DiagnosticConsumerHandlerCall>(handler);
    std::unique_ptr<clang::DiagnosticsEngine> diagnosticsEngine =
        std::make_unique<clang::DiagnosticsEngine>(diagIDs, diagnosticOptions, diagHandler.get(), false);

    clang::CompilerInstance compilerInstance;
    auto& compilerInvocation = compilerInstance.getInvocation();

    // compiler invocation
    std::stringstream ss;
    ss << "-triple=" << jitContext->triple;
    if (jitContext->options[ClangJitOption_OptimizeLevel] > 0) {
        ss << " -O" << jitContext->options[ClangJitOption_OptimizeLevel];
    }
    if (type == ClangJitSourceType_CXX_String || type == ClangJitSourceType_CXX_File) {
        ss << " -fcxx-exceptions";
    }
    ss << " -fms-extensions";
    std::istream_iterator<std::string> begin(ss);
    std::istream_iterator<std::string> end;
    std::istream_iterator<std::string> i = begin;
    std::vector<const char*> itemcstrs;
    std::vector<std::string> itemstrs;
    for ( ; i != end; ++i) {
        itemstrs.push_back(*i);
    }
    for (unsigned idx = 0; idx < itemstrs.size(); idx++) {
        itemcstrs.push_back(itemstrs[idx].c_str());
    }

    // compiler instance.
    clang::CompilerInvocation::CreateFromArgs(compilerInvocation,
        itemcstrs.data(), itemcstrs.data() + itemcstrs.size(), *diagnosticsEngine.get());

    // Options.
    // auto& preprocessorOptions = compilerInvocation.getPreprocessorOpts();
    // auto& codeGenOptions = compilerInvocation.getCodeGenOpts();
    auto* languageOptions = compilerInvocation.getLangOpts();
    auto& targetOptions = compilerInvocation.getTargetOpts();
    targetOptions.Triple = jitContext->triple;
    auto& headerSearchOptions = compilerInvocation.getHeaderSearchOpts();
    auto& frontEndOptions = compilerInvocation.getFrontendOpts();
    frontEndOptions.Inputs.clear();
    if (jitContext->options[ClangJitOption_DegugMode]) {
        frontEndOptions.ShowStats = true;
        headerSearchOptions.Verbose = true;
    }
    #ifdef _MSC_VER
    languageOptions->MSVCCompat = 1;
    languageOptions->MicrosoftExt = 1;
    #endif

    // Setting up to compile a file.
    std::unique_ptr<llvm::MemoryBuffer> buffer;
    switch (type) {
    case ClangJitSourceType_C_String:
        buffer = llvm::MemoryBuffer::getMemBufferCopy(source, "SourceTextStringBuffer");
        frontEndOptions.Inputs.push_back(clang::FrontendInputFile(buffer.get(), clang::InputKind::C));
        break;
    case ClangJitSourceType_CXX_String:
        // codeGenOptions.UnwindTables = 1;
        // codeGenOptions.Addrsig = 1;
        // languageOptions->Exceptions = 1;
        // languageOptions->CXXExceptions = 1;
        // languageOptions->RTTI = 1;
        languageOptions->Bool = 1;
        languageOptions->CPlusPlus = 1;
        buffer = llvm::MemoryBuffer::getMemBufferCopy(source, "SourceTextStringBuffer");
        frontEndOptions.Inputs.push_back(clang::FrontendInputFile(buffer.get(), clang::InputKind::CXX));
        break;
    case ClangJitSourceType_C_File:
        frontEndOptions.Inputs.push_back(clang::FrontendInputFile(source, clang::InputKind::C));
        break;
    case ClangJitSourceType_CXX_File:
        // codeGenOptions.UnwindTables = 1;
        // codeGenOptions.Addrsig = 1;
        // languageOptions->Exceptions = 1;
        // languageOptions->CXXExceptions = 1;
        // languageOptions->RTTI = 1;
        languageOptions->Bool = 1;
        languageOptions->CPlusPlus = 1;
        frontEndOptions.Inputs.push_back(clang::FrontendInputFile(source, clang::InputKind::CXX));
        break;
    default:
        return nullptr;
    }

    compilerInstance.createDiagnostics(diagHandler.get(), false);
    jitContext->llvm = new llvm::LLVMContext();
    std::unique_ptr<clang::CodeGenAction> action = std::make_unique<clang::EmitLLVMOnlyAction>(jitContext->llvm);
    if (!compilerInstance.ExecuteAction(*action)) {
        return nullptr;
    }
    if (diagHandler->getTotalErrorCount() > 0) {
        if ((diagHandler->getErrorCount() > 0) || (diagHandler->getFatalCount() > 0) ||
                (diagHandler->getWarnCount() > jitContext->options[ClangJitOption_WarningLimit])) {
            return nullptr;
        }
    }

    // get IR module.
    jitContext->module = action->takeModule();
    if (!jitContext->module) {
        return nullptr;
    }

    return jitContext;
}

void *clang_JitIrOptimize(void *ctx)
{
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext || !jitContext->module) return nullptr;

    clang::CompilerInstance compilerInstance;
    auto& compilerInvocation = compilerInstance.getInvocation();
    auto& codeGenOptions = compilerInvocation.getCodeGenOpts();

    // optimizations.
    llvm::PassBuilder passBuilder;
    llvm::LoopAnalysisManager loopAnalysisManager(codeGenOptions.DebugPassManager);
    llvm::FunctionAnalysisManager functionAnalysisManager(codeGenOptions.DebugPassManager);
    llvm::CGSCCAnalysisManager cGSCCAnalysisManager(codeGenOptions.DebugPassManager);
    llvm::ModuleAnalysisManager moduleAnalysisManager(codeGenOptions.DebugPassManager);
    passBuilder.registerModuleAnalyses(moduleAnalysisManager);
    passBuilder.registerCGSCCAnalyses(cGSCCAnalysisManager);
    passBuilder.registerFunctionAnalyses(functionAnalysisManager);
    passBuilder.registerLoopAnalyses(loopAnalysisManager);
    passBuilder.crossRegisterProxies(loopAnalysisManager, functionAnalysisManager, cGSCCAnalysisManager, moduleAnalysisManager);
    llvm::ModulePassManager modulePassManager = passBuilder.buildPerModuleDefaultPipeline(llvm::PassBuilder::OptimizationLevel::O3);
    modulePassManager.run(*jitContext->module, moduleAnalysisManager);

    return jitContext;
}

void *clang_JitGenerateTargetCode(void *ctx)
{
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext || !jitContext->module) return nullptr;

    llvm::EngineBuilder builder(std::move(jitContext->module));
    builder.setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>());
    builder.setOptLevel(llvm::CodeGenOpt::Level::Aggressive);
    auto executionEngine = builder.create();
    if (!executionEngine) {
        return nullptr;
    }

    jitContext->engine = executionEngine;
    return jitContext;
}

void clang_JitFinalizeCode(void *ctx)
{
    ClangJitContext *jitContext = (ClangJitContext*)ctx;
    if (!jitContext || !jitContext->engine) return;
    jitContext->engine->finalizeObject();
}

} // extern "C"

////////////////////////////////////////////////////////////////////////////
// Clang Driver needs version.dll for MSVC
////////////////////////////////////////////////////////////////////////////
#if _WIN32
#pragma comment(lib, "version.lib")

////////////////////////////////////////////////////////////////////////////
// Clang library
////////////////////////////////////////////////////////////////////////////
#pragma comment(lib, "clang/lib/clangAnalysis.lib")
#pragma comment(lib, "clang/lib/clangAST.lib")
#pragma comment(lib, "clang/lib/clangBasic.lib")
#pragma comment(lib, "clang/lib/clangCodeGen.lib")
#pragma comment(lib, "clang/lib/clangDriver.lib")
#pragma comment(lib, "clang/lib/clangEdit.lib")
#pragma comment(lib, "clang/lib/clangFrontend.lib")
#pragma comment(lib, "clang/lib/clangLex.lib")
#pragma comment(lib, "clang/lib/clangParse.lib")
#pragma comment(lib, "clang/lib/clangSema.lib")
#pragma comment(lib, "clang/lib/clangSerialization.lib")

////////////////////////////////////////////////////////////////////////////
// LLVM library
////////////////////////////////////////////////////////////////////////////
#pragma comment(lib, "clang/lib/LLVMAggressiveInstCombine.lib")
#pragma comment(lib, "clang/lib/LLVMAnalysis.lib")
#pragma comment(lib, "clang/lib/LLVMBinaryFormat.lib")
#pragma comment(lib, "clang/lib/LLVMBitReader.lib")
#pragma comment(lib, "clang/lib/LLVMBitWriter.lib")
#pragma comment(lib, "clang/lib/LLVMCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMCore.lib")
#pragma comment(lib, "clang/lib/LLVMCoroutines.lib")
#pragma comment(lib, "clang/lib/LLVMCoverage.lib")
#pragma comment(lib, "clang/lib/LLVMDebugInfoCodeView.lib")
#pragma comment(lib, "clang/lib/LLVMDemangle.lib")
#pragma comment(lib, "clang/lib/LLVMExecutionEngine.lib")
#pragma comment(lib, "clang/lib/LLVMGlobalISel.lib")
#pragma comment(lib, "clang/lib/LLVMInstCombine.lib")
#pragma comment(lib, "clang/lib/LLVMInstrumentation.lib")
#pragma comment(lib, "clang/lib/LLVMipo.lib")
#pragma comment(lib, "clang/lib/LLVMIRReader.lib")
#pragma comment(lib, "clang/lib/LLVMLinker.lib")
#pragma comment(lib, "clang/lib/LLVMLTO.lib")
#pragma comment(lib, "clang/lib/LLVMMC.lib")
#pragma comment(lib, "clang/lib/LLVMMCDisassembler.lib")
#pragma comment(lib, "clang/lib/LLVMMCJIT.lib")
#pragma comment(lib, "clang/lib/LLVMMCParser.lib")
#pragma comment(lib, "clang/lib/LLVMObjCARCOpts.lib")
#pragma comment(lib, "clang/lib/LLVMObject.lib")
#pragma comment(lib, "clang/lib/LLVMOption.lib")
#pragma comment(lib, "clang/lib/LLVMPasses.lib")
#pragma comment(lib, "clang/lib/LLVMProfileData.lib")
#pragma comment(lib, "clang/lib/LLVMRuntimeDyld.lib")
#pragma comment(lib, "clang/lib/LLVMScalarOpts.lib")
#pragma comment(lib, "clang/lib/LLVMSelectionDAG.lib")
#pragma comment(lib, "clang/lib/LLVMSupport.lib")
#pragma comment(lib, "clang/lib/LLVMTarget.lib")
#pragma comment(lib, "clang/lib/LLVMTransformUtils.lib")
#pragma comment(lib, "clang/lib/LLVMVectorize.lib")

////////////////////////////////////////////////////////////////////////////
// Supported Targets
////////////////////////////////////////////////////////////////////////////
#pragma comment(lib, "clang/lib/LLVMAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMAsmPrinter.lib")

#pragma comment(lib, "clang/lib/LLVMXCoreAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMXCoreCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMXCoreDesc.lib")
#pragma comment(lib, "clang/lib/LLVMXCoreInfo.lib")

#pragma comment(lib, "clang/lib/LLVMX86AsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMX86AsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMX86CodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMX86Desc.lib")
#pragma comment(lib, "clang/lib/LLVMX86Info.lib")
#pragma comment(lib, "clang/lib/LLVMX86Utils.lib")

#pragma comment(lib, "clang/lib/LLVMAArch64AsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMAArch64AsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMAArch64CodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMAArch64Desc.lib")
#pragma comment(lib, "clang/lib/LLVMAArch64Info.lib")
#pragma comment(lib, "clang/lib/LLVMAArch64Utils.lib")

#pragma comment(lib, "clang/lib/LLVMAMDGPUAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMAMDGPUAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMAMDGPUCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMAMDGPUDesc.lib")
#pragma comment(lib, "clang/lib/LLVMAMDGPUInfo.lib")
#pragma comment(lib, "clang/lib/LLVMAMDGPUUtils.lib")

#pragma comment(lib, "clang/lib/LLVMARMAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMARMAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMARMCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMARMDesc.lib")
#pragma comment(lib, "clang/lib/LLVMARMInfo.lib")
#pragma comment(lib, "clang/lib/LLVMARMUtils.lib")

#pragma comment(lib, "clang/lib/LLVMBPFAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMBPFAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMBPFCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMBPFDesc.lib")
#pragma comment(lib, "clang/lib/LLVMBPFInfo.lib")

#pragma comment(lib, "clang/lib/LLVMHexagonAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMHexagonCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMHexagonDesc.lib")
#pragma comment(lib, "clang/lib/LLVMHexagonInfo.lib")

#pragma comment(lib, "clang/lib/LLVMLanaiAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMLanaiAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMLanaiCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMLanaiDesc.lib")
#pragma comment(lib, "clang/lib/LLVMLanaiInfo.lib")

#pragma comment(lib, "clang/lib/LLVMMipsAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMMipsAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMMipsCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMMipsDesc.lib")
#pragma comment(lib, "clang/lib/LLVMMipsInfo.lib")

#pragma comment(lib, "clang/lib/LLVMMSP430AsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMMSP430AsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMMSP430CodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMMSP430Desc.lib")
#pragma comment(lib, "clang/lib/LLVMMSP430Info.lib")

#pragma comment(lib, "clang/lib/LLVMNVPTXAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMNVPTXCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMNVPTXDesc.lib")
#pragma comment(lib, "clang/lib/LLVMNVPTXInfo.lib")

#pragma comment(lib, "clang/lib/LLVMPowerPCAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMPowerPCAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMPowerPCCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMPowerPCDesc.lib")
#pragma comment(lib, "clang/lib/LLVMPowerPCInfo.lib")

#pragma comment(lib, "clang/lib/LLVMSparcAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMSparcAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMSparcCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMSparcDesc.lib")
#pragma comment(lib, "clang/lib/LLVMSparcInfo.lib")

#pragma comment(lib, "clang/lib/LLVMSystemZAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMSystemZAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMSystemZCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMSystemZDesc.lib")
#pragma comment(lib, "clang/lib/LLVMSystemZInfo.lib")

#pragma comment(lib, "clang/lib/LLVMWebAssemblyAsmParser.lib")
#pragma comment(lib, "clang/lib/LLVMWebAssemblyAsmPrinter.lib")
#pragma comment(lib, "clang/lib/LLVMWebAssemblyCodeGen.lib")
#pragma comment(lib, "clang/lib/LLVMWebAssemblyDesc.lib")
#pragma comment(lib, "clang/lib/LLVMWebAssemblyInfo.lib")
#endif
