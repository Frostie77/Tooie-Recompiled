#!/usr/bin/env bash
# Clean Ubuntu build host -> Android ARM64 APK. No device or publishing commands.
set -euo pipefail
source_root=$(cd "$(dirname "$0")/../.." && pwd)
work=${TOOIE_CI_ROOT:-"$source_root/.ci-android"}
sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
: "${sdk:?Set ANDROID_HOME to the installed Android SDK}"
ndk_version=${ANDROID_NDK_VERSION:-28.2.13676358}
ndk="$sdk/ndk/$ndk_version"
toolchain="$ndk/build/cmake/android.toolchain.cmake"
[[ -f "$toolchain" ]] || { echo "Missing NDK $ndk_version: $toolchain" >&2; exit 1; }
mkdir -p "$work/logs"
exec > >(tee "$work/logs/${1:?Expected prepare, native, package or candidate}.log") 2>&1
export CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-2}
sdl="$work/prefixes/sdl"
freetype="$work/prefixes/freetype"
common=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$toolchain"
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_POSITION_INDEPENDENT_CODE=ON)
case "$1" in
prepare)
  python3 "$source_root/tools/ci/prepare_android.py" "$source_root" "$work"
  sdl_source=$(find "$work/sources/sdl" -mindepth 1 -maxdepth 1 -type d)
  ft_source=$(find "$work/sources/freetype" -mindepth 1 -maxdepth 1 -type d)
  cmake -S "$sdl_source" -B "$work/sdl-build" "${common[@]}" \
    -DCMAKE_INSTALL_PREFIX="$sdl" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF
  cmake --build "$work/sdl-build"
  cmake --install "$work/sdl-build"
  cmake -S "$ft_source" -B "$work/freetype-build" "${common[@]}" \
    -DCMAKE_INSTALL_PREFIX="$freetype" -DBUILD_SHARED_LIBS=OFF \
    -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON \
    -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
  cmake --build "$work/freetype-build"
  cmake --install "$work/freetype-build"
  cmake -S "$source_root/lib/rt64/src/tools/file_to_c" -B "$source_root/build-host-tools" \
    -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build "$source_root/build-host-tools"
  ;;
native)
  revision=$(git -C "$source_root" rev-parse HEAD)
  run=${GITHUB_RUN_NUMBER:-0}
  attempt=${GITHUB_RUN_ATTEMPT:-1}
  [[ "$run" =~ ^[0-9]+$ && "$attempt" =~ ^[0-9]+$ ]] || exit 1
  export BT_VERSION="${TOOIE_RELEASE_VERSION:-0.1.0-ci.$run.$attempt+g${revision:0:12}}"
  [[ "$BT_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?$ ]] || {
    echo 'Invalid TOOIE_RELEASE_VERSION; expected a semantic version.' >&2; exit 1;
  }
  printf '%s\n' "$BT_VERSION" > "$work/logs/version.txt"
  flags='-DTOOIE_ANDROID_TIMING_TRACE=0 -DTOOIE_ANDROID_GEOMETRY_TRACE=0 -DTOOIE_ANDROID_CORRECTION_TRACE=0 -DTOOIE_VULKAN_QUEUE_ISOLATION_TEST=0'
  cmake -S "$source_root" -B "$work/native" "${common[@]}" \
    -DTOOIE_ANDROID_RUNTIME=ON -DTOOIE_VULKAN_DESCRIPTOR_ALLOCATION_PROBE:BOOL=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags" \
    -DSDL2_DIR="$sdl/lib/cmake/SDL2" -DFREETYPE_LIBRARY="$freetype/lib/libfreetype.a" \
    -DFREETYPE_LIBRARY_RELEASE="$freetype/lib/libfreetype.a" \
    -DFREETYPE_INCLUDE_DIR_ft2build="$freetype/include/freetype2" \
    -DFREETYPE_INCLUDE_DIR_freetype2="$freetype/include/freetype2" \
    -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF
  python3 "$source_root/tools/ci/verify_android.py" configuration "$source_root" "$work" "$ndk"
  cmake --build "$work/native" --target BanjoTooieRecompiled
  ;;
package)
  package="$work/package"
  [[ ! -e "$package" ]] || { echo 'Use a fresh packaging directory.' >&2; exit 1; }
  cp -a "$source_root/android" "$package"
  jni="$package/app/src/main/jniLibs/arm64-v8a"
  mkdir -p "$jni"
  cp "$work/native/libmain.so" "$jni/libmain.so"
  cp "$sdl/lib/libSDL2.so" "$jni/libSDL2.so"
  gradle --no-daemon --console=plain --stacktrace -p "$package" assembleDebug
  python3 "$source_root/tools/ci/verify_android.py" apk "$source_root" "$work" "$ndk"
  ;;
candidate)
  revision=$(git -C "$source_root" rev-parse HEAD)
  version=$(cat "$work/logs/version.txt")
  code=${TOOIE_RELEASE_VERSION_CODE:-2}
  gradle --no-daemon --console=plain --stacktrace -p "$work/package" \
    "-PtooieVersionCode=$code" "-PtooieVersionName=$version" assembleRelease
  python3 "$source_root/tools/release/prepare_unsigned.py" \
    --unsigned-apk "$work/package/app/build/outputs/apk/release/app-release-unsigned.apk" \
    --built-apk "$work/artifacts/Tooie-Recompiled-Android-arm64-${revision:0:12}-Debug.apk" \
    --build-report "$work/artifacts/build-report.json" \
    --build-tools "$sdk/build-tools/36.0.0" --source-commit "$revision" \
    --version-code "$code" --output "$work/candidate"
  ;;
*) echo 'Usage: build_android.sh prepare|native|package|candidate' >&2; exit 2 ;;
esac
