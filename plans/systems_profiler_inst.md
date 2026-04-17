Here’s a concise overview of the ROCm Systems Profiler (rocprof-sys) command-line tools, what each one does, and simple usage examples. The exact set of subcommands and options can vary slightly by ROCm version, so it’s a good idea to run each tool with --help on your system.

Core commands you’ll use most often

- rocprof-sys-instrument
  - Purpose: Instruments a native executable so it can be profiled by the systems profiler. Produces an .inst binary.
  - Common use cases:
    - General tracing of HIP/HSA/CPU activity.
    - Optional “sampling-only” instrumentation for on-demand call-stack sampling.
  - Key notes:
    - Pass your original binary after --.
    - “Sampling-only” mode is enabled with -M sampling (and then toggle sampling at runtime with ROCPROFSYS_USE_SAMPLING=ON).
  - Examples:
    ```bash
    # Instrument an app for tracing
    rocprof-sys-instrument -o ./my_app.inst -- ./my_app

    # Instrument for sampling-only control
    rocprof-sys-instrument -M sampling -o ./my_app.inst -- ./my_app
    ```

- rocprof-sys-run
  - Purpose: Runs an instrumented binary and records a system-wide Perfetto trace.
  - Common use cases: End-to-end timeline tracing for kernels, memcpy, HIP/HSA API regions, and CPU-side activity.
  - Examples:
    ```bash
    # Run the instrumented app and capture a trace
    rocprof-sys-run -- ./my_app.inst [args]

    # Many builds also accept a --trace flag for clarity
    rocprof-sys-run --trace -- ./my_app.inst [args]
    ```
  - Tips:
    - The tool prints where it writes the Perfetto trace; open it at https://ui.perfetto.dev/.
    - To add a timestamp to output names:
      ```bash
      export ROCPROFSYS_TIME_OUTPUT=ON
      rocprof-sys-run -- ./my_app.inst
      ```

- rocprof-sys-sample
  - Purpose: Call-stack sampling without any binary instrumentation. Good for quickly seeing hot CPU call paths (and, depending on configuration, associated GPU regions).
  - Examples:
    ```bash
    # Run your app with sampling enabled (no .inst required)
    rocprof-sys-sample -- ./my_app [args]
    ```

Helpful companion commands

- rocprof-sys-avail
  - Purpose: Lists available data collection components, hardware counters, and configuration keys for your system/ROCm build.
  - Example:
    ```bash
    rocprof-sys-avail
    ```

- rocprof-sys-python
  - Purpose: Convenience wrapper to profile Python scripts (ensures the proper Python environment variables are set so tracing and symbols work as expected).
  - Examples:
    ```bash
    # Sampling-only without instrumentation
    rocprof-sys-sample -- python3 your_script.py

    # Instrument-and-run flow for Python (instruments the interpreter)
    rocprof-sys-instrument -o ./python.inst -- $(command -v python3)
    rocprof-sys-run -- ./python.inst your_script.py [args]
    ```

On-demand capture controls (availability can vary by version)

- rocprof-sys-start and rocprof-sys-stop
  - Purpose: Start and stop capture windows for a long-lived instrumented process (useful when you want to trace only a specific phase).
  - Notes:
    - These typically work with an instrumented process that is launched with the systems profiler runtime and control enabled. Exact flags and control methods can vary; check --help on your version.
  - Examples (pattern):
    ```bash
    # In one shell, launch the instrumented app (idle or warm-up)
    rocprof-sys-run -- ./my_app.inst [args]

    # In another shell, trigger capture start/stop at the right time
    rocprof-sys-start
    # ... let your workload run the interesting phase ...
    rocprof-sys-stop
    ```

Sampling with an instrumented binary

If you want to control call-stack sampling for only part of a run (instead of sampling the whole time):

- Instrument for sampling-only:
  ```bash
  rocprof-sys-instrument -M sampling -o ./my_app.inst -- ./my_app
  ```

- Enable sampling at runtime (e.g., for a segment of execution):
  ```bash
  export ROCPROFSYS_USE_SAMPLING=ON
  rocprof-sys-run -- ./my_app.inst [args]
  ```

End-to-end quick workflows

- Timeline trace (recommended first run):
  ```bash
  rocprof-sys-instrument -o ./my_app.inst -- ./my_app
  rocprof-sys-run -- ./my_app.inst [args]
  # Open the generated Perfetto trace in https://ui.perfetto.dev/
  ```

- Fast CPU call-stack sampling (no instrumentation):
  ```bash
  rocprof-sys-sample -- ./my_app [args]
  ```

- Python script profiling:
  ```bash
  # Quick sampling:
  rocprof-sys-sample -- python3 your_script.py

  # Full instrumentation-and-run:
  rocprof-sys-instrument -o ./python.inst -- $(command -v python3)
  rocprof-sys-run -- ./python.inst your_script.py
  ```

Practical tips

- Command availability differs slightly across ROCm releases (e.g., 6.3.x vs 6.4.x+). Use --help to see the exact flags on your system:
  ```bash
  rocprof-sys-instrument --help
  rocprof-sys-run --help
  rocprof-sys-sample --help
  rocprof-sys-avail --help
  rocprof-sys-start --help
  rocprof-sys-stop --help
  ```
- If you’re only getting started and want the simplest timeline trace without any instrumentation, the classic tool also supports a system trace:
  ```bash
  rocprof --sys-trace -- ./my_app [args]
  ```

If you share your ROCm version and how you launch your workload (native binary, MPI, or Python), I can tailor exact commands and flags that work best for your setup.