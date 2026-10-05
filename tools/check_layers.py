#!/usr/bin/env python3
"""Fails when a file includes a module its own module may not use.

Usage: check_layers.py <layers.txt> <src dir> <tests dir>

layers.txt is written by CMake, one "module=dep,dep" line per module, so
the graph lives only in CMakeLists.txt. A module may include itself and
everything it reaches through its deps. A test may include what its
module may.
"""

import os
import re
import sys

INCLUDE = re.compile(r'^\s*#\s*include\s+"([a-z_]+)/')
MAX_MODULES = 64
MAX_FILES = 4096


def read_graph(path):
    graph = {}
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            name, _, deps = line.strip().partition("=")
            if name:
                graph[name] = [dep for dep in deps.split(",") if dep]
    assert graph, "empty layer graph"
    assert len(graph) <= MAX_MODULES, "too many modules"
    return graph


def reachable(graph, start):
    """Modules start may include: itself and all it depends on."""
    assert start in graph, start
    seen = {start}
    todo = [start]
    for _ in range(MAX_MODULES * MAX_MODULES):
        if not todo:
            break
        for dep in graph.get(todo.pop(), []):
            if dep not in seen:
                seen.add(dep)
                todo.append(dep)
    assert not todo, "layer walk did not finish"
    return seen


def code_files(root):
    found = []
    for folder, _, names in os.walk(root):
        found += [os.path.join(folder, n) for n in names
                  if n.endswith((".h", ".cpp"))]
    assert len(found) <= MAX_FILES, "too many files"
    return found


def check(graph, root):
    assert os.path.isdir(root), root
    problems = []
    for path in code_files(root):
        module = os.path.relpath(path, root).split(os.sep)[0]
        if module not in graph:
            problems.append("%s: folder %s is not a module" % (path, module))
            continue
        allowed = reachable(graph, module)
        with open(path, encoding="utf-8") as handle:
            for number, line in enumerate(handle, 1):
                match = INCLUDE.match(line)
                if match and match.group(1) not in allowed:
                    problems.append("%s:%d: %s may not include %s" %
                                    (path, number, module, match.group(1)))
    return problems


def self_test():
    graph = {"base": [], "model": ["base"], "edit": ["model"]}
    assert reachable(graph, "edit") == {"edit", "model", "base"}
    assert reachable(graph, "base") == {"base"}
    print("self-test passed")


def main():
    if "--self-test" in sys.argv:
        self_test()
        return 0
    assert len(sys.argv) == 4, __doc__
    graph = read_graph(sys.argv[1])
    problems = check(graph, sys.argv[2]) + check(graph, sys.argv[3])
    for problem in problems:
        print(problem, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
