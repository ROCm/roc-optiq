## Building Perfetto Trace Processor Libraries

The prebuilt libraries are committed to the repository and do not need to be
rebuilt unless updating the Perfetto version. When a rebuild is needed, use
either the CMake targets or the manual steps below.

### Via CMake

```cmake
# Windows libraries only
cmake --build . --target build_perfetto_libs_windows

# Linux libraries only (via Docker — can run on Windows)
cmake --build . --target build_perfetto_libs_linux

# Both platforms
cmake --build . --target build_perfetto_libs
```

> **Note:** The Linux target requires Docker to be installed and running.
> The Windows target must be run on Windows with GN, Ninja, and Python on PATH.

---

### Windows — Manual Steps (PowerShell)

Clone into a short path to avoid the Windows 260-character path limit:

```powershell
cd C:\pf
git clone https://github.com/google/perfetto
cd perfetto
git checkout v47.0

python tools\install-build-deps

# Write args files directly to avoid shell quoting issues
gn gen out\release --root=.
"is_debug=false`nextra_cflags=`"/MD`"" | Set-Content out\release\args.gn
gn gen out\release --root=.

gn gen out\debug --root=.
"is_debug=false`nextra_cflags=`"/MDd`"" | Set-Content out\debug\args.gn
gn gen out\debug --root=.

ninja -C out\release src/trace_processor:trace_processor
ninja -C out\debug   src/trace_processor:trace_processor

# Copy artifacts to project — replace ${PROJECT} with your project root
$PROJECT = "C:\path\to\rocprofiler-visualizer"
$DEST    = "$PROJECT\thirdparty\perfetto\trace_processor"

Copy-Item out\release\trace_processor.lib "$DEST\lib\windows\release\"
Copy-Item out\debug\trace_processor.lib   "$DEST\lib\windows\debug\"

# Copy headers and generated files (only needed once)
Copy-Item -Recurse out\release\gen\build_config "$DEST\gen\build_config" -Force
Copy-Item -Recurse include\perfetto             "$DEST\include\perfetto"  -Force
```

> **Note:** `gn` and `ninja` refer to the binaries downloaded by
> `install-build-deps` into `third_party/gn/` and `third_party/ninja/`.
> Add them to PATH or use their full paths:
> `third_party/gn/gn.exe` and `third_party/ninja/ninja.exe`.

---

### Linux — Manual Steps (via Docker, run from Windows PowerShell or Linux shell)

The Linux library is built inside a Docker container to ensure a consistent
build environment regardless of host OS.

```powershell
# From your project root
$PROJECT = "C:\path\to\rocprofiler-visualizer"
$DEST    = "$PROJECT\thirdparty\perfetto\trace_processor"

# Copy Dockerfile to a working directory
$WORKDIR = "C:\pf\docker_build"
New-Item -ItemType Directory -Force $WORKDIR
Copy-Item "$DEST\build\Dockerfile.perfetto" "$WORKDIR\"

# Build the Docker image
cd $WORKDIR
docker build -f Dockerfile.perfetto -t perfetto-builder .

# Create output directory and run the container
$OUTPUT = "$DEST\lib\linux\release"
New-Item -ItemType Directory -Force $OUTPUT

docker run --rm `
    -v "${OUTPUT}:/output/lib" `
    -v "${DEST}\include:/output/include" `
    -v "${DEST}\gen:/output/gen" `
    perfetto-builder
```

On Linux the steps are identical but use native shell syntax:

```bash
PROJECT=/path/to/rocprofiler-visualizer
DEST=$PROJECT/thirdparty/perfetto/trace_processor

mkdir -p $DEST/lib/linux/release

docker build \
    -f $DEST/build/Dockerfile.perfetto \
    -t perfetto-builder \
    $DEST/build

docker run --rm \
    -v $DEST/lib/linux/release:/output/lib \
    -v $DEST/include:/output/include \
    -v $DEST/gen:/output/gen \
    perfetto-builder
```