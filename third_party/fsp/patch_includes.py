#!/usr/bin/env python3
"""Make F's Plugins include lines work on Linux.

The sources are written for Windows: include paths use backslashes and
do not always match the real file name's case. This rewrites every
quoted #include in the given files to forward slashes and to the case
of the file as it is in the original source tree, in place, and prints
one line per change so it can be pasted into VENDORED.md.

Usage: patch_includes.py <F's source root> <file> [<file> ...]
The source root is the folder holding _NFLib and the plugin folders.
"""

import os
import re
import sys

INCLUDE = re.compile(rb'^(\s*#\s*include\s*")([^"]+)(".*)$')


def real_case(root, file_dir, path):
    """Returns path with each part in the case found on disk, or None."""
    here = os.path.relpath(file_dir, root) if file_dir else "."
    parts = path.split("/")
    current = os.path.normpath(os.path.join(root, here))
    fixed = []
    for part in parts:
        if part in ("..", "."):
            current = os.path.normpath(os.path.join(current, part))
            fixed.append(part)
            continue
        if not os.path.isdir(current):
            return None
        names = {name.lower(): name for name in os.listdir(current)}
        name = names.get(part.lower())
        if name is None:
            return None
        current = os.path.join(current, name)
        fixed.append(name)
    return "/".join(fixed)


def patch(root, vendored_root, file_path):
    with open(file_path, "rb") as handle:
        lines = handle.read().split(b"\n")
    rel_dir = os.path.dirname(os.path.relpath(file_path, vendored_root))
    source_dir = os.path.join(root, rel_dir)
    changed = False
    for index, line in enumerate(lines):
        match = INCLUDE.match(line)
        if not match:
            continue
        old = match.group(2).decode("latin-1")
        new = old.replace("\\", "/")
        cased = real_case(root, source_dir, new)
        if cased is not None:
            new = cased
        if new == old:
            continue
        lines[index] = (match.group(1) + new.encode("latin-1") +
                        match.group(3))
        changed = True
        print(f"{os.path.relpath(file_path, vendored_root)}:{index + 1}: "
              f"\"{old}\" -> \"{new}\"")
    if changed:
        with open(file_path, "wb") as handle:
            handle.write(b"\n".join(lines))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    root = os.path.abspath(sys.argv[1])
    vendored_root = os.path.dirname(os.path.abspath(__file__))
    for file_path in sys.argv[2:]:
        patch(root, vendored_root, os.path.abspath(file_path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
