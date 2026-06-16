#!/usr/bin/env pwsh
# Format every C++ source under src/ with clang-format, using the repo's .clang-format.
# Usage:  pwsh tools/format.ps1   (or right-click > Run with PowerShell)

$ErrorActionPreference = "Stop"

# Repo root is the parent of this script's directory (tools/).
$repoRoot = Split-Path -Parent $PSScriptRoot
$srcDir = Join-Path $repoRoot "src"

if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    Write-Error "clang-format not found on PATH. Install it (e.g. 'pip install clang-format') and reopen your terminal."
    exit 1
}

$files = Get-ChildItem -Recurse -Path $srcDir -Include *.cpp, *.h
if (-not $files) {
    Write-Output "No .cpp/.h files found under $srcDir"
    exit 0
}

Write-Output "Formatting $($files.Count) files with $(clang-format --version)..."
foreach ($file in $files) {
    & clang-format -i $file.FullName
}
Write-Output "Done."
