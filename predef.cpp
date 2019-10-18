#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <cmath>
#include <setjmp.h>
#include "jit.h"

static FILE* getStdIn()  { return stdin;  }
static FILE* getStdOut() { return stdout; }
static FILE* getStdErr() { return stderr; }

void* ks_strdup(const char* p)
{
    return strdup(p);
}

void* ks_malloc(uint32_t size)
{
    return malloc(size);
}

void* ks_calloc(uint32_t count, uint32_t size)
{
    return calloc(count, size);
}

void *ks_realloc(void* p, int32_t size)
{
    return realloc(p, size);
}

void ks_free(void* p)
{
    free(p);
}

double ks_sin(double d1) { return sin(d1); }
double ks_cos(double d1) { return cos(d1); }
double ks_tan(double d1) { return tan(d1); }
double ks_fabs(double d1) { return fabs(d1); }
double ks_pow(double d1, double d2) { return pow(d1, d2); }
double ks_fmod(double d1, double d2) { return fmod(d1, d2); }
double ks_ceil(double d1) { return ceil(d1); }
double ks_floor(double d1) { return floor(d1); }
double ks_sqrt(double d1) { return sqrt(d1); }
double ks_exp(double d1) { return exp(d1); }
double ks_log10(double d1) { return log10(d1); }
double ks_log(double d1) { return log(d1); }
double ks_asin(double d1) { return asin(d1); }
double ks_acos(double d1) { return acos(d1); }
double ks_atan(double d1) { return atan(d1); }
double ks_atan2(double d1, double d2) { return atan2(d1, d2); }
double ks_sinh(double d1) { return sinh(d1); }
double ks_cosh(double d1) { return cosh(d1); }
double ks_tanh(double d1) { return tanh(d1); }

void setupPredefinedFunctions(ClangJitCompiler& compiler)
{
    // Memory management.
    ClangJitCompiler::addSymbol("ks_strdup", ks_strdup);
    ClangJitCompiler::addSymbol("ks_malloc", ks_malloc);
    ClangJitCompiler::addSymbol("ks_calloc", ks_calloc);
    ClangJitCompiler::addSymbol("ks_realloc", ks_realloc);
    ClangJitCompiler::addSymbol("ks_free", ks_free);

    // Math functions.
    ClangJitCompiler::addSymbol("sin", ks_sin);
    ClangJitCompiler::addSymbol("cos", ks_cos);
    ClangJitCompiler::addSymbol("tan", ks_tan);
    ClangJitCompiler::addSymbol("fabs", ks_fabs);
    ClangJitCompiler::addSymbol("pow", ks_pow);
    ClangJitCompiler::addSymbol("fmod", ks_fmod);
    ClangJitCompiler::addSymbol("ceil", ks_ceil);
    ClangJitCompiler::addSymbol("floor", ks_floor);
    ClangJitCompiler::addSymbol("sqrt", ks_sqrt);
    ClangJitCompiler::addSymbol("exp", ks_exp);
    ClangJitCompiler::addSymbol("log10", ks_log10);
    ClangJitCompiler::addSymbol("log", ks_log);
    ClangJitCompiler::addSymbol("asin", ks_asin);
    ClangJitCompiler::addSymbol("acos", ks_acos);
    ClangJitCompiler::addSymbol("atan", ks_atan);
    ClangJitCompiler::addSymbol("atan2", ks_atan2);
    ClangJitCompiler::addSymbol("sinh", ks_sinh);
    ClangJitCompiler::addSymbol("cosh", ks_cosh);
    ClangJitCompiler::addSymbol("tanh", ks_tanh);

    // Printf style formatters
    ClangJitCompiler::addSymbol("printf", printf);
    ClangJitCompiler::addSymbol("putchar", putchar);

    // JIT can't find following string functions.
    ClangJitCompiler::addSymbol("sscanf", sscanf);
    ClangJitCompiler::addSymbol("strtol", strtol);
    ClangJitCompiler::addSymbol("strtoll", strtoll);

    // Helpers for standard I/O.
    ClangJitCompiler::addSymbol("getStdIn", getStdIn);
    ClangJitCompiler::addSymbol("getStdOut", getStdOut);
    ClangJitCompiler::addSymbol("getStdErr", getStdErr);

    // File I/O interfaces
    ClangJitCompiler::addSymbol("fopen", fopen);
    ClangJitCompiler::addSymbol("fgets", fgets);
    ClangJitCompiler::addSymbol("fclose", fclose);
}
