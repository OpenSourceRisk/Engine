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

### Why pre-compilation was still taking ~12-14 minutes per version

After moving compilation to `Dockerfile-Wheels-ORE` and stripping `-g` from Python's
CFLAGS, each Python version still took ~12 minutes. The root cause is that **any
optimization level above `-O0` is expensive on a ~300k line single translation unit**.

At `-O1`, the compiler still performs:
- Inlining of trivial functions (the SWIG wrapper is full of these)
- Dead code elimination across ~300k lines
- Register allocation and instruction scheduling
- Constant propagation through deeply-nested template instantiations

For SWIG glue code (pure call-forwarding wrappers), none of this optimization provides
measurable runtime benefit — every wrapper function just marshals Python arguments and
calls into pre-compiled ORE library functions. The actual computation happens in the
already-optimized ORE libraries.

Python's sysconfig CFLAGS also added unnecessary overhead: `-fwrapv` (signed overflow
wrapping), `-Wsign-compare`, `-Wall` etc. — all unnecessary for generated code.

## Solution: Pre-compile in `Dockerfile-Wheels-ORE` with `-O0`

We move SWIG generation and wrapper compilation into `Dockerfile-Wheels-ORE`, running
them **immediately after the ORE C++ library build**, with minimal compiler flags:

1. **`-O0`** — no optimization (SWIG glue code doesn't benefit from it)
2. **`-g0`** — no debug info (not useful in shipped wheel binaries)
3. **No Python sysconfig CFLAGS** — only `-fPIC`, `-DNDEBUG`, `-std=c++20`, `-w`
4. **ccache** enabled for faster rebuilds when wrapper source hasn't changed

The pre-built `.o` files and generated `.cpp` are baked into the `wheels_ore2` image.
When cibuildwheel runs, `CIBW_BEFORE_ALL` simply copies them into the project directory,
and `setup.py build_ext` performs only the fast (~30s) link step.

## Changes

### `ore/ORE-SWIG/Wheels-gitlab/precompile.sh`

| Change | Why |
|--------|-----|
| Dropped to `-O0` from `-O1` | Eliminates ~8 min of optimization on ~300k lines of call-forwarding glue code with no runtime impact |
| Removed Python sysconfig CFLAGS entirely | Those flags (`-fwrapv`, `-Wall`, etc.) are for real extension code, not SWIG wrappers |
| Only passes minimal flags | `-fPIC` (from `PY_CCSHARED`), `-DNDEBUG`, `-std=c++20`, `-g0`, `-w` |
| Added `-g0` explicitly | Prevents any debug info generation |
| Uses ccache if available | Wraps compiler with `ccache` for faster rebuilds |
| Added timing output | Prints compile duration and object file size for diagnostics |

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
??? python3 setup.py wrap               # Generate oreanalytics_wrap.cpp (SWIG)
??? precompile.sh cp310-cp310 cp312-cp312
    ??? ccache clang++ -O0 -g0 ... ? .prebuilt/linux-x86_64-cpython-310/oreanalytics_wrap.o
    ??? ccache clang++ -O0 -g0 ... ? .prebuilt/linux-x86_64-cpython-312/oreanalytics_wrap.o
    (sequential, ~4 min each expected)
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

## Why `-O0` is safe for SWIG wrapper code

SWIG-generated `oreanalytics_wrap.cpp` contains exclusively:
- Python/C API boilerplate (type registration, module init)
- Argument marshaling (converting Python objects ? C++ types)
- Direct forwarding calls to ORE library functions

No computation happens in the wrapper — it's all in the pre-compiled (`-O3`) ORE
libraries. The wrapper overhead is dominated by Python interpreter overhead (dict
lookups, reference counting), not by the wrapper's own machine code quality.

Benchmark expectation: The per-call overhead difference between `-O0` and `-O3` for
a typical SWIG wrapper function (unpack args ? call library ? pack result) is in the
single-digit nanoseconds range, completely dwarfed by Python's own overhead.

## Expected speedup

| Area | Before (original) | After (with `-O0`) |
|------|-------------------|-------------------|
| SWIG generation | In cibuildwheel (redundant) | In Dockerfile-Wheels-ORE (1×) |
| Wrapper compilation | ~25 min × 2 (cold, `-O3`, cibuildwheel) | ~4 min × 2 (`-O0`, ORE build stage) |
| Per-version wheel build | ~25 min each (compile + link) | ~30s each (link only) |
| **Total wrapper compile time** | **~50 min** | **~8 min** |
| **Net savings** | — | **~42 min** |

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
