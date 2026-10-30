#!/usr/bin/env python3
"""Compile actual menu navigation producer/consumer against controlled DOM inputs."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ELEMENT = ROOT / "lib/RecompFrontend/recompui/src/elements/ui_element.cpp"
DOCUMENT = ROOT / "lib/RecompFrontend/recompui/src/elements/ui_document.cpp"


def source(path):
    if path.exists():
        return path.read_text()
    # Sparse workspaces keep these vendored sources in Git.
    return subprocess.check_output(["git", "show", "HEAD:" + str(path.relative_to(ROOT))], cwd=ROOT).decode()


def function(text, signature):
    start = text.index(signature)
    end = text.index("{", start) + 1
    level = 1
    while level:
        level += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def consumer(text):
    start = text.index("    // struct for helping with rml element position comparisons.")
    end = text.index("    Document::Document(", start)
    return text[start:end] + function(text, "bool Document::handle_navigation_event(")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    compiler = os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise SystemExit("C++ compiler required")
    element = source(ELEMENT)
    producer = "\n".join(function(element, signature) for signature in [
        "Element *Element::get_nav_parent(", "Element::CanFocus Element::is_focusable(",
        "void Element::get_all_focusable_children(", "void Element::build_navigation("])
    document = source(DOCUMENT)
    with tempfile.TemporaryDirectory(prefix="tooie-ui-navigation-") as name:
        directory = Path(name)
        (directory / "navigation_producer.inc").write_text(producer)
        (directory / "navigation_consumer.inc").write_text(consumer(document))
        binary = directory / "navigation"
        command = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wno-unused-variable", "-Wno-unused-but-set-variable",
                   "-I" + str(directory), str(ROOT / "tests/ui_navigation.cpp"), "-o", str(binary)]
        if args.sanitize:
            command.extend(["-fsanitize=address,undefined", "-fno-omit-frame-pointer"])
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
        # Replace only the parent lookup/recovery with the old production block.
        # The same actual builder and event consumer must reproduce its throw.
        first = document.index("            Element *nav_parent = find_nav_parent(document, this_element);")
        last = document.index("            // Process grid navigation", first)
        old_block = '''            Element *nav_parent = this_element->get_nav_parent();
            if (nav_parent == nullptr) {
                return this_element;
            }

            int cur_element_index = get_element_index(this_element, nav_parent->nav_children);
            if (cur_element_index == -1) {
                throw std::runtime_error("This should never ever ever happen. Element not found in its parent's nav children.");
            }

'''
        old = document[:first] + old_block + document[last:]
        (directory / "navigation_consumer.inc").write_text(consumer(old))
        subprocess.run(command, check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        assert result.returncode == 1 and "Element not found in its parent's nav children" in result.stderr, result
        print("PASS: pre-fix production consumer throws the captured navigation exception with the same producer and DOM inputs")


if __name__ == "__main__":
    main()
