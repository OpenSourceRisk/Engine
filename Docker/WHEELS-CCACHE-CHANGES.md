# Wheels Docker Build — ccache & Performance Changes

## Problem

The `Dockerfile-Wheels-CIBW` build was slow because:

1. **No ccache** was configured inside the cibuildwheel-spawned containers, so the
   compilation of `oreanalytics_wrap.cpp` (a large SWIG-generated C++ file) happened
   from scratch for every Python version (cp310, cp312).
2. **SWIG generation ran redundantly** — `before_all_linux.sh` was set as
   `CIBW_BEFORE_BUILD`, which runs before *each* wheel, not once per platform.
3. **`Dockerfile-Wheels-ORE`** had ccache configured, but the `PATH` pointed to
   `/usr/lib/ccache` while `dnf`-based images place ccache symlinks in
   `/usr/lib64/ccache`, so ccache was not intercepting compiler calls via PATH.
4. **Both Dockerfiles shared the same BuildKit cache mount** (`--mount=type=cache,target=/ccache/`),
   but their CMake flags differed (e.g. `-fPIC`, different `-D` defines), so cached
   objects from one build never produced hits in the other.

## Changes

### `ore/Docker/Dockerfile-Wheels-ORE`

| Change | Why |
|--------|-----|
| `PATH` updated to `/usr/lib64/ccache:/usr/lib/ccache:$PATH` | ccache symlinks are in `/usr/lib64/ccache` on `dnf`-based (RHEL/Fedora/manylinux) images |
| Cache mount changed to `--mount=type=cache,id=ccache-wheels,target=/ccache/` | Gives the wheels build its own dedicated BuildKit cache volume |

### `ore/Docker/Dockerfile-ORE`

| Change | Why |
|--------|-----|
| Cache mount changed to `--mount=type=cache,id=ccache-ore,target=/ccache/` | Explicitly names this cache volume so it stays separate from the wheels cache |

### `ore/Docker/Dockerfile-Wheels-CIBW`

| Change | Why |
|--------|-----|
| Added `# syntax = docker/dockerfile:1.2` | Enables BuildKit features |
| Moved `before_all_linux.sh` from `CIBW_BEFORE_BUILD` to `CIBW_BEFORE_ALL` | SWIG wrapping (`setup.py wrap`) is Python-version-independent — now runs once per platform instead of once per wheel |
| Added `CIBW_BEFORE_BUILD="pip install setuptools"` | Ensures setuptools is available in each per-Python-version venv for `setup.py build_ext` |
| Added `CIBW_ENVIRONMENT` with ccache config | Routes compiler calls through ccache inside cibuildwheel containers (see details below) |

#### `CIBW_ENVIRONMENT` details

```
PATH=/usr/lib64/ccache:/usr/lib/ccache:$PATH
CCACHE_DIR=/project/.ccache
CCACHE_MAXSIZE=5G
```

- **`PATH`** — Prepends ccache symlink directories so compiler invocations from
  `setup.py build_ext` are intercepted by ccache. The `wheels_ore2` image already
  has ccache installed via `dnf`.
- **`CCACHE_DIR=/project/.ccache`** — Uses the project mount directory (`/project`),
  which persists across Python versions within a single cibuildwheel run. This means
  cp312 gets cache hits from cp310's compilation of the same `oreanalytics_wrap.cpp`.
- **`CCACHE_MAXSIZE=5G`** — Reasonable limit for the SWIG wrapper cache.

### `ore/ORE-SWIG/Wheels-gitlab/before_all_linux.sh`

| Change | Why |
|--------|-----|
| Removed `setuptools` from `pip3 install` | Now handled by `CIBW_BEFORE_ALL` and `CIBW_BEFORE_BUILD` in the Dockerfile |

## How ccache works in the pipeline

### `Dockerfile-ORE` (the main ORE build)

- Debian-based image, ccache symlinks at `/usr/lib/ccache`.
- CMake is configured with `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`.
- BuildKit cache mount `id=ccache-ore` persists the ccache directory across builds.
- On repeat builds with identical source, ccache produces direct hits.

### `Dockerfile-Wheels-ORE` (the wheels ORE library build)

- `dnf`-based manylinux image, ccache symlinks at `/usr/lib64/ccache`.
- CMake is configured with `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache`.
- BuildKit cache mount `id=ccache-wheels` gives this build its own cache volume,
  separate from the main ORE build (different flags like `-fPIC`).
- On repeat builds with identical source and flags, ccache produces direct hits.

### `Dockerfile-Wheels-CIBW` (cibuildwheel — the Python wheel packaging step)

- Runs inside the `ore-build-dependencies` image.
- `cibuildwheel` launches its own Docker containers from the `wheels_ore2` image.
- Inside those containers, `setup.py build_ext` compiles `oreanalytics_wrap.cpp`.
- `CIBW_ENVIRONMENT` sets up ccache in those containers:
  - Compiler calls go through ccache symlinks in PATH.
  - Cache stored at `/project/.ccache` (shared across Python versions).
- **First Python version** (e.g. cp310): cache miss, full compilation.
- **Second Python version** (e.g. cp312): cache hit on the bulk of compilation
  (same `oreanalytics_wrap.cpp`, same ORE headers, same compiler flags).

## Expected speedup

| Area | Before | After |
|------|--------|-------|
| SWIG generation | Runs once per Python version (2×) | Runs once per platform (1×) |
| `oreanalytics_wrap.cpp` compilation (cp310) | Full compile | Full compile (cold cache) |
| `oreanalytics_wrap.cpp` compilation (cp312) | Full compile | ccache hit (fast) |
| Subsequent CI runs (same source) | Full compile | ccache hits in `Dockerfile-Wheels-ORE` via BuildKit cache mount |

## Cache isolation

| Dockerfile | Cache mount `id` | Why separate |
|------------|-----------------|--------------|
| `Dockerfile-ORE` | `ccache-ore` | Flags include `-DORE_BUILD_SWIG=ON`, no `-fPIC`, `-DORE_ENABLE_OPENCL=ON` |
| `Dockerfile-Wheels-ORE` | `ccache-wheels` | Flags include `-fPIC`, `-DORE_ENABLE_OPENCL=OFF`, tests disabled |
| `Dockerfile-Wheels-CIBW` | N/A (ephemeral `/project/.ccache`) | Only lives within a single cibuildwheel run; shares across Python versions |
