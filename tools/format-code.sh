#!/bin/bash
set -euo pipefail

# Detect clang-format version
CLANG_FORMAT=$(command -v clang-format || command -v clang-format-15 || command -v clang-format-14 || command -v clang-format-13)

if [ -z "$CLANG_FORMAT" ]; then
  echo "clang-format not found. Please install clang-format." >&2
  exit 1
fi

echo "Using $CLANG_FORMAT"

# Set check mode if passed as first argument
CHECK_ONLY=${1:-""}

# Format all source/header files in key directories
find src include tests -type f \( -iname '*.cpp' -o -iname '*.h' \) -print0 | while IFS= read -r -d '' file; do
  if [[ "$CHECK_ONLY" == "check" ]]; then
    echo "Checking format: $file"
    "$CLANG_FORMAT" --dry-run --Werror "$file"
  else
    echo "Formatting: $file"
    "$CLANG_FORMAT" -i "$file"
  fi
done
