:orphan:

<head>
  <meta charset="UTF-8">
  <meta name="description" content="ROCm Optiq container-based profiling guide for Docker, Podman, and Singularity">
  <meta name="keywords" content="containers, Docker, Podman, Singularity, profiling, ROCm, AMD, GPU">
</head>

# Container-based profiling

ROCm Optiq supports profiling applications running inside containers, enabling consistent environments, reproducible profiling, and HPC workflow integration. Supported container runtimes include Docker, Podman, and Singularity.

## Overview

### Why profile in containers?

**Benefits of container-based profiling:**

1. **Environment consistency**: Identical runtime environment across systems
2. **Dependency isolation**: No conflicts with host system libraries
3. **Reproducibility**: Same profiling results on different machines
4. **HPC integration**: Native Singularity support for cluster environments
5. **Version control**: Container images versioned with application code
6. **Easy distribution**: Share exact profiling environment with team

**Use cases:**
- Multi-GPU application development
- HPC cluster profiling with Singularity
- CI/CD pipeline integration
- Reproducible performance benchmarking
- Isolated ROCm version testing

### Supported container runtimes

| Runtime | Type | Best For | ROCm Support |
| ------- | ---- | -------- | ------------ |
| **Docker** | Container platform | Local development, testing | Excellent |
| **Podman** | Daemonless container | Rootless execution, security | Excellent |
| **Singularity** | HPC container | Cluster computing, shared systems | Excellent |

## Docker profiling

### Prerequisites

#### Docker installation

**Verify Docker installed and running:**

```bash
docker --version
docker ps
```

**If not installed:**
- **Linux**: Follow [Docker Engine installation](https://docs.docker.com/engine/install/)
- **Windows**: Install [Docker Desktop](https://www.docker.com/products/docker-desktop/)
- **macOS**: Install [Docker Desktop](https://www.docker.com/products/docker-desktop/)

#### ROCm-enabled Docker container

**Option 1: Use official ROCm Docker images**

```bash
# Pull ROCm base image
docker pull rocm/dev-ubuntu-22.04:6.0

# Run with GPU access
docker run -it --device=/dev/kfd --device=/dev/dri --security-opt seccomp=unconfined \
  --group-add video --name rocm_container rocm/dev-ubuntu-22.04:6.0
```

**Option 2: Build custom Dockerfile**

```dockerfile
FROM rocm/dev-ubuntu-22.04:6.0

# Install ROCm profiling tools
RUN apt-get update && apt-get install -y \
    rocprofiler-dev \
    roctracer-dev \
    rocm-smi

# Install your application
COPY myapp /usr/local/bin/
RUN chmod +x /usr/local/bin/myapp

# Set environment
ENV PATH="/opt/rocm/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/rocm/lib:${LD_LIBRARY_PATH}"

CMD ["/bin/bash"]
```

Build and run:
```bash
docker build -t myapp-rocm .
docker run -it --device=/dev/kfd --device=/dev/dri --group-add video myapp-rocm
```

### Profiling workflow with Docker

#### Automatic container detection

**ROCm Optiq can auto-discover running Docker containers:**

1. Open profiling dialog: **Run Profiling** from Welcome tab
2. Select **Local Machine** execution mode
3. Enable **Run in Container**
4. Click **Detect Containers**
5. Running containers appear in dropdown:
   ```
   [Docker] my_container (rocm/dev-ubuntu-22.04:6.0) - Up 2 hours
   [Docker] app_test (myapp:latest) - Up 30 minutes
   ```

6. Select target container
7. Configure profiling options
8. Click **Run**

**What happens:**
- ROCm Optiq wraps profiling command with `docker exec`
- Command executes inside selected container
- Output captured and displayed
- Trace files accessible via container volume mounts

#### Manual container specification

**If container not detected or using specific configuration:**

1. Enable **Run in Container**
2. Set **Runtime**: Docker
3. Enter **Container ID/Name**: `my_container` or `abc123def456`
4. Add **Exec Options** (optional): `--user root --env VAR=value`

**Finding container ID/name:**

```bash
# List running containers
docker ps

# List all containers (including stopped)
docker ps -a

# Get container ID by name
docker ps --filter "name=my_container" --format "{{.ID}}"
```

#### Exec options

**Additional flags for `docker exec` command:**

Common options:

| Option | Purpose | Example |
| ------ | ------- | ------- |
| `--user` | Run as specific user | `--user root` |
| `--env` | Set environment variable | `--env ROCR_VISIBLE_DEVICES=0` |
| `--workdir` | Working directory | `--workdir /workspace` |
| `--privileged` | Extended privileges | `--privileged` |

**Example configuration:**
```
Runtime: Docker
Container ID: rocm_dev
Exec Options: --user root --env HSA_ENABLE_SDMA=0 --workdir /app
```

### Docker command wrapping

**Original profiling command:**
```bash
rocprofv3 --sys-trace /path/to/app --arg1 --arg2
```

**Wrapped for Docker:**
```bash
docker exec rocm_container bash -c 'rocprofv3 --sys-trace /path/to/app --arg1 --arg2'
```

**With exec options:**
```bash
docker exec --user root --env HSA_ENABLE_SDMA=0 rocm_container bash -c 'rocprofv3 --sys-trace /path/to/app --arg1 --arg2'
```

### Volume mounting for results

**Ensure trace files accessible on host:**

**Method 1: Container already has volume mount**

```bash
docker run -it --device=/dev/kfd --device=/dev/dri \
  -v /host/path:/container/path \
  --name rocm_container rocm/dev-ubuntu-22.04:6.0
```

Set **Output Directory** in profiling dialog to `/container/path`.
Results appear in `/host/path` on host.

**Method 2: Copy results after profiling**

```bash
# After profiling completes
docker cp rocm_container:/container/output/trace.db /host/path/
```

ROCm Optiq can automate this if configured.

### GPU access

**Docker containers must have GPU device access to profile GPU applications.**

**Required flags:**
```bash
--device=/dev/kfd       # GPU compute device
--device=/dev/dri       # GPU display/render devices
--group-add video       # Video group for permissions
--security-opt seccomp=unconfined  # For profiling
```

**Verify GPU access inside container:**

```bash
docker exec rocm_container rocm-smi
```

Should display GPU information. If not, check device flags.

## Remote container profiling

ROCm Optiq supports profiling applications running in containers on **remote servers** via SSH. This combines SSH remote execution with container exec commands.

### How it works

When you enable both **Remote Execution** and **Run in Container**, ROCm Optiq:

1. Connects to remote server via SSH
2. Wraps the profiling command with container exec (docker/podman/singularity)
3. Executes inside the container on the remote server
4. Copies results back to local machine

**Command chain:**
```bash
# Local machine -> SSH -> Remote server -> Container exec
ssh user@remote "docker exec container_name bash -c 'rocprofv3 --sys-trace /app/binary'"
```

### Setup instructions

#### Step 1: Ensure container is running on remote server

**On your remote server:**

```bash
# Verify Docker/Podman is installed and running
docker ps  # or: podman ps

# Start your container if not running
docker run -d --name my_app_container \
  --device=/dev/kfd --device=/dev/dri \
  --group-add video \
  -v /data:/data \
  rocm/dev-ubuntu-22.04:6.0

# Verify rocprofv3 is available inside container
docker exec my_app_container which rocprofv3
```

#### Step 2: Configure ROCm Optiq profiling dialog

**In ROCm Optiq profiling dialog:**

1. **Execution Mode**: Select **Remote (SSH)**
2. **SSH Connection**:
   - Host: `user@remote-server.example.com`
   - Port: `22` (or custom port)
   - SSH Key: Path to your SSH private key
   - Proxy Jump: (optional) `bastion.example.com`

3. **Enable "Run in Container"** checkbox

4. **Container Configuration**:
   - Runtime: **Docker** (or Podman/Singularity)
   - Container ID: `my_app_container`
   - Exec Options: `--user root` (optional)

5. **Application & Tool Arguments**:
   - Remote App Path: `/app/my_application`
   - Remote Args: `--input data.bin`
   - Tool Args: `--sys-trace --kernel-trace`

6. **Output**:
   - Remote Output Path: `/data/profiling_output`
   - Copy Results Back: **Enabled**

7. Click **Run**

#### Step 3: What happens during execution

**ROCm Optiq executes:**

```bash
# 1. SSH connection
ssh -i ~/.ssh/id_rsa user@remote-server.example.com

# 2. Container exec wrapped command
"docker exec my_app_container bash -c 'rocprofv3 --sys-trace --kernel-trace -d /data/profiling_output/20260220_120530_123 -- /app/my_application --input data.bin'"

# 3. File copy back via SCP
scp user@remote-server:/data/profiling_output/20260220_120530_123/merged.db /local/path/
```

### Container runtime specific examples

#### Docker on remote server

```
Execution Mode: Remote (SSH)
SSH Host: user@gpu-server.example.com
Run in Container: ✓ Enabled
  Runtime: Docker
  Container ID: rocm_dev
  Exec Options: --env HSA_ENABLE_SDMA=0
Remote App Path: /workspace/gpu_app
Remote Output Path: /workspace/traces
```

**Resulting command:**
```bash
ssh user@gpu-server.example.com "docker exec --env HSA_ENABLE_SDMA=0 rocm_dev bash -c 'rocprofv3 -d /workspace/traces/20260220_120530_123 -- /workspace/gpu_app'"
```

#### Podman on remote server (rootless)

```
Execution Mode: Remote (SSH)
SSH Host: user@hpc-node.example.com
Run in Container: ✓ Enabled
  Runtime: Podman
  Container ID: my_podman_container
  Exec Options: (empty for rootless)
Remote App Path: /home/user/app
```

**Resulting command:**
```bash
ssh user@hpc-node.example.com "podman exec my_podman_container bash -c 'rocprofv3 -d /output -- /home/user/app'"
```

#### Singularity on HPC cluster

```
Execution Mode: Remote (SSH)
SSH Host: user@cluster.hpc.edu
Run in Container: ✓ Enabled
  Runtime: Singularity
  Container ID: /scratch/containers/rocm_app.sif
  Exec Options: --nv --bind /scratch
Remote App Path: /app/simulation
```

**Resulting command:**
```bash
ssh user@cluster.hpc.edu "singularity exec --nv --bind /scratch /scratch/containers/rocm_app.sif bash -c 'rocprofv3 -d /scratch/output -- /app/simulation'"
```

### Container detection on remote servers

**Currently not supported:** Automatic container detection only works for local containers.

**Workaround:** Manually find container ID on remote server:

```bash
# SSH into remote server
ssh user@remote-server

# List running containers
docker ps --format "table {{.ID}}\t{{.Names}}\t{{.Image}}\t{{.Status}}"

# Or for Podman
podman ps --format "table {{.ID}}\t{{.Names}}\t{{.Image}}\t{{.Status}}"

# Copy the container ID or name to use in ROCm Optiq
```

### Volume mounting considerations

**For remote container profiling, ensure:**

1. **Output directory is accessible** to both container and SSH user:
   ```bash
   # On remote server, when starting container
   docker run -v /shared/profiling:/output ... rocm/dev
   ```

2. **Remote output path matches** container mount:
   ```
   Remote Output Path: /output  # (container path)
   ```

3. **SCP can access** the files:
   - Container writes to `/output/20260220_HHMMSS_mmm/merged.db`
   - This maps to `/shared/profiling/20260220_HHMMSS_mmm/merged.db` on host
   - SCP copies from host path to local machine

**Example setup:**

```bash
# Remote server: Start container with volume mount
docker run -d --name profiling_container \
  --device=/dev/kfd --device=/dev/dri \
  -v /home/user/traces:/traces \
  rocm/dev-ubuntu-22.04:6.0

# ROCm Optiq configuration:
Remote Output Path: /traces
Container ID: profiling_container

# Results flow:
# 1. rocprofv3 writes to: /traces/20260220_HHMMSS/merged.db (inside container)
# 2. Host sees file at: /home/user/traces/20260220_HHMMSS/merged.db
# 3. SCP copies from: user@remote:/home/user/traces/20260220_HHMMSS/merged.db
```

### Troubleshooting remote container profiling

#### Container not found

**Error:** `Error response from daemon: No such container: my_container`

**Solution:**
```bash
# On remote server, verify container is running
ssh user@remote docker ps -a

# If stopped, start it
ssh user@remote docker start my_container
```

#### Permission denied in container

**Error:** `Permission denied: /output/trace.db`

**Solution:** Use exec options to run as root or specific user:
```
Exec Options: --user root
```

#### rocprofv3 not found in container

**Error:** `rocprofv3: command not found`

**Solution:** Ensure ROCm tools are installed in container:
```bash
# Check on remote
ssh user@remote docker exec container_name which rocprofv3

# If not found, install in container or use different image
```

#### File copy fails

**Error:** `scp: /remote/path/merged.db: No such file`

**Solution:** Verify volume mount paths align:
```bash
# Inside container, rocprofv3 writes to: /output/merged.db
# On host, this should appear at: /actual/host/path/merged.db
# SCP must use: user@remote:/actual/host/path/merged.db
```

**Check paths:**
```bash
# Find container's volume mounts
ssh user@remote docker inspect container_name | grep -A 10 Mounts

# Adjust Remote Output Path to match container's mount point
```

### Best practices

1. **Pre-start containers:** Don't start/stop containers for each profiling run
2. **Use persistent volumes:** Mount host directory for easy file access
3. **Set ROCR_VISIBLE_DEVICES:** In exec options for multi-GPU setups
4. **Test container exec locally first:** Verify command works before adding SSH
5. **Use container names:** More readable than IDs (`my_app` vs `a1b2c3d4`)

### See also

- [Profiling workflow](profiling-workflow.md) - Main profiling documentation
- [Remote profiling](profiling-workflow.md#remote-ssh-profiling) - SSH configuration
- Local container profiling sections below for runtime-specific details

## Podman profiling

### Prerequisites

#### Podman installation

**Verify Podman installed:**

```bash
podman --version
podman ps
```

**If not installed:**
- **Linux**: Install via package manager (e.g., `apt install podman`, `dnf install podman`)
- **Windows/macOS**: Install [Podman Desktop](https://podman.io/desktop/)

#### Rootless vs. root mode

**Podman supports both rootless and root execution:**

- **Rootless** (default): Better security, no daemon
- **Root**: Similar to Docker, may require for some GPU operations

**For ROCm profiling, rootless is recommended when possible.**

### Profiling workflow with Podman

#### Automatic container detection

**Same process as Docker:**

1. Enable **Run in Container**
2. Click **Detect Containers**
3. Podman containers appear with `[Podman]` prefix:
   ```
   [Podman] my_podman_container (rocm/dev-ubuntu-22.04:6.0) - Up 1 hour
   ```

4. Select and configure

**Note:** ROCm Optiq auto-detects runtime type (Docker vs. Podman).

#### Manual specification

```
Runtime: Podman
Container ID: my_container
Exec Options: --user root
```

### Podman command wrapping

**Same format as Docker, using `podman exec`:**

```bash
podman exec my_container bash -c 'rocprofv3 --sys-trace /path/to/app'
```

### Differences from Docker

**Key Podman-specific considerations:**

1. **No daemon**: Podman doesn't require background daemon
2. **Rootless by default**: Better security, may need user namespace setup
3. **SELinux**: May affect file access on Fedora/RHEL systems
4. **cgroups v2**: Different resource management

### GPU access with Podman

**ROCm GPU access in rootless Podman:**

```bash
podman run -it --device /dev/kfd --device /dev/dri --group-add keep-groups \
  rocm/dev-ubuntu-22.04:6.0
```

**For root Podman:**

```bash
sudo podman run -it --device /dev/kfd --device /dev/dri \
  rocm/dev-ubuntu-22.04:6.0
```

**Verify GPU access:**

```bash
podman exec my_container rocm-smi
```

### SELinux considerations

**On RHEL/Fedora with SELinux enabled:**

```bash
# Check SELinux status
sestatus

# If enforcing, may need to relabel volumes
podman run -v /host/path:/container/path:Z ...
```

The `:Z` flag relabels volume for container access.

## Singularity profiling

### Overview

**Singularity (now Apptainer) is designed for HPC environments:**

- No daemon required
- No privilege escalation
- MPI support
- Bind mount flexibility
- Native cluster integration

### Prerequisites

#### Singularity installation

**Verify Singularity/Apptainer installed:**

```bash
singularity --version
# or
apptainer --version
```

**If not installed:**
- HPC clusters: Contact system administrator
- Local install: Follow [Apptainer installation guide](https://apptainer.org/docs/admin/main/installation.html)

#### Singularity image file (.sif)

**Option 1: Build from Docker image**

```bash
# Convert Docker image to Singularity
singularity pull rocm_dev.sif docker://rocm/dev-ubuntu-22.04:6.0
```

**Option 2: Build from definition file**

```singularity
Bootstrap: docker
From: rocm/dev-ubuntu-22.04:6.0

%post
    apt-get update && apt-get install -y rocprofiler-dev

%environment
    export PATH="/opt/rocm/bin:${PATH}"
    export LD_LIBRARY_PATH="/opt/rocm/lib:${LD_LIBRARY_PATH}"

%runscript
    exec "$@"
```

Build:
```bash
sudo singularity build myapp.sif myapp.def
```

**Option 3: Download pre-built image**

```bash
singularity pull library://rocm/default/rocm:6.0
```

### Profiling workflow with Singularity

#### Manual specification (no auto-detection)

**Singularity images are files, not running containers, so auto-detection doesn't apply.**

1. Enable **Run in Container**
2. Set **Runtime**: Singularity
3. Enter **Container ID/Name**: `/path/to/container.sif`
4. Add **Exec Options** (bind mounts, GPU flags)

**Example configuration:**
```
Runtime: Singularity
Container ID/Name: /home/user/containers/rocm_dev.sif
Exec Options: --rocm --bind /scratch:/scratch --bind /data:/data
```

### Singularity command wrapping

**Original command:**
```bash
rocprofv3 --sys-trace /path/to/app
```

**Wrapped for Singularity:**
```bash
singularity exec /path/to/container.sif rocprofv3 --sys-trace /path/to/app
```

**With exec options:**
```bash
singularity exec --rocm --bind /scratch:/scratch /path/to/container.sif \
  rocprofv3 --sys-trace /path/to/app
```

### Bind mounts

**Singularity only has access to specific directories by default. Use bind mounts for access to host filesystem.**

**Common bind mount patterns:**

| Host Path | Container Path | Purpose |
| --------- | -------------- | ------- |
| `/scratch` | `/scratch` | Cluster scratch space |
| `/data` | `/data` | Shared datasets |
| `/home/user` | `/home/user` | User home directory |
| `/opt/rocm` | `/opt/rocm` | Host ROCm installation (optional) |

**Syntax:**
```bash
--bind /host/path:/container/path[:ro]
```

**Multiple bind mounts:**
```bash
--bind /scratch:/scratch --bind /data:/data:ro
```

**Read-only bind:**
```bash
--bind /data:/data:ro
```

**Example in profiling dialog:**
```
Exec Options: --bind /scratch:/scratch --bind /home/user/data:/data
```

### GPU access with Singularity

**ROCm GPU access:**

```bash
singularity exec --rocm container.sif rocm-smi
```

**The `--rocm` flag automatically:**
- Binds `/dev/kfd`, `/dev/dri`
- Sets up ROCm libraries
- Configures GPU permissions

**Alternative (manual device binding):**
```bash
singularity exec --device /dev/kfd --device /dev/dri container.sif rocm-smi
```

### Environment variables

**Pass environment variables to container:**

```bash
# Single variable
singularity exec --env ROCR_VISIBLE_DEVICES=0 container.sif app

# Multiple variables
singularity exec --env ROCR_VISIBLE_DEVICES=0 --env HSA_ENABLE_SDMA=0 container.sif app
```

**In profiling dialog:**
```
Exec Options: --env ROCR_VISIBLE_DEVICES=0 --env HSA_ENABLE_SDMA=0
```

### Singularity on HPC clusters

**Typical cluster workflow:**

1. Load Singularity module:
   ```bash
   module load singularity
   ```

2. Use shared container image:
   ```
   Container ID: /shared/containers/rocm_6.0.sif
   ```

3. Bind cluster-specific paths:
   ```
   Exec Options: --rocm --bind /lustre:/lustre --bind /scratch:/scratch
   ```

4. Submit via SLURM with container execution:
   - Profiling dialog handles Singularity wrapping
   - SLURM batch script includes Singularity commands

## Container profiling best practices

### Image preparation

#### Include profiling tools in container

**Ensure container has ROCm profiling tools installed:**

```dockerfile
# Dockerfile example
FROM rocm/dev-ubuntu-22.04:6.0

RUN apt-get update && apt-get install -y \
    rocprofiler-dev \
    roctracer-dev \
    rocm-smi \
    && rm -rf /var/lib/apt/lists/*
```

**Verify tools available:**

```bash
docker exec my_container which rocprofv3
docker exec my_container which rocpd
```

#### Set up working directories

**Create consistent paths for input/output:**

```dockerfile
RUN mkdir -p /workspace /data /results
WORKDIR /workspace
```

**Use in profiling dialog:**
```
Application: /workspace/myapp
Output Directory: /results
```

#### Pre-load datasets (optional)

**For reproducible profiling, include test data:**

```dockerfile
COPY test_data/ /data/
```

### Volume organization

**Recommended directory structure:**

```
/host/profiling/
├── containers/          # Container images
│   ├── app_v1.sif
│   └── app_v2.sif
├── input/               # Input datasets
│   └── test_data/
├── output/              # Profiling results
│   ├── run_001/
│   │   ├── trace.db
│   │   └── ai_analysis.json
│   └── run_002/
└── scripts/             # Helper scripts
```

**Mount in profiling dialog:**
```
-v /host/profiling/input:/input:ro
-v /host/profiling/output:/output
```

### Performance considerations

#### Avoid overhead

**Container profiling may introduce overhead:**

1. **Filesystem overhead**: Use bind mounts instead of copying files
2. **Network overhead**: Local containers only (avoid remote Docker sockets)
3. **Privileges**: Use `--privileged` only if necessary
4. **Resource limits**: Don't set CPU/memory limits during profiling

**Minimize overhead:**
- Use native container runtime (no emulation)
- Bind mount host ROCm libraries (Singularity)
- Pre-warm containers before profiling
- Use local container images (not pulling during profiling)

#### GPU sharing

**Multiple containers accessing same GPU:**

- Use `ROCR_VISIBLE_DEVICES` to assign specific GPUs
- Example: Container 1 uses GPU 0, Container 2 uses GPU 1

```bash
docker run --env ROCR_VISIBLE_DEVICES=0 ...   # GPU 0
docker run --env ROCR_VISIBLE_DEVICES=1 ...   # GPU 1
```

### Reproducibility

#### Version all components

**Pin versions for reproducible profiling:**

```dockerfile
FROM rocm/dev-ubuntu-22.04:6.0.0  # Specific ROCm version

RUN apt-get update && apt-get install -y \
    rocprofiler-dev=6.0.0-1 \      # Exact package version
    myapp=1.2.3 \
    && rm -rf /var/lib/apt/lists/*
```

#### Tag images

**Use descriptive tags:**

```bash
docker build -t myapp:v1.2.3-rocm6.0 .
docker tag myapp:v1.2.3-rocm6.0 myapp:latest
```

#### Document configurations

**Save container and profiling configuration:**

```yaml
# profiling_config.yaml
container:
  runtime: docker
  image: myapp:v1.2.3-rocm6.0
  exec_options: "--user root --env ROCR_VISIBLE_DEVICES=0"
profiling:
  tool: rocprofv3
  tool_args: "--sys-trace --kernel-trace"
  application: "/workspace/myapp"
  app_args: "--input /data/test.dat --iterations 1000"
```

## Troubleshooting

### Container detection issues

#### No containers detected

**Symptoms:** "No running containers found"

**Solutions:**

1. **Verify containers are running:**
   ```bash
   docker ps
   podman ps
   ```

2. **Check container runtime accessible:**
   ```bash
   which docker
   which podman
   ```

3. **Verify permissions:**
   ```bash
   # Docker: User in docker group
   groups | grep docker

   # If not, add user to docker group
   sudo usermod -aG docker $USER
   # Log out and back in
   ```

4. **Try manual specification** instead of auto-detection

#### Wrong runtime detected

**Symptoms:** Podman container shown as Docker or vice versa

**Solutions:**

1. Use manual specification with correct runtime
2. Check if both Docker and Podman running simultaneously
3. Verify ROCm Optiq detection logic in logs

### GPU access issues

#### GPU not visible in container

**Symptoms:** `rocm-smi` shows no GPUs inside container

**Solutions:**

1. **Docker: Add device flags**
   ```bash
   docker run --device=/dev/kfd --device=/dev/dri --group-add video ...
   ```

2. **Podman: Add device flags**
   ```bash
   podman run --device /dev/kfd --device /dev/dri --group-add keep-groups ...
   ```

3. **Singularity: Use --rocm flag**
   ```bash
   singularity exec --rocm container.sif rocm-smi
   ```

4. **Check host GPU visibility:**
   ```bash
   rocm-smi  # On host
   ls -la /dev/kfd /dev/dri
   ```

5. **Verify udev rules (Linux):**
   ```bash
   cat /etc/udev/rules.d/70-kfd.rules
   ```

#### Permission denied errors

**Symptoms:** GPU device permission errors inside container

**Solutions:**

1. **Add video group:**
   ```bash
   docker run --group-add video ...
   ```

2. **Run as root (not recommended for production):**
   ```
   Exec Options: --user root
   ```

3. **Check device permissions on host:**
   ```bash
   ls -la /dev/kfd /dev/dri
   # Should be accessible by video group
   ```

4. **Verify user in video group:**
   ```bash
   groups
   ```

### Profiling tool errors

#### rocprofv3 not found

**Symptoms:** "rocprofv3: command not found" inside container

**Solutions:**

1. **Verify tool installed in container:**
   ```bash
   docker exec my_container which rocprofv3
   docker exec my_container ls -la /opt/rocm/bin/
   ```

2. **Install profiling tools:**
   ```dockerfile
   RUN apt-get update && apt-get install -y rocprofiler-dev
   ```

3. **Set PATH environment:**
   ```
   Exec Options: --env PATH="/opt/rocm/bin:$PATH"
   ```

4. **Use full path:**
   ```
   Application: /opt/rocm/bin/rocprofv3
   ```

#### Library not found

**Symptoms:** "error while loading shared libraries: libhsa-runtime64.so.1"

**Solutions:**

1. **Set LD_LIBRARY_PATH:**
   ```
   Exec Options: --env LD_LIBRARY_PATH="/opt/rocm/lib:$LD_LIBRARY_PATH"
   ```

2. **Verify libraries in container:**
   ```bash
   docker exec my_container ldconfig -p | grep rocm
   ```

3. **Install ROCm runtime:**
   ```dockerfile
   RUN apt-get install -y rocm-dev
   ```

### File access issues

#### Cannot access input files

**Symptoms:** "No such file or directory" when accessing input data

**Solutions:**

1. **Docker: Add volume mount**
   ```bash
   docker run -v /host/data:/data ...
   ```

2. **Singularity: Add bind mount**
   ```
   Exec Options: --bind /host/data:/data
   ```

3. **Verify file exists in container:**
   ```bash
   docker exec my_container ls -la /data/input.dat
   ```

4. **Check permissions:**
   ```bash
   # On host
   ls -la /host/data/input.dat
   chmod 644 /host/data/input.dat
   ```

#### Cannot write output files

**Symptoms:** "Permission denied" writing trace files

**Solutions:**

1. **Create output directory on host:**
   ```bash
   mkdir -p /host/output
   chmod 777 /host/output  # Or appropriate permissions
   ```

2. **Mount with write access:**
   ```bash
   docker run -v /host/output:/output ...
   ```

3. **Check SELinux (Podman on RHEL/Fedora):**
   ```bash
   podman run -v /host/output:/output:Z ...
   ```

4. **Run as matching UID:**
   ```bash
   docker exec --user $(id -u):$(id -g) ...
   ```

## Advanced container profiling

### Multi-container profiling

**Profile distributed applications across containers:**

1. Create Docker network:
   ```bash
   docker network create rocm_net
   ```

2. Run containers on same network:
   ```bash
   docker run --network rocm_net --name app1 ...
   docker run --network rocm_net --name app2 ...
   ```

3. Profile with network communication:
   - Profile each container separately
   - Or profile orchestrator (e.g., MPI rank 0)

### CI/CD integration

**Automated profiling in pipelines:**

```yaml
# .gitlab-ci.yml example
profile_app:
  image: rocm/dev-ubuntu-22.04:6.0
  script:
    - rocprofv3 --sys-trace /app/myapp --iterations 100
    - rocpd merge -d . -o trace.db
    - rocpd analyze -i trace.db --format json -o analysis.json
  artifacts:
    paths:
      - trace.db
      - analysis.json
```

### Cluster profiling with Singularity

**SLURM batch script with container:**

```bash
#!/bin/bash
#SBATCH --job-name=container_profile
#SBATCH --partition=gpu
#SBATCH --gpus=1
#SBATCH --time=01:00:00

module load singularity rocm

singularity exec --rocm /shared/containers/myapp.sif \
  rocprofv3 --sys-trace /workspace/app --input /data/large_dataset.dat
```

Submit from ROCm Optiq profiling dialog with SLURM mode + container option enabled.

## Related topics

- [Profiling workflow](profiling-workflow.md)
- [Job monitoring](job-monitoring.md)
- [Settings](settings.md)

---

*For issues or feature requests, visit [GitHub Issues](https://github.com/ROCm/roc-optiq/issues)*
