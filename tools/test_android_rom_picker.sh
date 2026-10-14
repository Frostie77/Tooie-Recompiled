#!/usr/bin/env bash
# No ROM, APK, emulator or device needed. Tests production Java copy and JNI code.
set -euo pipefail
repo_root=$(cd "$(dirname "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
picker_jdk=${TOOIE_JDK:-${JAVA_HOME:-}}
if [ -z "$picker_jdk" ]; then
    echo 'Set TOOIE_JDK or JAVA_HOME to a JDK (17+).' >&2; exit 1
fi
java_package=io.github.banjotooierecompiled
"$picker_jdk/bin/javac" --release 17 -d "$test_dir/classes" \
    "$repo_root/android/app/src/main/java/io/github/banjotooierecompiled/RomImporter.java" \
    "$repo_root"/tests/java/io/github/banjotooierecompiled/*.java
"${CXX:-clang++}" -std=c++20 -D__ANDROID__ -fPIC -shared -pthread \
    -I"$repo_root/tests/android_picker_headers" -I"$picker_jdk/include" -I"$picker_jdk/include/linux" \
    -I"$repo_root/tests/desktop_picker_headers" \
    -I"$repo_root/lib/RecompFrontend/recompui/lib/RmlUi/Include" \
    -I"$repo_root/lib/RecompFrontend/recompui/include" \
    "$repo_root/lib/RecompFrontend/recompui/src/util/android_file_dialog.cpp" \
    "$repo_root/lib/RecompFrontend/recompui/src/util/file.cpp" \
    "$repo_root/tests/android_rom_picker_jni.cpp" -o "$test_dir/libtooie_picker_test.so"
"$picker_jdk/bin/java" -ea -cp "$test_dir/classes" "$java_package.RomImporterTest"
"$picker_jdk/bin/java" -ea -Xcheck:jni -Djava.library.path="$test_dir" -cp "$test_dir/classes" "$java_package.RomPickerJniTest"
read -ra sdl_flags <<< "$(pkg-config --cflags sdl2)"
"${CXX:-clang++}" -std=c++20 "${sdl_flags[@]}" \
    -I"$repo_root/tests/desktop_picker_headers" \
    -I"$repo_root/lib/RecompFrontend/recompui/lib/RmlUi/Include" \
    -I"$repo_root/lib/RecompFrontend/recompui/include" \
    -I"$repo_root/lib/rt64/src/contrib/nativefiledialog-extended/src/include" \
    "$repo_root/lib/RecompFrontend/recompui/src/util/file.cpp" \
    "$repo_root/tests/desktop_file_dialog.cpp" -o "$test_dir/desktop"
"$test_dir/desktop"
