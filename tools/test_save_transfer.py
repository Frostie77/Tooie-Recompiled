#!/usr/bin/env python3
"""Disposable host save tests. No ROM, user save, install or device access."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def run(argv, **kwargs):
    return subprocess.run(list(map(str, argv)), check=True, **kwargs)


def function(source, name):
    start = source.index(name)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    root = Path(__file__).resolve().parents[1]
    runtime = root / 'lib/N64ModernRuntime'
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jni-include', type=Path)
    parser.add_argument('--require-jni', action='store_true')
    args = parser.parse_args()
    cxx = os.environ.get('CXX', 'g++')
    with tempfile.TemporaryDirectory(prefix='tooie-save-tests-') as temporary:
        build = Path(temporary)
        flags = ['-std=c++20', '-O1', '-pthread', '-ffunction-sections', '-fdata-sections',
                 '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        for directory in [root / 'src', runtime / 'N64Recomp/include', runtime / 'librecomp/include', runtime / 'librecomp/include/librecomp',
                          runtime / 'ultramodern/include', runtime / 'thirdparty', runtime / 'thirdparty/concurrentqueue']:
            flags += ['-I' + str(directory)]
        support = [root / 'tests/save_transfer_store.cpp', runtime / 'librecomp/src/pi.cpp',
                   runtime / 'librecomp/src/eep.cpp', runtime / 'librecomp/src/files.cpp']
        objects = []
        for i, source in enumerate(support):
            obj = build / (str(i) + '.o')
            run([cxx, *flags, '-c', source, '-o', obj]); objects.append(obj)
        source = (root / 'src/save_transfer_store.cpp').read_text()
        env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'}
        for mode in ['current', 'missing_recovery_control']:
            tested = source
            if mode != 'current':
                anchor = 'if (old) save_recovery(backups.value, "before-import-save-", *old);'
                assert tested.count(anchor) == 1
                tested = tested.replace(anchor, '/* deliberate missing recovery copy */')
            cpp = build / (mode + '.cpp'); cpp.write_text(tested)
            exe = build / mode
            run([cxx, *flags, cpp, *objects, '-Wl,--gc-sections', '-Wl,--wrap=write',
                 '-Wl,--wrap=fsync', '-Wl,--wrap=renameat', '-o', exe])
            completed = subprocess.run([str(exe), str(build / (mode + '-data'))], env=env, text=True, capture_output=True)
            if mode == 'current':
                print(completed.stdout, end='')
                if completed.returncode: raise RuntimeError(completed.stderr)
            elif completed.returncode == 0:
                raise RuntimeError('Missing-recovery mutation was accepted')
            elif 'history.size() == 2' not in completed.stderr:
                raise RuntimeError('Recovery control failed for an unexpected reason: ' + completed.stderr)
            else:
                print('PASS: actual missing-recovery control rejected.')

        # Compile the exact runtime start/guard bodies with just game-selection
        # globals and a mod-notice stub. No alternate locking algorithm is used.
        recomp = (runtime / 'librecomp/src/recomp.cpp').read_text()
        gate = build / 'gate.cpp'
        gate.write_text('''#include <atomic>
#include <mutex>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <future>
#include <cassert>
#include <cstdio>
enum GameStatus {None,Running,Quit};
std::atomic<GameStatus> game_status=None;
std::mutex current_game_mutex;
std::optional<std::u8string> current_game;
std::string current_game_mode_id;
namespace recomp {void start_game(const std::u8string&,const std::string&);bool with_game_stopped(const std::function<void()>&);
namespace mods {void set_latest_game_mode_id(const std::string&) {}}}
''' + function(recomp, 'void recomp::start_game(') + '\n' + function(recomp, 'bool recomp::with_game_stopped(') + '''
int main() {
std::promise<void> inside,release,attempt;
auto allowed=release.get_future().share();
std::thread transfer([&]{assert(recomp::with_game_stopped([&]{inside.set_value();allowed.wait();assert(game_status==None);}));});
inside.get_future().wait();
std::thread start([&]{attempt.set_value();recomp::start_game(u8"bt.n64.us.1.0",{});});
attempt.get_future().wait();
assert(!current_game_mutex.try_lock());
assert(game_status==None);release.set_value();transfer.join();start.join();assert(game_status==Running);
bool called=false;assert(!recomp::with_game_stopped([&]{called=true;}));assert(!called);
game_status=Quit;assert(!recomp::with_game_stopped([&]{called=true;}));assert(!called);
std::puts("PASS: actual runtime Start Game/transfer exclusion and started/closing rejection.");
}
''')
        run([cxx, *flags, gate, '-o', build / 'gate']); run([build / 'gate'], env=env)
        java = Path(shutil.which('java') or 'java').resolve()
        compiler = shutil.which('javac')
        if compiler:
            javac = [compiler]
        else:
            helper = build / 'CompileJava.java'
            helper.write_text('import javax.tools.ToolProvider; public class CompileJava { public static void main(String[] a) { var c=ToolProvider.getSystemJavaCompiler(); if(c==null) throw new IllegalStateException("JDK compiler unavailable"); System.exit(c.run(null,System.out,System.err,a)); }}')
            javac = [java, '--source', '17', helper]
        run([*javac, '--release', '17', '-d', build / 'classes',
             root / 'android/app/src/main/java/io/github/banjotooierecompiled/SaveBackup.java',
             root / 'tests/java_save/io/github/banjotooierecompiled/SaveBackupTest.java'])
        run([java, '-ea', '-cp', build / 'classes', 'io.github.banjotooierecompiled.SaveBackupTest'])
        jni = args.jni_include or Path(os.environ.get('JAVA_HOME', java.parent.parent)) / 'include'
        if (jni / 'jni.h').is_file():
            controller = (root / 'android/app/src/main/java/io/github/banjotooierecompiled/SaveTransferController.java').read_text()
            declarations = re.findall(r'private static native ([^;]+);', controller)
            assert len(declarations) == 4
            shim = build / 'SaveTransferController.java'
            shim.write_text('package io.github.banjotooierecompiled; import java.io.IOException; final class SaveTransferController {\n' +
                            '\n'.join('static native ' + d + ';' for d in declarations) + '\n}')
            run([*javac, '--release', '17', '-d', build / 'classes', shim,
                 root / 'tests/java_save/io/github/banjotooierecompiled/NativeSaveJniTest.java'])
            adapter = build / 'jni_runtime.cpp'
            adapter.write_text('''#include <jni.h>
#include <mutex>
#include <atomic>
#include <optional>
#include <cstdio>
#include "librecomp/game.hpp"
enum GameStatus {None,Running,Quit};
std::atomic<GameStatus> game_status=None;
std::mutex current_game_mutex;
std::optional<std::u8string> current_game;
std::string current_game_mode_id;
std::filesystem::path config_path;
namespace recomp::mods {void set_latest_game_mode_id(const std::string&) {}}
''' + function(recomp, 'void recomp::start_game(') + '\n' + function(recomp, 'bool recomp::with_game_stopped(') + '\n' +
                function(recomp, 'bool ultramodern::is_game_started(') + '\n' + function(recomp, 'std::filesystem::path recomp::get_config_path(') + '''
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_NativeSaveJniTest_setup(JNIEnv* env,jclass,jstring path) {
const char* value=env->GetStringUTFChars(path,nullptr);if(!value)return;config_path=value;env->ReleaseStringUTFChars(path,value);
}
extern "C" JNIEXPORT void JNICALL Java_io_github_banjotooierecompiled_NativeSaveJniTest_start(JNIEnv*,jclass) {recomp::start_game(u8"bt.n64.us.1.0",{});}
''')
            native_flags = ['-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror', '-pthread', '-fPIC', '-shared', '-D__ANDROID__',
                            '-I' + str(jni), '-I' + str(jni / 'linux'), '-I' + str(root / 'tests/android_mod_headers')]
            native_flags += [flag for flag in flags if flag.startswith('-I')]
            run([cxx, *native_flags, root / 'src/android_save_jni.cpp', root / 'src/save_transfer_store.cpp',
                 adapter, '-o', build / 'libtooie_save_transfer_test.so'])
            result = subprocess.run([str(java), '-ea', '-Xcheck:jni', '-Djava.library.path=' + str(build),
                '-cp', str(build / 'classes'), 'io.github.banjotooierecompiled.NativeSaveJniTest', str(build / 'jni-data')],
                text=True, capture_output=True)
            print(result.stdout, end='')
            if result.returncode or 'WARNING in native method' in result.stdout + result.stderr:
                raise RuntimeError('JNI check failed: ' + result.stderr)
        elif args.require_jni or args.jni_include:
            raise RuntimeError('JNI headers required; provide --jni-include from JDK 17')
        else:
            print('JNI execution not requested; use --jni-include JDK/include --require-jni for full host coverage.')
        print('Host checks do not establish Android picker behavior or gameplay/save acceptance on Odin.')


if __name__ == '__main__':
    main()
