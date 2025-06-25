#!/bin/bash
set -e

CLANG_FORMAT=$(command -v clang-format || command -v clang-format-15 || command -v clang-format-14)

if [ -z "$CLANG_FORMAT" ]; then
  echo "clang-format not found. Please install clang-format." >&2
  exit 1
fi

echo "Using $CLANG_FORMAT"

# Only run formatting on existing directories
for DIR in src include tests; do
  if [ -d "$DIR" ]; then
    echo "Formatting files in $DIR/"
    find "$DIR" -type f \( -iname '*.cpp' -o -iname '*.h' \) | xargs "$CLANG_FORMAT" -i
  else
    echo "Skipping missing directory: $DIR"
  fi
done
