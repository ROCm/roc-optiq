"""The terminal chapters of the AMD ROCm Systems Profiler tutorials.

Each chapter types the commands systems_capture.sh ran and replays their output
with termcast.py, paced to the narration. make_masters.py reads the Say() lines
here for the voice, as it reads them from tutorial_chapters.cpp, where the
chapters that open a run's trace in ROCm Optiq are recorded.

  py -3 systems_chapters.py [--chapter sb] [--narration voice-systems] [--capture systems-capture]
                            [--out raw-systems]
"""

import argparse
import sys
from pathlib import Path

import make_masters as mm
from termcast import Take

HERE = Path(__file__).resolve().parent
SUMMARY_TOP = "\u250c"


# Video 1: getting started

def chapter_what_it_does(t):
    t.Say("AMD ROCm Systems Profiler traces and profiles applications that run on the CPU, or on "
          "the CPU and GPU together.")
    t.run("ls /opt/rocm/bin | grep rocprof-sys", "sa_tools", word="applications")
    t.Say("It comes with ROCm, as a set of command-line tools whose names start with rocprof-sys.")
    t.wait_until_said("tools")
    t.spotlight("rocprof-sys-attach", "rocprof-sys-sample")
    t.Say("rocprof-sys-sample profiles a program as it is, by sampling its call stacks.")
    t.spotlight("rocprof-sys-sample")
    t.Say("rocprof-sys-instrument adds measurements to the functions of a program, and "
          "rocprof-sys-run runs a program instrumented that way.")
    t.spotlight("rocprof-sys-instrument")
    t.wait_until_said("runs")
    t.spotlight("rocprof-sys-run")
    t.Say("rocprof-sys-avail lists what the profiler can collect, and all of its settings.")
    t.spotlight("rocprof-sys-avail")
    t.Say("The others attach to a running process, profile Python scripts, and estimate what an "
          "optimization would gain.")
    for tool, word in (("rocprof-sys-attach", "attach"), ("rocprof-sys-python", "Python"),
                       ("rocprof-sys-causal", "estimate")):
        t.wait_until_said(word)
        t.spotlight(tool)
    t.pause()
    t.clear_spotlight()
    # It prints nothing, so it has no captured output.
    t.Say("If your shell can't find the tools or their libraries, source the setup-env script that "
          "comes with them.")
    t.type("source /opt/rocm/share/rocprofiler-systems/setup-env.sh", word="source")
    t.enter()
    t.Say("To check the install, ask any of the tools for its version.")
    t.run("rocprof-sys-sample --version", "sa_version", word="ask")
    t.pause()


def chapter_first_profile(t):
    t.Say("Our example is transpose, a small HIP program from the examples in the ROCm Systems "
          "Profiler repository on GitHub.")
    t.run("ls", "sb_ls", word="example")
    t.Say("Build it with dash g, so the results can include source file and line information.")
    t.run("hipcc --offload-arch=gfx1100 -O2 -g transpose.cpp -o transpose", "sb_build", word="Build")
    t.Say("On its own, it transposes a large matrix on the GPU, hundreds of times, from two CPU "
          "threads.")
    t.run("./transpose", "sb_plain", word="own")
    t.pause()
    t.Say("The quickest way to profile it is rocprof-sys-sample. It runs your program unchanged, and "
          "records what each thread is doing, many times a second.")
    t.type("rocprof-sys-sample -- ./transpose", word="quickest")
    t.Say("The double dash separates the profiler's options from your program and its arguments.")
    t.wait_until_said("double")
    t.spotlight("rocprof-sys-sample -- ./transpose")
    t.wait_for_voice()
    t.clear_spotlight()
    t.Say("Along with the call stacks, it records the HIP calls, the kernels on the GPU, and the "
          "GPU's activity in the background.")
    t.output("sb_sample")
    t.Say("When it's done, the output summary shows the rocpd database it wrote, in a folder named "
          "after the program, with a subfolder for each run.")
    t.wait_until_said("summary")
    t.spotlight(SUMMARY_TOP, "View with")
    t.Say("Next to the database are JSON files that describe the run.")
    t.clear_spotlight()
    t.run("ls rocprofsys-transpose-output/*/", "sb_list", word="Next")
    t.pause()
    t.Say("ROCm Optiq opens rocpd databases, so let's look at this one there.")


# Video 2: configuring

def chapter_presets(t):
    t.Say("Presets are the easiest way to choose what to collect. Each one bundles the settings for "
          "a common kind of workload.")
    t.run("rocprof-sys-run --list-presets", "sd_presets", word="Presets")
    t.Say("balanced is a good place to start, and trace-gpu focuses on work on the GPU.")
    t.spotlight("balanced -")
    t.wait_until_said("focuses")
    t.spotlight("trace-gpu -")
    t.Say("To see what a preset turns on, explain it.")
    t.clear_spotlight()
    t.run("rocprof-sys-run --explain=balanced", "sd_explain", word="explain")
    t.Say("Each line is a setting. balanced writes a profile and a rocpd database, samples call "
          "stacks fifty times a second, and records GPU metrics.")
    for setting, word in (("ROCPROFSYS_PROFILE", "profile"), ("ROCPROFSYS_USE_ROCPD", "database"),
                          ("ROCPROFSYS_SAMPLING_FREQ", "fifty"), ("ROCPROFSYS_USE_AMD_SMI", "metrics")):
        t.wait_until_said(word)
        t.spotlight(setting)
    t.pause()
    t.clear_spotlight()
    t.Say("rocprof-sys-sample and rocprof-sys-run both take a preset.")
    t.run("rocprof-sys-sample --preset=balanced -- ./transpose", "sd_balanced", word="both")
    t.Say("This time, the summary also lists a wall-clock profile, as text and as JSON.")
    t.wait_until_said("summary")
    t.spotlight(SUMMARY_TOP, "jq .")
    t.pause()
    t.clear_spotlight()


def chapter_saving_settings(t):
    t.Say("Options adjust a preset. Here, the gpu option keeps just the GPU's activity and power, "
          "and the export option saves the result to a file.")
    t.run("rocprof-sys-run --preset=balanced --gpu=busy,power --export-config=gpu-profile.json",
          "sd_export", word="Here")
    t.Say("It's a JSON file, with the GPU metrics you picked.")
    t.run("head -17 gpu-profile.json", "sd_export_cat", word="JSON")
    t.wait_until_said("metrics")
    t.spotlight('"gpu"', '"process_sampling"')
    t.Say("Pass the file to the preset option, to profile with the same settings again.")
    t.clear_spotlight()
    t.run("rocprof-sys-sample --preset=./gpu-profile.json -- ./transpose", "sd_reuse", word="Pass")
    t.pause()
    t.Say("Every option maps to a setting. rocprof-sys-avail describes them all, and dash r filters "
          "the list.")
    t.run("rocprof-sys-avail -bd -r SAMPLING_FREQ", "se_avail", word="describes")
    t.Say("You can set any setting as an environment variable, for a single run.")
    t.type("ROCPROFSYS_SAMPLING_FREQ=200 rocprof-sys-sample -- ./transpose", word="environment")
    t.output("se_env", pause_at="thread 0 will be triggered")
    t.Say("As the program starts, the sampler reports the rate it uses: two hundred times a second.")
    t.wait_until_said("rate")
    t.spotlight("triggered 200.0x")
    t.wait_for_voice()
    t.clear_spotlight()
    t.resume()
    t.Say("Command-line options take priority over environment variables, so here, dash f sets five "
          "hundred.")
    t.type("ROCPROFSYS_SAMPLING_FREQ=200 rocprof-sys-sample -f 500 -- ./transpose", word="priority")
    t.output("se_cli", pause_at="thread 0 will be triggered")
    t.spotlight("triggered 500.0x")
    t.wait_for_voice()
    t.wait(1.5)
    t.clear_spotlight()
    t.resume()
    t.pause()
    t.Say("To keep settings for every run, dash G writes them all to a configuration file.")
    t.run("rocprof-sys-avail -G ~/.rocprof-sys.cfg", "se_generate", word="writes")
    t.Say("Here are a few of them. Edit the values you need, and point the ROCPROFSYS_CONFIG_FILE "
          "variable at the file.")
    t.run('grep -E "^ROCPROFSYS_(TRACE|PROFILE|USE_SAMPLING|SAMPLING_FREQ|USE_ROCPD) " ~/.rocprof-sys.cfg',
          "se_grep", word="few")
    t.pause()


def chapter_output_options(t):
    t.Say("By default, each run writes to a folder named after the program, with a subfolder for "
          "each time it ran.")
    t.run("ls rocprofsys-transpose-output", "sf_ls", word="folder")
    t.Say("Dash o picks the output folder, and a prefix for the file names.")
    t.run("rocprof-sys-sample -o results first-run -- ./transpose", "sf_output", word="picks")
    t.Say("The files land in that folder, in a subfolder for the run, each with your prefix.")
    t.run("ls results/*", "sf_ls2", word="files")
    t.pause()
    t.Say("The output format option chooses the formats: rocpd for ROCm Optiq, proto for Perfetto, "
          "and text or JSON summaries.")
    t.run("rocprof-sys-sample --output-format rocpd proto text -- ./transpose", "sf_formats",
          word="chooses")
    t.Say("Ask for several at once, and the summary lists every file.")
    t.wait_until_said("summary")
    t.spotlight(SUMMARY_TOP, "jq .")
    t.pause()
    t.clear_spotlight()


# Video 3: sampling and instrumentation

def chapter_sampling(t):
    t.Say("Call-stack sampling interrupts each thread at a steady rate, and records where it is. The "
          "program runs at nearly full speed, so it's a good first look at any application.")
    t.Say("Dash f sets the rate, and the profile option adds summaries of where the samples landed.")
    t.run("rocprof-sys-sample -f 200 --profile -o samples -- ./transpose", "sg_profile", word="sets")
    t.Say("The wall-clock summary is a call tree, built from the samples.")
    t.run("cut -c1-131 samples/*/sampling_wall_clock-*.txt | head -16", "sg_tree", word="summary")
    t.Say("Most of the samples fall inside run, the function where each thread does its work.")
    t.wait_until_said("run")
    t.spotlight("|_run(int")
    t.Say("Samples give a statistical picture. They are cheap to take, but they don't see every "
          "call. For that, use instrumentation.")
    t.pause()
    t.clear_spotlight()


def chapter_runtime_instrumentation(t):
    t.Say("Binary instrumentation measures every call to the functions it covers. "
          "rocprof-sys-instrument adds that code as it launches your program.")
    t.type("rocprof-sys-instrument -- ./transpose", word="adds")
    t.output("sh_runtime", pause_at="Filtered objects")
    t.Say("Launched this way, it also covers the libraries the program loads, except very large "
          "ones.")
    t.wait_until_said("except")
    t.spotlight("skipping shared lib 'libclang", "skipping shared lib 'libLLVM")
    t.wait_for_voice()
    t.clear_spotlight()
    t.resume()
    t.Say("That makes it thorough, but slower to start, and heavier to run.")
    t.pause()


def chapter_binary_rewrite(t):
    t.Say("Dash o rewrites the program into a new file instead. First, simulate it, and dump what "
          "it would instrument.")
    t.run("rocprof-sys-instrument --simulate --dump-info -o transpose.inst -- ./transpose",
          "si_simulate", word="simulate")
    t.Say("For transpose, the list of instrumented functions is empty.")
    t.run("cat rocprofsys-transpose.inst-output/instrumentation/instrumented.txt", "si_instrumented",
          word="list")
    t.Say("The available functions show why. By default, only functions with at least 1024 "
          "instructions are instrumented, and main and run are smaller.")
    t.run('grep -E "#Instructions| main | run " rocprofsys-transpose.inst-output/instrumentation/'
          'available.txt | cut -c1-130', "si_available", word="available")
    t.wait_until_said("main")
    t.spotlight("   665   ")
    t.wait_until_said("run")
    t.spotlight("   847   ")
    t.Say("Dash i lowers that limit. The rewritten program starts faster, and runs with less "
          "overhead, than instrumenting at launch.")
    t.clear_spotlight()
    t.run("rocprof-sys-instrument -i 256 -o transpose.inst -- ./transpose", "si_rewrite",
          word="lowers")
    t.spotlight("The instrumented executable image is stored in")
    t.Say("Run it with rocprof-sys-run. The profile option adds a summary of every instrumented "
          "call.")
    t.clear_spotlight()
    t.run("rocprof-sys-run --profile -- ./transpose.inst", "si_run", word="Run")
    t.pause()


def chapter_reading_the_profile(t):
    t.Say("The wall-clock profile lists each function under the one that called it, with how often "
          "it ran, and how long it took.")
    t.run("cut -c1-121 rocprofsys-transpose.inst-output/*/wall_clock-*.txt | head -26", "sj_profile",
          word="lists")
    t.Say("The program itself is at the top. Below it are the two calls to run, one on each thread, "
          "which we instrumented.")
    t.wait_until_said("program")
    t.spotlight("transpose.inst ")
    t.wait_until_said("two")
    t.spotlight("|1>>>     |_run")
    t.wait_until_said("each")
    t.spotlight("|0>>> |_run")
    t.Say("Under run are the HIP calls it made. Systems Profiler traces those without any "
          "instrumentation.")
    t.spotlight("|1>>>       |_hipSetDevice", "|1>>>       |_hipFree")
    t.Say("Most of the time goes to hipStreamSynchronize, where each thread waits for its kernels to "
          "finish.")
    t.wait_until_said("hipStreamSynchronize")
    t.spotlight("|1>>>       |_hipStreamSynchronize")
    t.pause()
    t.clear_spotlight()
    t.Say("Next, let's see these calls in ROCm Optiq.")


# Video 4: GPU applications

def chapter_rocm_apis(t):
    t.Say("Systems Profiler traces the ROCm runtime through ROCprofiler-SDK: HIP API calls, kernel "
          "dispatches, memory copies, and more.")
    t.Say("rocprof-sys-avail lists every domain it can trace.")
    t.run("rocprof-sys-avail -bd -r ROCM_DOMAINS", "sl_domains", word="lists")
    t.Say("The rocm option picks the ones you want. Here, the HIP runtime, kernel dispatches, and "
          "memory copies.")
    t.run("rocprof-sys-sample --rocm=hip,kernel,memory -- ./transpose", "sl_trace", word="Here")
    t.pause()


def chapter_gpu_metrics(t):
    t.Say("In the background, Systems Profiler also samples each GPU's activity, temperature, power, "
          "and memory use, through AMD SMI.")
    t.Say("The gpu option picks the metrics, and dash dash gpus picks the GPUs, on a machine with "
          "several.")
    t.run("rocprof-sys-sample --gpu=busy,temp,power,mem_usage -- ./transpose", "sm_metrics",
          word="picks")
    t.Say("In ROCm Optiq, each metric becomes a counter track under its GPU.")
    t.pause()


def chapter_hardware_counters(t):
    t.Say("For a closer look at each kernel, collect GPU hardware counters. Dash H, dash c GPU, lists "
          "the ones your GPU supports.")
    t.run('rocprof-sys-avail -H -c GPU | grep -E "HARDWARE COUNTER|^\\| SQ_WAVES" | cut -c1-131',
          "sn_counters", word="lists")
    t.Say("SQ_WAVES counts the wavefronts each kernel launched. Dash G collects it as the program "
          "runs.")
    t.wait_until_said("counts")
    t.spotlight("SQ_WAVES:device=0")
    t.run("rocprof-sys-sample -G SQ_WAVES -o counters -- ./transpose", "sn_collect", word="collects")
    t.Say("The counter's summary file lists every kernel, with its values over all dispatches.")
    t.clear_spotlight()
    t.run("cut -c1-120 counters/*/rocprof-device-0-SQ_WAVES-*.txt", "sn_result", word="summary")
    t.Say("Each transpose dispatch launched about three million waves.")
    t.wait_until_said("transpose")
    t.spotlight("transpose_a")
    t.pause()
    t.clear_spotlight()


def chapter_roctx(t):
    t.Say("To see the phases of your own code in a trace, mark them with ROCTx ranges.")
    t.type("cd ~/roctx", word="phases")
    t.enter(cwd="~/roctx")
    t.Say("The roctx example from the same repository starts a range, and pushes a nested one, "
          "around its GPU work.")
    t.run('sed -n "66,92p" roctx.cpp', "so_source", word="example")
    t.wait_until_said("starts")
    t.spotlight("roctxRangeStart(", "roctxRangePush(")
    t.wait_until_said("around")
    t.spotlight("roctxRangePop()", "roctxRangeStop(")
    t.Say("Build it with the ROCTx library,")
    t.clear_spotlight()
    t.run("hipcc --offload-arch=gfx1100 -O2 -g roctx.cpp -o roctx -lrocprofiler-sdk-roctx",
          "so_build", word="Build")
    t.Say("and add the marker domain when you profile it.")
    t.run("rocprof-sys-sample --rocm=marker,hip,kernel --gpu -- ./roctx", "so_run", word="marker")
    t.pause()
    t.Say("Let's open this run in ROCm Optiq.")


CHAPTERS = [
    ("sa_what_it_does", chapter_what_it_does),
    ("sb_your_first_profile", chapter_first_profile),
    ("sd_presets", chapter_presets),
    ("se_saving_and_changing_settings", chapter_saving_settings),
    ("sf_choosing_the_output", chapter_output_options),
    ("sg_call_stack_sampling", chapter_sampling),
    ("sh_runtime_instrumentation", chapter_runtime_instrumentation),
    ("si_rewriting_a_binary", chapter_binary_rewrite),
    ("sj_reading_the_profile", chapter_reading_the_profile),
    ("sl_tracing_rocm_apis", chapter_rocm_apis),
    ("sm_gpu_metrics", chapter_gpu_metrics),
    ("sn_gpu_hardware_counters", chapter_hardware_counters),
    ("so_marking_regions_with_roctx", chapter_roctx),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--capture", type=Path, default=HERE / "systems-capture")
    parser.add_argument("--narration", type=Path, default=HERE / "voice-systems")
    parser.add_argument("--out", type=Path, default=HERE / "raw-systems")
    parser.add_argument("--chapter", default="", help="only chapters starting with this, like sb")
    args = parser.parse_args()
    ffmpeg, _ = mm.tools()
    narration = args.narration if (args.narration / "lines.tsv").exists() else None
    if narration is None:
        print(f"No synthesized narration in {args.narration}; line lengths are estimated.")
    failed = 0
    for stem, chapter in CHAPTERS:
        if stem.startswith(args.chapter):
            take = Take(stem, args.out, narration, args.capture, ffmpeg)
            chapter(take)
            failed += 0 if take.finish() else 1
    sys.exit(f"{failed} chapter(s) did not run as scripted" if failed else 0)


if __name__ == "__main__":
    main()
