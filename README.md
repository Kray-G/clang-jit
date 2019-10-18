# clang-jit

Example of C Compiler using JIT Engine of Clang.

## Overview

This is a sample of how to use Clang JIT Engine.
I did a lot of searching how to do it, combining information, so many try & error, and I finally got it.
I hope this information will help a lot of people.

Note: This information only for Windows.

## Building CLang

See [clang/README.md](clang/README.md) for details.
I tried this with the version 8.0.0 of Clang.

## Point Of View.

*   If you want to use a host prgrams funcion, please see [predef.cpp](predef.cpp).
*   [jit.h](jit.h) is available for C and C++. It defines `ClangJitCompiler` class wrapping C functions.

