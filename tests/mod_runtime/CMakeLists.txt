cmake_minimum_required(VERSION 3.20)
project(TooieModRuntimeChecks LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(RUNTIME "${ROOT}/lib/N64ModernRuntime")
add_subdirectory("${RUNTIME}/N64Recomp" recomp)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/miniz_export.h" "#pragma once\n#define MINIZ_EXPORT\n")
add_executable(mod_runtime
    "${ROOT}/tests/mod_runtime.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/version.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/boot-consumer.cpp"
    "${RUNTIME}/librecomp/src/mods.cpp"
    "${RUNTIME}/librecomp/src/mod_manifest.cpp"
    "${RUNTIME}/librecomp/src/mod_config_api.cpp"
    "${RUNTIME}/librecomp/src/mod_events.cpp"
    "${RUNTIME}/librecomp/src/mod_hooks.cpp"
    "${RUNTIME}/librecomp/src/overlays.cpp"
    "${RUNTIME}/librecomp/src/config.cpp"
    "${RUNTIME}/librecomp/src/config_option.cpp"
    "${RUNTIME}/librecomp/src/files.cpp"
    "${RUNTIME}/librecomp/src/patcher.cpp"
    "${RUNTIME}/thirdparty/miniz/miniz.c"
    "${RUNTIME}/thirdparty/miniz/miniz_zip.c"
    "${RUNTIME}/thirdparty/miniz/miniz_tinfl.c"
    "${RUNTIME}/thirdparty/miniz/miniz_tdef.c")
target_include_directories(mod_runtime PRIVATE
    "${ROOT}/src" "${ROOT}/tests/android_mod_headers" "${CMAKE_CURRENT_BINARY_DIR}"
    "${RUNTIME}/librecomp/include" "${RUNTIME}/librecomp/include/librecomp" "${RUNTIME}/ultramodern/include"
    "${RUNTIME}/thirdparty" "${RUNTIME}/thirdparty/concurrentqueue"
    "${RUNTIME}/thirdparty/miniz")
target_compile_definitions(mod_runtime PRIVATE __ANDROID__ _POSIX_C_SOURCE=200809L _FILE_OFFSET_BITS=64)
target_compile_options(mod_runtime PRIVATE -ffunction-sections -fdata-sections -fno-omit-frame-pointer)
target_link_options(mod_runtime PRIVATE -Wl,--gc-sections)
find_package(zstd CONFIG REQUIRED)
target_link_libraries(mod_runtime PRIVATE LiveRecomp dl pthread zstd::libzstd_static)
if(TOOIE_MOD_TEST_SANITIZERS)
    target_compile_options(mod_runtime PRIVATE -fsanitize=address,undefined)
    target_link_options(mod_runtime PRIVATE -fsanitize=address,undefined)
endif()
