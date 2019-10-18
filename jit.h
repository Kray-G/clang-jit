#ifndef KS_JIT_H_
#define KS_JIT_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum ClangJitOption_ {
    ClangJitOption_DegugMode = 0,
    ClangJitOption_OptimizeLevel,
    ClangJitOption_WarningLimit,
    ClangJitOption_ErrorLimit,
    ClangJitOption_MaxOptionCount,
} ClangJitOption;

typedef enum ClangJitSourceType_ {
    ClangJitSourceType_C_String = 0,
    ClangJitSourceType_CXX_String,
    ClangJitSourceType_C_File,
    ClangJitSourceType_CXX_File,
} ClangJitSourceType;

typedef int (*error_handler_t)(int level, const char *filename, int line, int column, const char *message);
extern void *clang_JitAllocContext();
extern void clang_JitFreeContext(void *ctx);
extern const char *clang_JitLoadSharedFile(const char *name);
extern const char *clang_JitLoadObjectFile(void *ctx, const char *name);
extern const char *clang_JitOutputObjectFile(void *ctx, const char *name);
extern void clang_JitAddSymbol(const char *name, void *value);
extern void* clang_JitSearchSymbol(const char *name);
extern void clang_JitSetOptionInt(void *ctx, int key, int value);
extern void clang_JitSetTriple(void *ctx, const char *triple);
extern void clang_JitIrDump(void *ctx, const char *filename);
extern int clang_JitIrSave(void *ctx, const char *filename);
extern const char *clang_JitIrLoad(void *ctx, const char *filename);
extern const char *clang_JitIrMergeFile(void *ctx, const char *filename);
extern const char *clang_JitIrMerge(void *ctx, void *another);
extern void *clang_JitIrCompile(void *ctx, const char *source, int type, error_handler_t handler);
extern void *clang_JitIrOptimize(void *ctx);
extern void *clang_JitGenerateTargetCode(void *ctx);
extern void *clang_JitGetFunctionAddress(void *ctx, const char *name);
extern void clang_JitFinalizeCode(void *ctx);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus
#include <stdexcept>

class ClangJitCompiler
{
public:
    ClangJitCompiler() :
        ctx_(clang_JitAllocContext())
    {
    }

    ~ClangJitCompiler()
    {
        clang_JitFreeContext(ctx_);
    }

    static void loadSharedFile(const char *name)
    {
        const char *errmsg = clang_JitLoadSharedFile(name);
        if (errmsg) {
            throw std::runtime_error(errmsg);
        }
    }

    static void addSymbol(const char *name, void *value)
    {
        clang_JitAddSymbol(name, value);
    }

    template<typename R>
    static R searchSymbol(const char *name)
    {
        return static_cast<R>(clang_JitSearchSymbol(name));
    }

    ClangJitCompiler& setOption(int key, int value)
    {
        clang_JitSetOptionInt(ctx_, key, value);
        return *this;
    }

    ClangJitCompiler& setDebugMode(bool debug = true)
    {
        clang_JitSetOptionInt(ctx_, ClangJitOption_DegugMode, debug);
        return *this;
    }

    ClangJitCompiler& setOptimizeLevel(int level)
    {
        clang_JitSetOptionInt(ctx_, ClangJitOption_OptimizeLevel, level);
        return *this;
    }

    ClangJitCompiler& setWarningLimit(int limit)
    {
        clang_JitSetOptionInt(ctx_, ClangJitOption_WarningLimit, limit);
        return *this;
    }

    ClangJitCompiler& setErrorLimit(int limit)
    {
        clang_JitSetOptionInt(ctx_, ClangJitOption_ErrorLimit, limit);
        return *this;
    }

    ClangJitCompiler& setTriple(const std::string& triple)
    {
        clang_JitSetTriple(ctx_, triple.c_str());
        return *this;
    }

    void saveObjectFile(const char* filename)
    {
        const char *errmsg = clang_JitOutputObjectFile(ctx_, filename);
        if (errmsg) {
            throw std::runtime_error(errmsg);
        }
    }

    void loadObjectFile(const char* filename)
    {
        const char *errmsg = clang_JitLoadObjectFile(ctx_, filename);
        if (errmsg) {
            throw std::runtime_error(errmsg);
        }
    }

    void dump(const char* filename = nullptr) const
    {
        clang_JitIrDump(ctx_, filename);
    }

    void save(const char* filename)
    {
        if (clang_JitIrSave(ctx_, filename) == 0) {
            throw std::runtime_error("Failed to save a bitcode file.");
        }
    }

    void load(const char* filename)
    {
        const char *errmsg = clang_JitIrLoad(ctx_, filename);
        if (errmsg) {
            throw std::runtime_error(errmsg);
        }
    }

    void merge(const char* filename)
    {
        const char *errmsg = clang_JitIrMergeFile(ctx_, filename);
        if (errmsg) {
            throw std::runtime_error(errmsg);
        }
    }

    void merge(ClangJitCompiler* another)
    {
        const char *errmsg = clang_JitIrMerge(ctx_, another->ctx_);
        if (errmsg) {
            throw std::runtime_error(errmsg);
        }
    }

    ClangJitCompiler& generateIR(const char *source, int type, error_handler_t handler)
    {
        if (!clang_JitIrCompile(ctx_, source, type, handler)) {
            throw std::runtime_error("Compile error.");
        }
        return *this;
    }

    ClangJitCompiler& optimizeIR()
    {
        if (!clang_JitIrOptimize(ctx_)) {
            throw std::runtime_error("Optimization error.");
        }
        return *this;
    }

    ClangJitCompiler& generateTargetCode()
    {
        if (!clang_JitGenerateTargetCode(ctx_)) {
            throw std::runtime_error("Failed to generate a target code.");
        }
        return *this;
    }

    ClangJitCompiler& compile(const char *source, int type, error_handler_t handler)
    {
        if (!clang_JitIrCompile(ctx_, source, type, handler)) {
            throw std::runtime_error("Compile error.");
        }
        if (!clang_JitIrOptimize(ctx_)) {
            throw std::runtime_error("Optimization error.");
        }
        return generateTargetCode();
    }

    void finalize()
    {
        clang_JitFinalizeCode(ctx_);
    }

    template<typename R>
    R getFunctionAddress(const char* name)
    {
        void *addr = clang_JitGetFunctionAddress(ctx_, name);
        if (!addr) {
            throw std::runtime_error("Function not found.");
        }
        return static_cast<R>(addr);
    }

private:
    void *ctx_;
};
#endif // __cplusplus

#endif // KSJIT_JIT_H_
