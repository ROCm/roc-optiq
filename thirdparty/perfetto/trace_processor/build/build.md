## Building Perfetto

### Windows (PowerShell)

```powershell
git clone https://github.com/google/perfetto
git checkout v47.0
cd perfetto
python tools\install-build-deps
third_party/gn/gn gen out/release --args='is_debug=false extra_cflags=\"/MD\"'
third_party/gn/gn gen out/debug --args='is_debug=false extra_cflags=\"/MDd\"'
third_party/ninja/ninja -C out\debug src/trace_processor:trace_processor
third_party/ninja/ninja -C out\release src/trace_processor:trace_processor
copy out/debug/trace_processor.lib ${PROJECT}/thirdparty/perfetto/trace_processor/lib/windows/debug
copy out/release/trace_processor.lib ${PROJECT}/thirdparty/perfetto/trace_processor/lib/windows/release
copy out/release/gen ${PROJECT}/thirdparty/perfetto/trace_processor/
copy include ${PROJECT}/thirdparty/perfetto/trace_processor/
```

### Linux (Windows PowerShell)

```powershell
git clone https://github.com/google/perfetto
git checkout v47.0
cd perfetto
# Copy Dockerfile.perfetto to perfetto root directory
docker build -f Dockerfile.perfetto -t perfetto-builder .
docker run --rm -v "${PWD}\thirdparty\perfetto_prebuilt\lib\linux_x64:/output" perfetto-builder
cp thirdparty/perfetto_prebuilt/lib/linux_x64/lib/libtrace_processor.a
```
