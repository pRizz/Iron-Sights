# Cross-compile toolchain: 64-bit Windows PE via MinGW-w64 (WIN64-001,
# scripts/port/windows_x64_plan.md). Builds only GE_POINTER_BITS=64.
# Usage: cmake -B build_win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-x86_64.cmake -DGE_POINTER_BITS=64
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_OBJCOPY x86_64-w64-mingw32-objcopy)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
