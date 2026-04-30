#!/bin/bash
# Pre-compile oreanalytics_wrap.cpp for all target Python versions.
#
# This script runs during the Dockerfile-Wheels-ORE build, right after the
# ORE C++ library build completes.
#
# Compilations run sequentially because sccache does not support concurrent
# invocations from parallel background processes (its env-sanitized spawn
# mechanism fails with "No such file or directory" when multiple clients race).
# Each compilation at -O0 takes ~4 minutes, which is acceptable.
#
# We use -O0 because this is SWIG glue code — thin wrappers that forward
# calls to pre-compiled ORE libraries. Optimization provides no measurable
# runtime benefit for function-call forwarding code but dramatically increases
# compile time (from ~4 min at -O0 to ~12+ min at -O1 for a ~300k line file).
#
# Usage: precompile.sh <python_version> [<python_version> ...]
#   e.g. precompile.sh cp310-cp310 cp312-cp312
#
# The python_version argument is a manylinux cpython tag like "cp310-cp310".
# The script derives include paths and version tags from this string without
# executing the Python binary, so it can run on a different OS (e.g. Debian)
# with just the manylinux Python headers copied in.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SWIG_DIR="$PROJECT_DIR/ORE-SWIG"
WRAP_SRC="$SWIG_DIR/oreanalytics_wrap.cpp"
PREBUILT_DIR="$SWIG_DIR/.prebuilt"

if [ ! -f "$WRAP_SRC" ]; then
    echo "ERROR: $WRAP_SRC not found. Run 'setup.py wrap' first."
    exit 1
fi

mkdir -p "$PREBUILT_DIR"

ARCH=$(uname -m)

# Use clang++ if available (matches the ORE library build), fall back to g++.
if command -v clang++ &> /dev/null; then
    CXX=clang++
else
    CXX=g++
fi

# Use sccache if available (wraps the compiler for caching via S3).
# Ensure the server is running before any compilation.
if command -v sccache &> /dev/null; then
    CXX="sccache $CXX"
    echo "Using compiler: $CXX (sccache enabled)"
else
    echo "Using compiler: $CXX"
fi

for PYVER in "$@"; do
    # Parse the cpython tag: "cp310-cp310" ? major=3, minor=10
    CPVER="${PYVER%%-*}"       # "cp310"
    DIGITS="${CPVER#cp}"       # "310"
    PY_MAJOR="${DIGITS:0:1}"   # "3"
    PY_MINOR="${DIGITS:1}"     # "10"

    # Derive include path from the well-known manylinux layout.
    # /opt/python/cp310-cp310/include/python3.10/
    PY_INCLUDE="/opt/python/${PYVER}/include/python${PY_MAJOR}.${PY_MINOR}"
    if [ ! -d "$PY_INCLUDE" ]; then
        echo "WARNING: $PY_INCLUDE not found, skipping $PYVER"
        continue
    fi

    # Build the output object name matching what setuptools generates.
    PY_TAG="cpython-${PY_MAJOR}${PY_MINOR}"
    PLATFORM_TAG="linux-${ARCH}"
    OBJ_DIR="$PREBUILT_DIR/${PLATFORM_TAG}-${PY_TAG}"
    OBJ_FILE="$OBJ_DIR/oreanalytics_wrap.o"

    mkdir -p "$OBJ_DIR"

    echo "Compiling oreanalytics_wrap.cpp for $PYVER ($PY_TAG) ..."
    echo "  Include: $PY_INCLUDE"
    START_TIME=$(date +%s)

    # Compile sequentially (one at a time to avoid OOM and sccache conflicts).
    #
    # We intentionally do NOT use Python's sysconfig CFLAGS here. Those flags
    # (e.g. -O3, -g, -fstack-protector-strong, -specs=...) are tuned for
    # building Python extension modules that contain real application logic.
    # This file is pure SWIG glue code — every function just marshals
    # arguments and calls into pre-compiled ORE libraries. The flags we need:
    #
    #   -O0:     No optimization. SWIG glue is call-forwarding code; the
    #            optimizer has nothing meaningful to improve but still spends
    #            ~8 extra minutes analyzing ~300k lines. -O0 vs -O1 has no
    #            measurable runtime impact on wrapper call overhead.
    #   -g0:     No debug info. Debug info for ~300k lines of template-heavy
    #            headers is enormous and useless in a shipped wheel.
    #   -DNDEBUG: Disable asserts (matches Release builds).
    #   -fPIC:   Position-independent code (required for shared libraries).
    #   -w:      Suppress warnings (SWIG-generated code triggers many).
    #
    $CXX -pthread -fPIC -DNDEBUG \
        -I"$PY_INCLUDE" \
        -c "$WRAP_SRC" \
        -o "$OBJ_FILE" \
        -w -std=c++20 -g0 -O0

    END_TIME=$(date +%s)
    ELAPSED=$((END_TIME - START_TIME))
    OBJ_SIZE=$(stat -c%s "$OBJ_FILE" 2>/dev/null || stat -f%z "$OBJ_FILE")
    echo "  ? $PYVER compiled in ${ELAPSED}s ($(( OBJ_SIZE / 1048576 )) MB)"
done

echo "All pre-compilations complete. Objects in $PREBUILT_DIR:"
find "$PREBUILT_DIR" -name '*.o' -ls
