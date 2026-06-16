#!/usr/bin/env bash
# Format every C++ source under src/ with clang-format, using the repo's .clang-format.
# Usage:  ./tools/format.sh
set -euo pipefail

# Repo root is the parent of this script's directory (tools/).
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"
src_dir="$repo_root/src"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format not found on PATH. Install it (e.g. 'brew install clang-format' on macOS," \
         "or your distro package on Linux) and retry." >&2
    exit 1
fi

# Portable across macOS's default bash 3.2 (no mapfile): collect then act via find -exec.
count="$(find "$src_dir" -type f \( -name '*.cpp' -o -name '*.h' \) | wc -l | tr -d ' ')"
if [ "$count" -eq 0 ]; then
    echo "No .cpp/.h files found under $src_dir"
    exit 0
fi

echo "Formatting $count files with $(clang-format --version)..."
find "$src_dir" -type f \( -name '*.cpp' -o -name '*.h' \) -exec clang-format -i {} +
echo "Done."
