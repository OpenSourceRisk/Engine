# Wheels Docker Build — Pre-compilation & Performance Changes

## Problem

The `Dockerfile-Wheels-CIBW` build was slow because:

1. **`oreanalytics_wrap.cpp` was compiled inside cibuildwheel** for each Python version,
   cold (no page cache), with `-O3`, each taking ~25 minutes. Total: ~50 minutes.
2. **SWIG generation ran redundantly** — `before_all_linux.sh` was set as
   `CIBW_BEFORE_BUILD`, which runs before *each* wheel, not once per platform.
3. **`Dockerfile-Wheels-ORE`** had ccache configured, but the `PATH` pointed to
   `/usr/lib/ccache` while `dnf`-based images place ccache symlinks in
   `/usr/lib64/ccache`, so ccache was not intercepting compiler calls via PATH.
4. **Both Dockerfiles shared the same BuildKit cache mount** (`--mount=type=cache,target=/ccache/`),
   but their CMake flags differed (e.g. `-fPIC`, different `-D` defines), so cached
   objects from one build never produced hits in the other.

### Why ccache doesn't help inside cibuildwheel

ccache hashes the preprocessed output of a source file. Because `oreanalytics_wrap.cpp`
includes `<Python.h>`, and `Python.h` content differs between Python 3.10 and 3.12,
the preprocessed output is different for each version — resulting in guaranteed cache
misses. ccache cannot speed up cross-Python-version builds for SWIG wrappers.

### Why compiling in cibuildwheel is slow

Compiling `oreanalytics_wrap.cpp` (~300k lines) is expensive. When done inside the
cibuildwheel container, the ORE/Boost headers are cold (not in page cache) because
the C++ library build happened in a different Docker image (`Dockerfile-Wheels-ORE`).
Parallel compilation of multiple Python versions causes OOM kills (ARM) or swap
thrashing until timeout (x86_64) on memory-constrained CI runners.

### Why pre-compilation was still taking ~14 minutes per version

After moving compilation to `Dockerfile-Wheels-ORE`, each Python version still took
~14 minutes. The root cause was Python's `sysconfig.get_config_var('CFLAGS')` from
the manylinux images, which includes:

```
-Wno-unused-result -Wsign-compare -DNDEBUG -g -fwrapv -O3 -Wall -fstack-protector-strong -specs=...
```

The **`-g` flag** (generate full DWARF debug info) was the primary culprit. For a ~300k
line file, DWARF generation is extremely expensive in both time and memory, easily
adding 10+ minutes. The `-O1` at the end of `precompile.sh` overrode the `-O3`, but
`-g` persisted.

In contrast, when `Dockerfile-ORE` compiles the SWIG wrapper via CMake
(`-DORE_BUILD_SWIG=ON`), CMake's `Release` build type uses `-O3 -DNDEBUG` with
**no `-g`**, explaining why it completes in just a few minutes.

## Solution: Pre-compile in `Dockerfile-Wheels-ORE`

We move SWIG generation and wrapper compilation into `Dockerfile-Wheels-ORE`, running
them **immediately after the ORE C++ library build**. At this point:

1. **All ORE/Boost headers are warm** in the OS page cache from the library build
2. **16 cores and full RAM** are available (Kaniko build, not Docker-in-Docker)
3. **clang++** is already installed (used for the library build, faster than GCC)
4. **`-O1 -g0`** is used — light optimization, no debug info

The pre-built `.o` files and generated `.cpp` are baked into the `wheels_ore2` image.
When cibuildwheel runs, `CIBW_BEFORE_ALL` simply copies them into the project directory,
and `setup.py build_ext` performs only the fast (~30s) link step.

## Changes

### `ore/ORE-SWIG/Wheels-gitlab/precompile.sh`

| Change | Why |
|--------|-----|
| Added `strip_debug_and_opt_flags()` function | Strips `-g*`, `-O*`, `-specs=*`, `-fstack-protector*`, `-fcf-protection*` from Python's CFLAGS |
| Added `-g0` to compiler invocation | Explicitly disables debug info generation; this is the primary fix for the ~14 min compile time |
| `-O*` stripped before appending `-O1` | Avoids relying on "last flag wins" behaviour |
| Uses ccache if available | Wraps compiler with `ccache` for faster rebuilds when source hasn't changed |
| Prints original vs stripped CFLAGS | Diagnostic output to verify flag stripping |
| Prints object file size on completion | Confirms debug info is not bloating the `.o` |

### `ore/Docker/Dockerfile-Wheels-ORE`

| Change | Why |
|--------|-----|
| `PATH` updated to `/usr/lib64/ccache:/usr/lib/ccache:$PATH` | ccache symlinks are in `/usr/lib64/ccache` on `dnf`-based (RHEL/Fedora/manylinux) images |
| Cache mount changed to `--mount=type=cache,id=ccache-wheels,target=/ccache/` | Gives the wheels build its own dedicated BuildKit cache volume |
| Added `dnf -y install swig` | Needed to generate `oreanalytics_wrap.cpp` |
| Added COPY of ORE-SWIG source directories | Needed for SWIG generation and `precompile.sh` |
| Pre-compile RUN step mounts ccache cache | Enables ccache for wrapper compilation across rebuilds |
| Pre-built artifacts stored at `/ore-swig-prebuilt/` | Baked into the image for use by cibuildwheel |

### `ore/Docker/Dockerfile-ORE`

| Change | Why |
|--------|-----|
| Cache mount changed to `--mount=type=cache,id=ccache-ore,target=/ccache/` | Explicitly names this cache volume so it stays separate from the wheels cache |

### `ore/Docker/Dockerfile-Wheels-CIBW`

| Change | Why |
|--------|-----|
| Added `ARG DOCKER_REPO` and `ARG ORE_BUILD_VERSION` before `FROM` | Docker requires ARGs used in `FROM` to be declared before it |
| `CIBW_BEFORE_ALL` now copies pre-built artifacts instead of compiling | `.cpp` and `.o` files are already in the image at `/ore-swig-prebuilt/` |
| Added `CIBW_BEFORE_BUILD="pip install setuptools"` | Ensures setuptools is available in each per-Python-version venv |
| `CIBW_ENVIRONMENT` sets `ORE_PREBUILT_DIR` | Tells `setup.py` where to find the pre-built `.o` files |
| Removed ccache from `CIBW_ENVIRONMENT` | ccache cannot produce cross-Python-version hits (different `Python.h` ? different hash) |

### `ore/ORE-SWIG/setup.py`

| Change | Why |
|--------|-----|
| Added `build_extension()` override to `my_build_ext` | Checks for pre-built `.o` in `ORE_PREBUILT_DIR`, copies it to the build temp directory, and skips compilation (link-only) |
| Added `import shutil, platform` | Needed for the pre-built object file handling |
| No-op when `ORE_PREBUILT_DIR` is not set | Normal (non-wheel) builds are completely unaffected |

## How it works

### Build flow

```
Dockerfile-Wheels-ORE (Kaniko, 16 cores, full RAM)
??? cmake --build . -- -j 16 install   # Build ORE C++ libraries
?                                       # (headers now warm in page cache)
??? python3 setup.py wrap               # Generate oreanalytics_wrap.cpp (SWIG)
??? precompile.sh cp310-cp310 cp312-cp312
    ??? ccache clang++ ... -g0 -O1 ... ? .prebuilt/linux-x86_64-cpython-310/oreanalytics_wrap.o
    ??? ccache clang++ ... -g0 -O1 ... ? .prebuilt/linux-x86_64-cpython-312/oreanalytics_wrap.o
    (sequential, no debug info, warm page cache, ~2-3 min each)
    ? baked into wheels_ore2 image at /ore-swig-prebuilt/
```

### cibuildwheel flow

```
Dockerfile-Wheels-CIBW
??? cibuildwheel (uses wheels_ore2 image)
    ??? CIBW_BEFORE_ALL
    ?   ??? cp /ore-swig-prebuilt/* ? /project/ORE-SWIG/  # copy pre-built artifacts
    ??? Per-version (e.g. cp312)
        ??? pip install setuptools                          # CIBW_BEFORE_BUILD
        ??? setup.py build_ext
            ??? my_build_ext.build_extension()
                ??? Finds .prebuilt/linux-x86_64-cpython-312/oreanalytics_wrap.o
                ??? Copies to build/temp.linux-x86_64-cpython-312/
                ??? Runs link only (~30 seconds)
```

## Root cause analysis: `-g` flag impact

| Scenario | Flags | Compile time | `.o` size |
|----------|-------|-------------|-----------|
| CMake Release (Dockerfile-ORE) | `-O3 -DNDEBUG` (no `-g`) | ~3-5 min | ~50-80 MB |
| precompile.sh (before fix) | Python CFLAGS with `-g` + `-O1` | ~14 min | ~500+ MB |
| precompile.sh (after fix) | Python CFLAGS stripped, `-g0 -O1` | ~2-4 min | ~50-80 MB |

DWARF debug info for a ~300k line SWIG wrapper (which `#include`s all of ORE/Boost/QuantLib)
records type information, line mappings, and variable locations for every inlined template
instantiation. This easily exceeds the size of the code itself and dominates both compile
time and disk I/O.

## Expected speedup

| Area | Before (original) | After (with `-g0` fix) |
|------|-------------------|----------------------|
| SWIG generation | In cibuildwheel (redundant) | In Dockerfile-Wheels-ORE (1×) |
| Wrapper compilation | ~25 min × 2 (cold, `-O3 -g`, cibuildwheel) | ~3 min × 2 (warm, `-O1 -g0`, ORE build stage) |
| Per-version wheel build | ~25 min each (compile + link) | ~30s each (link only) |
| **Total wrapper compile time** | **~50 min** | **~6 min** |
| **Net savings** | — | **~44 min** |

## ccache in Dockerfile-ORE and Dockerfile-Wheels-ORE

ccache remains useful for the C++ library builds (`Dockerfile-ORE` and
`Dockerfile-Wheels-ORE`) where it caches across CI runs via BuildKit cache mounts.
It is now also used by `precompile.sh` for wrapper compilation (helps on rebuilds
when only non-SWIG code changes). It was removed from the cibuildwheel step where
it provided no benefit.

### Cache isolation

| Dockerfile | Cache mount `id` | Why separate |
|------------|-----------------|--------------|
| `Dockerfile-ORE` | `ccache-ore` | Flags include `-DORE_BUILD_SWIG=ON`, no `-fPIC`, `-DORE_ENABLE_OPENCL=ON` |
| `Dockerfile-Wheels-ORE` | `ccache-wheels` | Flags include `-fPIC`, `-DORE_ENABLE_OPENCL=OFF`, tests disabled; also used by precompile.sh |
| `Dockerfile-Wheels-CIBW` | N/A (pre-compiled) | Uses pre-compiled objects from Dockerfile-Wheels-ORE |

## Bug Fix: `build-wheels-cibw` missing `check-out` dependency

The `build-wheels-cibw` CI job was failing with:

```
Error: error resolving dockerfile path: please provide a valid path to a Dockerfile within the build context with --dockerfile
```

### Root Cause

The job had `GIT_STRATEGY: none` (no repo checkout) and relied on artifacts from
`needs` jobs. However, `check-out` was not listed in `needs`, so the `ore/` directory
(containing `ore/Docker/Dockerfile-Wheels-CIBW`) was never downloaded. Kaniko could
not find the Dockerfile.

### Fixes

1. **`ci/build_wheels.yml`**: Added `check-out` to the `needs` list of
   `build-wheels-cibw` so that the `ore/` artifacts are available.
2. **`ore/Docker/Dockerfile-Wheels-CIBW`**: Added `ARG DOCKER_REPO` and
   `ARG ORE_BUILD_VERSION` before the `FROM` line, matching the pattern in
   `Dockerfile-ORE`. Docker requires ARGs used in `FROM` to be declared before it`.
