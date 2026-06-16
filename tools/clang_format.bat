@echo off
REM Format every C++ source under src/ with clang-format.
REM Delegates to format.ps1 (single source of truth) so double-clicking works too.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0clang_format.ps1" %*
