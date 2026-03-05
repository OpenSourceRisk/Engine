#!/bin/bash
# Pre-compile oreanalytics_wrap.cpp for all target Python versions in parallel.
#
# This script runs during CIBW_BEFORE_ALL (once per platform) and compiles
# the SWIG-generated wrapper for each Python version simultaneously.
# When setup.py build_ext later runs for each version, it finds the pre-built
# .o file and skips the expensive (~25 min) compilation, only performing the
# fast (~30s) link step.
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
PIDS=()
PYVERS=()

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
    # CFLAGS includes optimization, warning, and debug flags.
    PY_CFLAGS=$($PYTHON -c "import sysconfig; print(sysconfig.get_config_var('CFLAGS') or '')")
    # CCSHARED provides -fPIC on Linux.
    PY_CCSHARED=$($PYTHON -c "import sysconfig; print(sysconfig.get_config_var('CCSHARED') or '')")

    # Build the output object name matching what setuptools generates.
    PY_TAG=$($PYTHON -c "import sys; print(f'cpython-{sys.version_info.major}{sys.version_info.minor}')")
    PLATFORM_TAG="linux-${ARCH}"
    OBJ_DIR="$PREBUILT_DIR/${PLATFORM_TAG}-${PY_TAG}"
    OBJ_FILE="$OBJ_DIR/oreanalytics_wrap.o"

    mkdir -p "$OBJ_DIR"

    echo "Compiling oreanalytics_wrap.cpp for $PYVER ($PY_TAG) ..."

    # Compile in background for parallelism.
    # Flags match what setuptools/distutils would generate:
    #   - PY_CFLAGS: from sysconfig CFLAGS (includes -O3 -g -Wall etc.)
    #   - PY_CCSHARED: -fPIC
    #   - -DNDEBUG: defined by setup.py for non-debug builds
    #   - -I for Python headers
    #   - -w -Wno-unused -std=c++20: from oreanalytics-config --cflags + setup.py
    (
        g++ -pthread $PY_CFLAGS $PY_CCSHARED -DNDEBUG \
            -I"$PY_INCLUDE" -I"$PY_PLATINCLUDE" \
            -c "$WRAP_SRC" \
            -o "$OBJ_FILE" \
            -w -Wno-unused -std=c++20 \
        && echo "  ? $PYVER compiled successfully" \
        || { echo "  ? $PYVER compilation FAILED"; exit 1; }
    ) &
    PIDS+=($!)
    PYVERS+=("$PYVER")
done

# Wait for all compilations to finish
FAILED=0
for i in "${!PIDS[@]}"; do
    if ! wait "${PIDS[$i]}"; then
        echo "ERROR: Compilation failed for ${PYVERS[$i]}"
        FAILED=1
    fi
done

if [ "$FAILED" -ne 0 ]; then
    echo "ERROR: One or more pre-compilations failed."
    exit 1
fi

echo "All pre-compilations complete. Objects in $PREBUILT_DIR:"
find "$PREBUILT_DIR" -name '*.o' -ls
