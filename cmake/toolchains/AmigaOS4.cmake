set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR powerpc)

set(AMIGAOS4_SDK "/opt/ppc-amigaos/ppc-amigaos/SDK" CACHE PATH "AmigaOS4 SDK root")
set(AMIGAOS4_LOCAL "${AMIGAOS4_SDK}/local/newlib" CACHE PATH "AmigaOS4 local package prefix")

set(CMAKE_C_COMPILER /opt/ppc-amigaos/bin/ppc-amigaos-gcc)
set(CMAKE_CXX_COMPILER /opt/ppc-amigaos/bin/ppc-amigaos-g++)
set(CMAKE_AR /opt/ppc-amigaos/bin/ppc-amigaos-ar)
set(CMAKE_RANLIB /opt/ppc-amigaos/bin/ppc-amigaos-ranlib)
set(CMAKE_STRIP /opt/ppc-amigaos/bin/ppc-amigaos-strip)

# Executables built by the cross compiler cannot run on the build host.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH "${AMIGAOS4_SDK}" "${AMIGAOS4_LOCAL}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_PREFIX_PATH "${AMIGAOS4_LOCAL}" CACHE STRING "AmigaOS4 dependency prefixes")
set(CMAKE_INCLUDE_PATH
    "${AMIGAOS4_LOCAL}/include"
    "${AMIGAOS4_SDK}/local/common/include"
    CACHE STRING "AmigaOS4 dependency include paths")
set(CMAKE_LIBRARY_PATH "${AMIGAOS4_LOCAL}/lib" CACHE STRING "AmigaOS4 dependency library paths")

set(CMAKE_C_FLAGS_INIT "-athread=native")
set(CMAKE_CXX_FLAGS_INIT "-athread=native")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-athread=native")

# The direct VirtIO build supplies its GL ABI inside Widelands.  Do not point
# CMake at gl4es here: even a static link can pull in its SDL/Warp3DNova path.
