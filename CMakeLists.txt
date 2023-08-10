cmake_minimum_required(VERSION 3.17)
project(cas CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

add_subdirectory(corecommon)
include_directories(./corecommon/src)

add_compile_options(-Wall)

if (EMSCRIPTEN)
    set(ENV{PKG_CONFIG_LIBDIR} "/Users/thomasmarlowe/Downloads/gmpbuild/lib/pkgconfig")

    #    set(EMCC_OPTIONS "SHELL:-s ASYNCIFY -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3 -s ALLOW_MEMORY_GROWTH -s USE_FREETYPE=1 -s USE_SDL=2 -s INITIAL_MEMORY=281608192")
    #    set(EMCC_LINK_OPTIONS "SHELL:-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -s USE_FREETYPE=1 -s USE_SDL=2")
    set(EMCC_OPTIONS "-fwasm-exceptions" "-pthread" "-msimd128")
        set(EMCC_LINK_OPTIONS ${EMCC_OPTIONS} "-sENVIRONMENT=web,worker" "-sDEMANGLE_SUPPORT" "-sMODULARIZE" "-sEXPORT_ES6=1" "-sEXPORT_NAME=\"CASModule\"" "-lembind" "-sINITIAL_MEMORY=281MB" "-sASSERTIONS" "-sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency")
    add_compile_options(${EMCC_OPTIONS})
    add_link_options(${EMCC_LINK_OPTIONS} "LINKER:--fatal-warnings")
else()
    set(ENV{PKG_CONFIG_LIBDIR} "/usr/local/Cellar/gmp/6.2.1_1/lib/pkgconfig")
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(BUILD_DEBUG)

    if (EMSCRIPTEN)
        add_link_options("-gsource-map")
    else()
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
endif()

find_package(PkgConfig REQUIRED)
#pkg_check_modules(MPFR REQUIRED IMPORTED_TARGET mpfr)
#pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3)
pkg_check_modules(GMP REQUIRED IMPORTED_TARGET gmp)
pkg_check_modules(GMPXX REQUIRED IMPORTED_TARGET gmpxx)

include_directories(${bGMP_INCLUDE_DIRS})

#file(GLOB SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(cas ./src/main.cpp)
add_dependencies(cas corecommon)

target_link_options(cas PUBLIC ${GMPXX_LDFLAGS} ${GMP_LDFLAGS})
target_link_libraries(cas corecommon PkgConfig::GMP PkgConfig::GMPXX)

if (EMSCRIPTEN)
set_target_properties(cas PROPERTIES
        SUFFIX ".mjs"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web"
        )
endif()

#add_custom_command(TARGET cas POST_BUILD
#                   COMMAND sed -i '' -literal "'s/\"cas.wasm.map\"/require(\"url:.\\/cas.wasm.map\")/g'" '${CMAKE_CURRENT_SOURCE_DIR}/web/cas.mjs'
#                   COMMENT "Replacing cas.wasm in cas.mjs"
#                    DEPENDS cas
#                   )
#
