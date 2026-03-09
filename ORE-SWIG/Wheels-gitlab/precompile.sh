#!/bin/bash
# Pre-compile oreanalytics_wrap.cpp for all target Python versions.
#
# This script runs during the Dockerfile-Wheels-ORE build, right after the
# ORE C++ library build completes. At this point all ORE/Boost headers are
# warm in the OS page cache, making compilation dramatically faster than
# doing it cold in a separate container.
#
# Compilations run sequentially to avoid OOM on memory-constrained CI runners
# (a single compilation of this ~300k-line file needs most of available RAM).
#
# We use -O1 because this is SWIG glue code that just forwards calls to
# pre-compiled ORE libraries — heavy optimization provides no runtime benefit
# but significantly increases compile time and memory usage.
#
# Usage: precompile.sh <python_version> [<python_version> ...]
#   e.g. precompile.sh cp310-cp310 cp312-cp312

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

# Use ccache if available (wraps the compiler for caching across runs).
if command -v ccache &> /dev/null; then
    CXX="ccache $CXX"
    echo "Using compiler: $CXX (ccache enabled)"
else
    echo "Using compiler: $CXX"
fi

# strip_debug_and_opt_flags <flags_string>
#
# Removes flags that are harmful for compiling SWIG glue code:
#   -g, -g<N>, -ggdb<N> etc. — debug info generation is extremely expensive
#     for a ~300k line file and produces multi-GB .o files; not needed for
#     a wheel binary that will never be debugged at the wrapper level.
#   -O<N> — we override with -O1; stripping first avoids relying on
#     "last flag wins" behaviour across compiler versions.
#   -specs=... — RPM/distro hardening specs that slow compilation.
#   -fstack-protector* — runtime hardening overhead, no benefit in glue code.
#   -fcf-protection — control-flow hardening, adds overhead.
strip_debug_and_opt_flags() {
    local result=()
    for flag in $1; do
        case "$flag" in
            -g|-g[0-9]|-ggdb|-ggdb[0-9]|-gdwarf*|-gsplit-dwarf) ;;
            -O|-O[0-9]|-Os|-Oz|-Ofast|-Og) ;;
            -specs=*) ;;
            -fstack-protector*) ;;
            -fcf-protection*) ;;
            *) result+=("$flag") ;;
        esac
    done
    echo "${result[*]}"
}

for PYVER in "$@"; do
    PYBIN="/opt/python/${PYVER}/bin"
    if [ ! -d "$PYBIN" ]; then
        echo "WARNING: $PYBIN not found, skipping $PYVER"
        continue
    fi

    PYTHON="$PYBIN/python"
    if [ ! -x "$PYTHON" ]; then
        echo "WARNING: $PYTHON not found, skipping $PYVER"
        continue
    fi

    # Extract sysconfig variables matching what setuptools uses for compilation.
    PY_INCLUDE=$($PYTHON -c "import sysconfig; print(sysconfig.get_path('include'))")
    PY_PLATINCLUDE=$($PYTHON -c "import sysconfig; print(sysconfig.get_path('platinclude'))")

    # Get compiler flags from sysconfig (same source as setuptools/distutils).
    PY_CFLAGS_RAW=$($PYTHON -c "import sysconfig; print(sysconfig.get_config_var('CFLAGS') or '')")
    # CCSHARED provides -fPIC on Linux.
    PY_CCSHARED=$($PYTHON -c "import sysconfig; print(sysconfig.get_config_var('CCSHARED') or '')")

    # Strip debug info (-g*) and optimization (-O*) flags from Python's CFLAGS.
    # Python's sysconfig CFLAGS for manylinux images typically contain -g (full
    # DWARF debug info) and -O3. Generating debug info for ~300k lines is the
    # primary cause of the ~14 min compile time; stripping -g and using -g0
    # brings it down to a few minutes. We override -O with -O1.
    PY_CFLAGS=$(strip_debug_and_opt_flags "$PY_CFLAGS_RAW")
    echo "  Original PY_CFLAGS: $PY_CFLAGS_RAW"
    echo "  Stripped PY_CFLAGS: $PY_CFLAGS"

    # Build the output object name matching what setuptools generates.
    PY_TAG=$($PYTHON -c "import sys; print(f'cpython-{sys.version_info.major}{sys.version_info.minor}')")
    PLATFORM_TAG="linux-${ARCH}"
    OBJ_DIR="$PREBUILT_DIR/${PLATFORM_TAG}-${PY_TAG}"
    OBJ_FILE="$OBJ_DIR/oreanalytics_wrap.o"

    mkdir -p "$OBJ_DIR"

    echo "Compiling oreanalytics_wrap.cpp for $PYVER ($PY_TAG) ..."

    # Compile sequentially (one at a time to avoid OOM).
    # -g0:  explicitly disable debug info (overrides any residual -g).
    # -O1:  light optimization — sufficient for SWIG glue code.
    $CXX -pthread $PY_CFLAGS $PY_CCSHARED -DNDEBUG \
        -I"$PY_INCLUDE" -I"$PY_PLATINCLUDE" \
        -c "$WRAP_SRC" \
        -o "$OBJ_FILE" \
        -w -Wno-unused -std=c++20 -g0 -O1

    echo "  ? $PYVER compiled successfully ($(stat -c%s "$OBJ_FILE" 2>/dev/null || stat -f%z "$OBJ_FILE") bytes)"
done

echo "All pre-compilations complete. Objects in $PREBUILT_DIR:"
find "$PREBUILT_DIR" -name '*.o' -ls
