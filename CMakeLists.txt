# NOTE: C++20 supported since CMake 3.12.0:
# https://cmake.org/cmake/help/v3.12/prop_tgt/CXX_STANDARD.html
cmake_minimum_required(VERSION 3.12.0)

# Set default build type to "Release".
# NOTE: this should be done before the project command since the latter can set
# CMAKE_BUILD_TYPE itself (it does so for nmake).
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING
		"Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
	FORCE)
endif()

# NOTE: obake is C++, however we are linking to C
# libraries (e.g., libbacktrace). Moreover, if we don't
# enable C here, on some configurations (e.g., Ubuntu
# + clang) the threading detection/setup seems to break
# down.
project(obake VERSION 0.8.0 LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/yacma")

message(STATUS "System name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "obake version: ${obake_VERSION}")

# The build options.
option(OBAKE_BUILD_TESTS "Build unit tests." OFF)
option(OBAKE_BUILD_BENCHMARKS "Build benchmarks." OFF)
option(OBAKE_WITH_LIBBACKTRACE "Use libbacktrace for improved stack traces." OFF)

# Run the YACMA compiler setup.
include(YACMACompilerLinkerSettings)
# Also the threading setup.
include(YACMAThreadingSetup)

# NOTE: on Unix systems, the correct library installation path
# could be something other than just "lib", such as "lib64",
# "lib32", etc., depending on platform/configuration. Apparently,
# CMake provides this information via the GNUInstallDirs module.
# Let's enable this for now on all Unixes except OSX.
# NOTE: potentially, this could be applicable to Cygwin as well.
#
# https://cmake.org/cmake/help/v3.15/module/GNUInstallDirs.html
# https://cmake.org/pipermail/cmake/2013-July/055375.html
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    set(_OBAKE_INSTALL_LIBDIR_DEFAULT "${CMAKE_INSTALL_LIBDIR}")
else()
    set(_OBAKE_INSTALL_LIBDIR_DEFAULT "lib")
endif()
if(NOT OBAKE_INSTALL_LIBDIR)
    set(OBAKE_INSTALL_LIBDIR "${_OBAKE_INSTALL_LIBDIR_DEFAULT}" CACHE STRING
        "Library installation directory." FORCE)
endif()
mark_as_advanced(OBAKE_INSTALL_LIBDIR)
message(STATUS "Library installation directory: ${OBAKE_INSTALL_LIBDIR}")

# Assemble the flags.
set(OBAKE_CXX_FLAGS_DEBUG ${YACMA_CXX_FLAGS} ${YACMA_CXX_FLAGS_DEBUG})
set(OBAKE_CXX_FLAGS_RELEASE ${YACMA_CXX_FLAGS})

if(YACMA_COMPILER_IS_MSVC)
    include(CheckCXXCompilerFlag)
    # Disable the idiotic minmax macros on MSVC (both cl and clang-cl).
    # Also, enable the bigobj flag and the WIN32_LEAN_AND_MEAN definitions:
    # https://stackoverflow.com/questions/11040133/what-does-defining-win32-lean-and-mean-exclude-exactly
    list(APPEND OBAKE_CXX_FLAGS_DEBUG "-DNOMINMAX" "/bigobj" "-DWIN32_LEAN_AND_MEAN")
    list(APPEND OBAKE_CXX_FLAGS_RELEASE "-DNOMINMAX" "/bigobj" "-DWIN32_LEAN_AND_MEAN")
    # Enable strict conformance mode, if supported.
    set(CMAKE_REQUIRED_QUIET TRUE)
    check_cxx_compiler_flag("/permissive-" _OBAKE_MSVC_SUPPORTS_STRICT_CONFORMANCE)
    unset(CMAKE_REQUIRED_QUIET)
    if(_OBAKE_MSVC_SUPPORTS_STRICT_CONFORMANCE)
        message(STATUS "The '/permissive-' flag is supported, enabling it.")
        list(APPEND OBAKE_CXX_FLAGS_DEBUG "/permissive-")
        list(APPEND OBAKE_CXX_FLAGS_RELEASE "/permissive-")
    endif()
    unset(_OBAKE_MSVC_SUPPORTS_STRICT_CONFORMANCE)
    if(YACMA_COMPILER_IS_CLANGXX)
        # clang-cl emits various warnings from GMP/MPFR, let's just silence them.
        # NOTE: at one point in the recent past, MSVC added an options similar to GCC's isystem:
        # https://blogs.msdn.microsoft.com/vcblog/2017/12/13/broken-warnings-theory/
        # We probably just need to wait for this to be picked up by CMake/clang-cl. Let's
        # revisit the issue in the future.
        list(APPEND _OBAKE_CLANG_CL_DISABLED_WARNINGS
            "-Wno-unused-variable"
            "-Wno-inconsistent-dllimport"
            "-Wno-unknown-pragmas"
            "-Wno-unused-parameter"
            "-Wno-sign-compare"
            "-Wno-deprecated-declarations"
            "-Wno-deprecated-dynamic-exception-spec"
            "-Wno-old-style-cast"
            "-Wno-sign-conversion"
            "-Wno-non-virtual-dtor"
            "-Wno-deprecated"
            "-Wno-shadow"
            "-Wno-shorten-64-to-32"
            "-Wno-reserved-id-macro"
            "-Wno-undef"
            "-Wno-c++98-compat-pedantic"
            "-Wno-documentation-unknown-command"
            "-Wno-zero-as-null-pointer-constant"
            "-Wno-language-extension-token"
            "-Wno-gnu-anonymous-struct"
            "-Wno-nested-anon-types"
            "-Wno-documentation"
            "-Wno-comma"
            "-Wno-nonportable-system-include-path"
            "-Wno-global-constructors"
            "-Wno-redundant-parens"
            "-Wno-exit-time-destructors"
            "-Wno-missing-noreturn"
            "-Wno-switch-enum"
            "-Wno-covered-switch-default"
            "-Wno-float-equal"
            "-Wno-double-promotion"
            "-Wno-microsoft-enum-value"
            "-Wno-missing-prototypes"
            "-Wno-implicit-fallthrough"
            "-Wno-format-nonliteral"
            "-Wno-cast-qual"
            "-Wno-disabled-macro-expansion"
            "-Wno-unused-private-field"
            "-Wno-unused-template"
            "-Wno-unused-macros"
            "-Wno-extra-semi-stmt"
            "-Wno-c++98-compat")
        list(APPEND OBAKE_CXX_FLAGS_DEBUG ${_OBAKE_CLANG_CL_DISABLED_WARNINGS})
        list(APPEND OBAKE_CXX_FLAGS_RELEASE ${_OBAKE_CLANG_CL_DISABLED_WARNINGS})
        unset(_OBAKE_CLANG_CL_DISABLED_WARNINGS)
    else()
        # Problematic MSVC cl warnings.
        list(APPEND OBAKE_CXX_FLAGS_DEBUG "/wd4459")
        list(APPEND OBAKE_CXX_FLAGS_RELEASE "/wd4459")
    endif()
endif()
if(MINGW)
    # In MinGW some tests generate big object files.
    message(STATUS "Enabling the '-Wa,-mbig-obj' flag for MinGW.")
    list(APPEND OBAKE_CXX_FLAGS_DEBUG "-Wa,-mbig-obj")
    list(APPEND OBAKE_CXX_FLAGS_RELEASE "-Wa,-mbig-obj")
endif()

# Find the dependencies.

# mp++.
# NOTE: put the minimum version in a variable
# so that we can re-use it in the config-file package
# machinery below.
set (_OBAKE_MIN_MPPP_VERSION 0.23)
find_package(mp++ REQUIRED)
if(${mp++_VERSION} VERSION_LESS ${_OBAKE_MIN_MPPP_VERSION})
    message(FATAL_ERROR "The minimum mp++ version required by obake is ${_OBAKE_MIN_MPPP_VERSION}, but version ${mp++_VERSION} was found instead.")
endif()

# abseil.
find_package(absl REQUIRED)

# Boost setup.
include(ObakeFindBoost)

# libbacktrace.
if(OBAKE_WITH_LIBBACKTRACE)
    set(OBAKE_ENABLE_LIBBACKTRACE "#define OBAKE_WITH_LIBBACKTRACE")
    find_package(libbacktrace REQUIRED)
endif()

# TBB. Try to find it first in config mode (supported
# since version 2021 after the oneTBB rename), and, if this
# fails, fall back to our own FindTBB.cmake. This is of course
# not an ideal solution, but it should work until oneTBB
# becomes widely deployed.
find_package(TBB QUIET CONFIG)
if(NOT TBB_FOUND)
    message(STATUS "TBB not found using config mode, retrying in module mode.")
    find_package(TBB REQUIRED MODULE)
else()
    message(STATUS "TBB found using config mode.")
endif()

# Depend on DbgEng on WIN32, for the stack trace support.
if(WIN32)
    find_package(DbgEng REQUIRED)
endif()

# fmt.
find_package(fmt REQUIRED)

# Wrap the CMAKE_DL_LIBS variable in an imported target.
include(ObakeFindDl)

# Initial setup of the obake target.
set(OBAKE_SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cf/cf_stream_insert.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/detail/atomic_flag_array.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/detail/hc.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/detail/to_string.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/detail/fw_utils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/stack_trace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/series.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/symbols.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tex_stream_insert.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/kpack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/polynomials/packed_monomial.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/polynomials/d_packed_monomial.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/poisson_series/d_packed_trig_monomial.cpp"
)

if(OBAKE_WITH_LIBBACKTRACE)
    set(OBAKE_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/libbacktrace_stack_trace.cpp" "${OBAKE_SRC_FILES}")
else()
    set(OBAKE_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/boost_stack_trace.cpp" "${OBAKE_SRC_FILES}")
endif()


# Make obake header files accessible in Visual Studio IDE.
if(YACMA_COMPILER_IS_MSVC)
	set(OBAKE_HEADER_FILES
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/byte_size.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/exceptions.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/hash.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/kpack.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/ranges.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/s11n.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/series.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/stack_trace.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/symbols.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/tex_stream_insert.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/type_name.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/type_traits.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/d_packed_monomial.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/monomial_diff.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/monomial_homomorphic_hash.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/monomial_integrate.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/monomial_mul.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/monomial_pow.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/monomial_range_overflow_check.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/monomial_subs.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/packed_monomial.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/polynomials/polynomial.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/degree.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/diff.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/evaluate.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/fma3.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/integrate.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/is_zero.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/negate.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/p_degree.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/pow.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/safe_cast.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/safe_convert.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/subs.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/trim.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/truncate_degree.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/math/truncate_p_degree.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_degree.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_evaluate.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_is_compatible.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_is_one.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_is_zero.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_merge_symbols.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_p_degree.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_stream_insert.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_tex_stream_insert.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_trim.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/key/key_trim_identify.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/abseil.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/atomic_flag_array.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/atomic_lock_guard.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/fcast.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/fw_utils.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/hc.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/ignore.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/it_diff_check.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/limits.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/mppp_utils.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/not_implemented.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/priority_tag.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/safe_integral_arith.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/ss_func_forward.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/to_string.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/tuple_for_each.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/type_c.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/visibility.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/detail/xoroshiro128_plus.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/cf/cf_stream_insert.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/include/obake/cf/cf_tex_stream_insert.hpp"
    )

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include/obake" PREFIX "Header Files" FILES ${OBAKE_HEADER_FILES})
    set(OBAKE_SRC_FILES ${OBAKE_SRC_FILES} ${OBAKE_HEADER_FILES})
endif()

add_library(obake SHARED "${OBAKE_SRC_FILES}")
set_property(TARGET obake PROPERTY VERSION "8.0")
set_property(TARGET obake PROPERTY SOVERSION 8)
target_compile_options(obake PRIVATE
    "$<$<CONFIG:Debug>:${OBAKE_CXX_FLAGS_DEBUG}>"
    "$<$<CONFIG:Release>:${OBAKE_CXX_FLAGS_RELEASE}>"
    "$<$<CONFIG:RelWithDebInfo>:${OBAKE_CXX_FLAGS_RELEASE}>"
    "$<$<CONFIG:MinSizeRel>:${OBAKE_CXX_FLAGS_RELEASE}>"
)
set_target_properties(obake PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(obake PROPERTIES VISIBILITY_INLINES_HIDDEN TRUE)
# Ensure that C++20 is employed when both compiling and consuming obake.
target_compile_features(obake PUBLIC cxx_std_20)
# Ensure vanilla C++ is being used.
set_property(TARGET obake PROPERTY CXX_EXTENSIONS NO)
if(YACMA_COMPILER_IS_MSVC)
    # NOTE: older abseil versions have a problem on MSVC
    # when using C++20:
    # https://github.com/abseil/abseil-cpp/issues/649
    # Hopefully we can eventually remove this.
    target_compile_definitions(obake PUBLIC -D_HAS_DEPRECATED_RESULT_OF=1)
endif()
if(YACMA_COMPILER_IS_GNUCXX AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10")
    # GCC < 10 still needs the -fconcepts flag, even if C++20 is in use.
    message(STATUS "Activating the '-fconcepts' flag for GCC < 10.")
    target_compile_options(obake PUBLIC -fconcepts)
endif()

# Add the include dirs to the obake target's interface.
target_include_directories(obake PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>)

# Link the deps.
target_link_libraries(obake PUBLIC
    Threads::Threads
    mp++::mp++
    absl::flat_hash_map
    absl::flat_hash_set
    Boost::boost
    Boost::serialization
    Boost::disable_autolinking
    TBB::tbb
    fmt::fmt
)

if(OBAKE_WITH_LIBBACKTRACE)
    target_link_libraries(obake PRIVATE libbacktrace::libbacktrace)
else()
    # NOTE: Boost stacktrace sometimes requires linking to the system
    # DL libs.
    target_link_libraries(obake PRIVATE obake::dl)
endif()

# Link to DbgEng on Windows.
if(WIN32)
    target_link_libraries(obake PRIVATE DbgEng::DbgEng)
endif()

# Configure config.hpp.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/include/obake/config.hpp" @ONLY)

# Configure the sphinx config file.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/doc/conf.py.in" "${CMAKE_CURRENT_SOURCE_DIR}/doc/conf.py" @ONLY)

# Installation of the header files.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/obake" DESTINATION include)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/obake/config.hpp" DESTINATION include/obake)

# Installation of the library.
install(TARGETS obake
    EXPORT obake_export
    LIBRARY DESTINATION "${OBAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${OBAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION bin
)

# Setup of the optional deps.
set(_OBAKE_CONFIG_OPTIONAL_DEPS)
if(OBAKE_WITH_LIBBACKTRACE)
    set(_OBAKE_CONFIG_OPTIONAL_DEPS "${_OBAKE_CONFIG_OPTIONAL_DEPS}find_package(libbacktrace REQUIRED)\n")
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/obake-config.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/obake-config.cmake" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/obake-config.cmake" DESTINATION "${OBAKE_INSTALL_LIBDIR}/cmake/obake")
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindDbgEng.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindTBB.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Findlibbacktrace.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ObakeFindBoost.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ObakeFindDl.cmake"
DESTINATION "${OBAKE_INSTALL_LIBDIR}/cmake/obake")
install(EXPORT obake_export NAMESPACE obake:: DESTINATION "${OBAKE_INSTALL_LIBDIR}/cmake/obake")
# Take care of versioning.
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/obake-config-version.cmake" COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/obake-config-version.cmake" DESTINATION "${OBAKE_INSTALL_LIBDIR}/cmake/obake")

if(OBAKE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(OBAKE_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()
