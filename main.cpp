
#include <cstdio>
#include "jit.h"
void setupPredefinedFunctions(ClangJitCompiler& compiler);

extern "C" int error_handler(int level, const char *filename, int line, int column, const char *message)
{
    static int count = 0;
    static char *levelString[] = {"Ignore", "Note", "Remark", "Warning", "Error", "Fatal"};
    printf("%s(%d): %s\n", levelString[level], ++count, message);
    printf("    File: %s\n", filename);
    printf("    Line: %d\n\n", line);
    return 1;
}

int main(int ac, char **av)
{
    ClangJitCompiler compiler;
    try {
        // Use ClangJitSourceType_C_String if you want to compile from string data.
        int fileType = ClangJitSourceType_C_File;

        compiler.setOptimizeLevel(3);
        setupPredefinedFunctions(compiler);
        compiler.compile(av[1], fileType, error_handler);
        compiler.finalize();

        typedef int (*mainf_t)();
        mainf_t ff = compiler.getFunctionAddress<mainf_t>("main");
        if (ff) {
            return ff();
        }

        printf("The function main was not found.\n");
    } catch (std::exception& e) {
        printf("Error: %s\n", e.what());
    } catch (...) {
        printf("Unknown Error.\n");
    }

    return 1;
}
