#!/usr/bin/env bash
# Runs a script from stdin in the public ROCm image, as the tutorials' user on a
# machine named demo-node, with the ROCm Systems Profiler examples at /examples.
# Needs Docker, an AMD GPU and the examples from github.com/ROCm/rocm-systems
# (projects/rocprofiler-systems/examples):
#
#   EXAMPLES=<examples folder> GPU_CARD=card1 GPU_RENDER=renderD128 \
#       bash systems_docker.sh < systems_capture.sh
#
# The tutorials' home folder is ~/sys-public/home on the host.
set -eu
IMAGE=${IMAGE:-rocm/dev-ubuntu-24.04:10.1.0-full}
EXAMPLES=${EXAMPLES:-~/sys-public/rocm-systems/projects/rocprofiler-systems/examples}
HOMEDIR=${HOMEDIR:-~/sys-public/home}
mkdir -p "$HOMEDIR"
# The image has no machine ID; a real install does, and the profiler records it.
MACHINE_ID=$HOMEDIR/../machine-id
[ -s "$MACHINE_ID" ] || python3 -c 'import uuid; print(uuid.uuid4().hex)' > "$MACHINE_ID"
# One GPU, so the traces show one device; CAP_PERFMON lets PAPI read CPU counters.
exec docker run --rm -i --pid=host -v "$MACHINE_ID":/etc/machine-id:ro \
    --device=/dev/kfd --device="/dev/dri/${GPU_CARD:-card1}" --device="/dev/dri/${GPU_RENDER:-renderD128}" \
    --group-add "$(getent group video | cut -d: -f3)" --group-add "$(getent group render | cut -d: -f3)" \
    --cap-add=SYS_PTRACE --cap-add=PERFMON --security-opt seccomp=unconfined --hostname demo-node \
    -e HOME=/home/user -e USER=user -e LOGNAME=user -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
    -v "$HOMEDIR":/home/user -v "$EXAMPLES":/examples:ro -w /home/user \
    "$IMAGE" bash -s
