# Clang Library

## Directory Structure

This clang directory have 2 directories of `include` and `lib`.
Please copy those from clang which you built by the next section's procedure.

```
clang/
    include/
        clang/
        clang-c/
        llvm/
        llvm-c/
    lib/
        *.lib
```

## How To Build Clang

### Clang V8.0.0

We need Clang V8.0.0.
Prepare it from Clang page.

### LLVM/Clang Build with Visual Studio 2017

1.  Extract `llvm-8.0.0`, and rename it to `llvm`.
2.  Extract `cfe-8.0.0` and copy `cfe-8.0.0` under `llvm/tools`, and rename it to `clang`.
3.  Modify below.
    -   `clang/lib/Driver/ToolsChains/MSVC.cpp`
        ```c
        StringRef Linker = Args.getLastArgValue(options::OPT_fuse_ld_EQ, "link");
        
        if (Linker.equals_lower("link"))
        ```
        ↓
        ```c
        const Arg* A = Args.getLastArg(options::OPT_fuse_ld_EQ);
        StringRef Linker = A ? A->getValue() : CLANG_DEFAULT_LINKER;
        if (Linker.equals_lower("lld"))
          Linker = "lld-link";
        if (Linker.empty() || Linker.equals_lower("link")) {
        ```
4.  Extract `lld-8.0.0` and copy `lld-8.0.0` under `llvm/tools`, and rename it to `lld`.
5.  Extract `clang-tools-extra-8.0.0` and copy `clang-tools-extra-8.0.0` under `clang/tools`, and rename it to `extra`.
6.  `cmake` with following options.
    -   -Thost=x64 
    -   -G "Visual Studio 15 2017 Win64" 
    -   -DLLVM_ENABLE_DIA_SDK=OFF
    -   -DLLVM_ENABLE_TERMINFO=OFF
    -   -DLLVM_USE_CRT_RELWITHDEBINFO=MT
    -   -DLLVM_USE_CRT_MINSIZEREL=MT 
    -   -DLLVM_USE_CRT_DEBUG=MTd 
    -   -DLLVM_USE_CRT_RELEASE=MT 
    -   -DLLVM_BUILD_TEST=OFF 
    -   -DCMAKE_BUILD_TYPE=MinSizeRel 
    -   -DCLANG_DEFAULT_LINKER=lld
    -   -DLLVM_ENABLE_RTTI=OFF 
    -   -DLLVM_ENABLE_EH=OFF
    -   -DCMAKE_INSTALL_PREFIX="C:\Clang64-800"
7.  Build & install as below.
    ```sh
    $ msbuild /p:Configuration=MinSizeRel INSTALL.vcxproj
    ```



