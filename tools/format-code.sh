#!/bin/bash
set -e

CLANG_FORMAT=$(command -v clang-format || command -v clang-format-15 || command -v clang-format-14 || command -v clang-format-13)

if [ -z "$CLANG_FORMAT" ]; then
  echo "clang-format not found. Please install clang-format." >&2
  exit 1
fi

echo "Using $CLANG_FORMAT"

find src -iname '*.cpp' -o -iname '*.h' | xargs "$CLANG_FORMAT" -i
