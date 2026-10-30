#!/usr/bin/env bash
# Runs the commands of the ROCm Systems Profiler tutorials and saves each
# command's output, line by line with the time it appeared, for
# systems_chapters.py to replay on screen. Run it inside the public ROCm image
# (systems_docker.sh does), with the profiler's examples at /examples:
#
#   bash systems_docker.sh < systems_capture.sh
#
# Writes ~/capture/<id>.tsv ("<seconds>\t<line>") per command, and copies of the
# output folders the Optiq chapters open into ~/capture/optiq/.
set -u
CAPTURE=~/capture
rm -rf "$CAPTURE" ~/transpose ~/roctx ~/.rocprof-sys.cfg
mkdir -p "$CAPTURE/optiq" ~/transpose ~/roctx
cp /examples/transpose/transpose.cpp ~/transpose/
cp /examples/roctx/roctx.cpp /examples/roctx/roctx-example-kernels.hpp ~/roctx/ 2>/dev/null

stamp() {
    python3 -u -c '
import sys, time
start = float(sys.argv[1])
for line in sys.stdin:
    sys.stdout.write(f"{time.time() - start:.3f}\t{line.rstrip(chr(10))}\n")
' "$1"
}

run() {
    local id=$1
    shift
    local start
    start=$(python3 -c 'import time; print(time.time())')
    eval "$@" 2>&1 | stamp "$start" > "$CAPTURE/$id.tsv"
    echo "$id: $(wc -l < "$CAPTURE/$id.tsv") lines"
}

# The run each Optiq chapter opens, as the output folder the user copies over.
keep() {
    local name=$1 folder=$2
    mkdir -p "$CAPTURE/optiq/$name"
    cp -r "$folder" "$CAPTURE/optiq/$name/"
}

# Each run gets an output folder of its own minute.
next_minute() {
    sleep $((61 - $(date +%S | sed 's/^0//')))
}

cd ~/transpose || exit 1

# Video 1: getting started
run sa_tools 'ls /opt/rocm/bin | grep rocprof-sys'
run sa_version 'rocprof-sys-sample --version'
run sb_ls 'ls'
run sb_build 'hipcc --offload-arch=gfx1100 -O2 -g transpose.cpp -o transpose'
run sb_plain './transpose'
run sb_sample 'rocprof-sys-sample -- ./transpose'
keep first rocprofsys-transpose-output
run sb_list 'ls rocprofsys-transpose-output/*/'
next_minute

# Video 2: configuring
run sd_presets 'rocprof-sys-run --list-presets'
run sd_explain 'rocprof-sys-run --explain=balanced'
run sd_balanced 'rocprof-sys-sample --preset=balanced -- ./transpose'
run sd_export 'rocprof-sys-run --preset=balanced --gpu=busy,power --export-config=gpu-profile.json'
run sd_export_cat 'head -17 gpu-profile.json'
run sd_reuse 'rocprof-sys-sample --preset=./gpu-profile.json -- ./transpose'
next_minute
run se_avail 'rocprof-sys-avail -bd -r SAMPLING_FREQ'
run se_env 'ROCPROFSYS_SAMPLING_FREQ=200 rocprof-sys-sample -- ./transpose'
run se_cli 'ROCPROFSYS_SAMPLING_FREQ=200 rocprof-sys-sample -f 500 -- ./transpose'
run se_generate 'rocprof-sys-avail -G ~/.rocprof-sys.cfg'
run se_grep 'grep -E "^ROCPROFSYS_(TRACE|PROFILE|USE_SAMPLING|SAMPLING_FREQ|USE_ROCPD) " ~/.rocprof-sys.cfg'
rm -f ~/.rocprof-sys.cfg
next_minute
run sf_ls 'ls rocprofsys-transpose-output'
run sf_output 'rocprof-sys-sample -o results first-run -- ./transpose'
run sf_ls2 'ls results/*'
run sf_formats 'rocprof-sys-sample --output-format rocpd proto text -- ./transpose'
next_minute

# Video 3: sampling and instrumentation
run sg_profile 'rocprof-sys-sample -f 200 --profile -o samples -- ./transpose'
run sg_tree 'cut -c1-131 samples/*/sampling_wall_clock-*.txt | head -16'
next_minute
run sh_runtime 'rocprof-sys-instrument -- ./transpose'
next_minute
run si_simulate 'rocprof-sys-instrument --simulate --dump-info -o transpose.inst -- ./transpose'
run si_instrumented 'cat rocprofsys-transpose.inst-output/instrumentation/instrumented.txt'
run si_available 'grep -E "#Instructions| main | run " rocprofsys-transpose.inst-output/instrumentation/available.txt | cut -c1-130'
run si_rewrite 'rocprof-sys-instrument -i 256 -o transpose.inst -- ./transpose'
run si_run 'rocprof-sys-run --profile -- ./transpose.inst'
keep instrumented rocprofsys-transpose.inst-output
run sj_profile 'cut -c1-121 rocprofsys-transpose.inst-output/*/wall_clock-*.txt | head -26'
next_minute

# Video 4: GPU applications
run sl_domains 'rocprof-sys-avail -bd -r ROCM_DOMAINS'
run sl_trace 'rocprof-sys-sample --rocm=hip,kernel,memory -- ./transpose'
run sm_metrics 'rocprof-sys-sample --gpu=busy,temp,power,mem_usage -- ./transpose'
next_minute
run sn_counters 'rocprof-sys-avail -H -c GPU | grep -E "HARDWARE COUNTER|^\| SQ_WAVES" | cut -c1-131'
run sn_collect 'rocprof-sys-sample -G SQ_WAVES -o counters -- ./transpose'
run sn_result 'cut -c1-120 counters/*/rocprof-device-0-SQ_WAVES-*.txt'
next_minute
cd ~/roctx || exit 1
run so_source 'sed -n "66,92p" roctx.cpp'
run so_build 'hipcc --offload-arch=gfx1100 -O2 -g roctx.cpp -o roctx -lrocprofiler-sdk-roctx'
run so_run 'rocprof-sys-sample --rocm=marker,hip,kernel --gpu -- ./roctx'
keep roctx rocprofsys-roctx-output

chown -R "${HOST_UID:-1000}:${HOST_GID:-1000}" ~
