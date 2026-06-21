#!/usr/bin/env python3
"""Count lines of code in the Hob2D project, grouped by language.

Counts source files under the project root, separated into C++, Lua, and HLSL.
For each group it reports total lines, code lines (non-blank), and file count.

Usage:
    python tools/count_lines.py
"""

import os
import sys

# Directory of this script -> project root is one level up.
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Map a display group name to the file extensions that belong to it.
GROUPS = {
    "C++": {".cpp", ".h", ".hpp", ".cc", ".cxx", ".inl"},
    "Lua": {".lua"},
    "HLSL": {".hlsl"},
}

# Directories to skip entirely (build artifacts, deps, VCS, generated copies).
SKIP_DIRS = {"build", ".git", ".vs", "out", "vcpkg_installed", "__pycache__"}

# Generated Lua files should not count toward hand-written totals.
SKIP_SUFFIXES = (".generated.lua",)


def group_for(filename):
    ext = os.path.splitext(filename)[1].lower()
    for group, extensions in GROUPS.items():
        if ext in extensions:
            return group
    return None


def count_file(path):
    """Return (total_lines, code_lines) for a single file."""
    total = 0
    code = 0
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            total += 1
            if line.strip():
                code += 1
    return total, code


def main():
    stats = {group: {"files": 0, "total": 0, "code": 0} for group in GROUPS}

    for dirpath, dirnames, filenames in os.walk(PROJECT_ROOT):
        # Prune skipped directories in place so os.walk does not descend into them.
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]

        for filename in filenames:
            if filename.endswith(SKIP_SUFFIXES):
                continue
            group = group_for(filename)
            if group is None:
                continue
            path = os.path.join(dirpath, filename)
            try:
                total, code = count_file(path)
            except OSError as e:
                print(f"warning: could not read {path}: {e}", file=sys.stderr)
                continue
            stats[group]["files"] += 1
            stats[group]["total"] += total
            stats[group]["code"] += code

    header = f"{'Group':<8}{'Files':>8}{'Lines':>12}{'Code':>12}"
    print(header)
    print("-" * len(header))

    totals = {"files": 0, "total": 0, "code": 0}
    for group in GROUPS:
        s = stats[group]
        print(f"{group:<8}{s['files']:>8}{s['total']:>12,}{s['code']:>12,}")
        for key in totals:
            totals[key] += s[key]

    print("-" * len(header))
    print(f"{'Total':<8}{totals['files']:>8}{totals['total']:>12,}{totals['code']:>12,}")


if __name__ == "__main__":
    main()
