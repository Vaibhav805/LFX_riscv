# Ubuntu 24.04 riscv64 cross-toolchain
# CMAKE_SYSROOT is intentionally NOT set.
# riscv64-linux-gnu-gcc-13 has its own correct sysroot baked in.
# Setting CMAKE_SYSROOT overrides it and breaks libc resolution.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_C_COMPILER       /usr/bin/riscv64-linux-gnu-gcc-13)
set(CMAKE_CXX_COMPILER     /usr/bin/riscv64-linux-gnu-g++-13)
set(CMAKE_Fortran_COMPILER /usr/bin/riscv64-linux-gnu-gfortran-13)
set(CMAKE_FIND_ROOT_PATH
    /usr/lib/riscv64-linux-gnu
    /usr/lib/gcc-cross/riscv64-linux-gnu/13
    /usr/riscv64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE  ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM  NEVER)
