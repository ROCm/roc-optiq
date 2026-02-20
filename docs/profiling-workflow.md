:orphan:

<head>
  <meta charset="UTF-8">
  <meta name="description" content="ROCm Optiq profiling workflow and automation guide">
  <meta name="keywords" content="profiling, ROCm, AMD, SLURM, SSH, containers, automation">
</head>

# Profiling workflow

ROCm Optiq includes an integrated profiling workflow that allows you to profile applications locally, remotely via SSH, or on SLURM clusters, with optional container execution and automated AI analysis.

## Features

- **Multiple execution modes**: Local, remote SSH, and SLURM cluster submission
- **Container support**: Profile inside Docker, Podman, or Singularity containers
- **Automated AI analysis**: Automatically run `rocpd analyze` on profiling results
- **Job monitoring**: Track SLURM job status in real-time
- **Keyboard shortcuts**: Navigate timeline with configurable hotkeys
- **Color themes**: Choose from 6 professional color schemes

## Getting started

### Launching the profiling dialog

1. When ROCm Optiq starts, the Welcome tab appears
2. Click **Run Profiling** to open the profiling dialog
3. Select your execution mode:
   - **Local Machine**: Profile on your local system
   - **Remote via SSH**: Profile on a remote server
   - **Submit to SLURM Job Scheduler**: Submit to HPC cluster (requires `sbatch`)

### Local profiling

Configure and run profiling on your local machine:

1. **Application**: Path to your executable
2. **Arguments**: Command-line arguments
3. **Profiling Tool**: `rocprofv3` (currently supported)
4. **Tool Arguments**: Profiling flags (default: `--sys-trace --kernel-trace --memory-copy-trace`)
5. **Output Directory**: Where trace files will be saved

#### Container execution (optional)

To profile inside a container:

1. Check **Run in Container**
2. Click **Detect Containers** to auto-discover running containers
3. Select from dropdown or specify manually:
   - **Runtime**: Docker, Podman, or Singularity
   - **Container ID/Name**: Container identifier or `.sif` file path
   - **Exec Options**: Additional flags (e.g., `--privileged`, `--bind /path`)

#### AI analysis (optional)

To enable automated analysis:

1. Check **Run AI Analysis**
2. Select **AI Analysis LLM**:
   - **Local (No LLM)**: Basic analysis without cloud services
   - **Anthropic Claude**: Advanced insights (requires API key)
   - **OpenAI GPT**: Advanced insights (requires API key)
3. Enter **API Key** if using cloud LLMs (auto-populated from settings)

### Remote SSH profiling

Profile applications on remote servers:

1. **SSH Connection**:
   - **Host**: Remote hostname or IP address
   - **User**: SSH username
   - **Port**: SSH port (default: 22)
   - **SSH Key Path**: Path to private key (optional)
   - **Jump Host**: Bastion host for multi-hop SSH (format: `user@host:port`)
   - **SSH Options**: Additional SSH flags (optional)

2. Click **Test Connection** to verify connectivity

3. **Remote Application**:
   - **Application Path (on remote)**: Full path to executable on remote system
   - **Arguments**: Command-line arguments
   - **Output Path**: Remote directory for trace files
   - **Copy results back automatically**: Copy traces to local machine

#### SSH Jump Host example

For clusters behind bastion hosts:

```
Host:        gpu-node01.cluster.edu
User:        username
Port:        22
Jump Host:   user@bastion.cluster.edu:22
```

This connects through `bastion.cluster.edu` to reach `gpu-node01`.

### SLURM cluster profiling

Submit profiling jobs to HPC clusters with job schedulers:

1. Configure application and profiling options (same as local mode)

2. **SLURM Resource Requirements**:
   - **Job Name**: Name for your job (e.g., `rocprof_benchmark`)
   - **Partition**: SLURM partition/queue (e.g., `gpu`)
   - **Account**: Billing account (optional)
   - **Nodes**: Number of nodes (default: 1)
   - **Tasks**: Number of tasks (default: 1)
   - **CPUs per Task**: CPU cores per task (default: 1)
   - **GPUs**: Number of GPUs (default: 1)
   - **Time Limit**: Wall time in `HH:MM:SS` format (default: `01:00:00`)
   - **Extra Arguments**: Additional `sbatch` options

3. Click **Submit Job**

The system automatically:
- Generates a SLURM batch script
- Submits the job with `sbatch`
- Monitors job status every 5 seconds
- Retrieves results when complete
- Loads trace and AI analysis into ROCm Optiq

## Job monitoring

### Job Queue Monitor

Access the Job Queue Monitor to track multiple profiling jobs:

- **Location**: Automatically opens when SLURM jobs are submitted, or via **Tools** → **Job Queue Monitor**
- **Auto-refresh**: Configurable interval (1-60 seconds)
- **Status colors**:
  - Green: Running
  - Yellow: Pending
  - Blue: Completed
  - Red: Failed/Cancelled

### Job actions

- **Cancel**: Stop a running or pending job
- **Output**: View job output file in system editor
- **View Results**: Load trace and AI analysis into ROCm Optiq

## Keyboard shortcuts

### Timeline navigation

#### Zoom
- `+` or `=`: Zoom in (20%)
- `-`: Zoom out (25%)
- `Ctrl+0`: Reset zoom to fit all data

#### Pan
- `←` / `→`: Pan left/right (10% of view)
- `Shift+←` / `Shift+→`: Pan faster (25% of view)
- `Home`: Jump to start
- `End`: Jump to end

#### Vertical scroll
- `↑` / `↓`: Scroll up/down (20 pixels)
- `Ctrl+↑` / `Ctrl+↓`: Scroll faster (100 pixels)
- `Page Up` / `Page Down`: Scroll by page

#### Selection
- `Ctrl+A`: Select entire time range
- `Escape`: Clear selection
- `Ctrl+F`: Frame selection (center in view)
- `Ctrl+Shift+Z`: Zoom to fit selection

### Customizing shortcuts

Keyboard shortcuts can be customized in **Settings** → **Keyboard Shortcuts** (planned feature).

## Settings and customization

### Display settings

#### Color schemes

Choose from 6 professional color schemes in **Settings** → **Display** → **Color Scheme**:

| Scheme | Description |
| ------ | ----------- |
| **Default** | Standard light or dark theme |
| **High Contrast** | Maximum contrast for accessibility |
| **Solarized** | Popular low-contrast theme |
| **Nord** | Cool, arctic-inspired colors |
| **Monokai** | Warm, vibrant syntax highlighting |
| **Dracula** | Modern, eye-friendly dark theme |

#### Font settings

- **DPI-based Font Scaling**: Automatically scale font size based on display DPI
- **Font Size**: Manual size adjustment (disabled when DPI scaling is enabled)

### Profiling settings

In **Settings** → **Other** → **Profiling options**:

- **Auto-run AI analysis after profiling**: Enable by default for all profiling sessions

#### API keys

Save LLM API keys for reuse in **Settings** → **Other** → **AI Analysis API Keys**:

- **Anthropic API Key**: For Claude models ([Get API key](https://console.anthropic.com/))
- **OpenAI API Key**: For GPT models ([Get API key](https://platform.openai.com/api-keys))

API keys are stored locally and auto-populated in profiling dialogs.

## AI recommendation execution

When AI analysis generates profiling recommendations, you can execute them directly from the AI Analysis view:

1. **Navigate to AI Analysis tab** after loading an analyzed trace
2. **Expand a recommendation** to view suggested profiling commands
3. **Click "▶ Run This Command"** next to the recommendation
4. **Review the configuration**:
   - If the trace was profiled through ROCm Optiq, your original configuration (Local/Remote SSH/SLURM) is restored
   - The recommended tool arguments are automatically applied
   - All other settings (application path, SSH host, etc.) are preserved
5. **Modify if needed** and click **Run** to profile with the new settings

### Benefits

- **Preserves execution context**: Remote profiling recommendations run remotely, SLURM recommendations submit to SLURM
- **One-click re-profiling**: No need to manually reconfigure settings
- **Iterative optimization**: Quickly test different profiling flags based on AI suggestions

### Example workflow

1. Profile your application with basic flags: `--sys-trace`
2. AI analysis suggests: "Use `--kernel-trace --hip-api` for GPU kernel insights"
3. Click "▶ Run This Command" on the recommendation
4. ROCm Optiq opens profiling dialog with:
   - Same application path
   - Same execution mode (e.g., Remote SSH to `gpu-server.example.com`)
   - Updated tool args: `--kernel-trace --hip-api`
5. Click Run to re-profile with enhanced tracing

## Troubleshooting

### SSH connection issues

#### Connection refused

**Causes**: SSH server not running, firewall blocking port, wrong port number

**Solutions**:
1. Verify SSH server is running: `sudo systemctl status sshd`
2. Check firewall: `sudo firewall-cmd --list-all`
3. Verify port in `/etc/ssh/sshd_config`
4. Test from terminal: `ssh -p 22 user@host`

#### Connection timeout

**Causes**: Incorrect hostname, network issue, firewall blocking

**Solutions**:
1. Verify hostname: `ping hostname`
2. Check network connectivity
3. Try connecting from terminal: `ssh -p 22 user@host`

#### Authentication failed

**Causes**: SSH key not set up, wrong username, incorrect permissions

**Solutions**:
1. Verify SSH key exists: `ls -la ~/.ssh/`
2. Copy key to remote: `ssh-copy-id user@host`
3. Check key permissions: `chmod 600 ~/.ssh/id_rsa`
4. Specify custom key in "SSH Key Path" field

### Container execution issues

#### No containers detected

**Solutions**:
1. Verify Docker/Podman is running
2. Check permissions: `docker ps` or `podman ps`
3. Ensure containers are running (not stopped)

#### Container execution fails

**Solutions**:
1. Verify ROCm tools are available inside container
2. Add `--privileged` flag for GPU access
3. Bind mount necessary paths with `--bind`
4. Check container has necessary permissions

### SLURM job issues

#### Job stays pending

**Solutions**:
1. Check partition limits: `sinfo`
2. Verify account resources: `sacctmgr show assoc`
3. Reduce resource requirements
4. Check job queue: `squeue -u $USER`

#### Job fails immediately

**Solutions**:
1. View job output via **Output** button in Job Queue Monitor
2. Verify application path on cluster
3. Check ROCm modules are loaded
4. Test application manually on cluster first

### AI analysis issues

#### Analysis not running

**Solutions**:
1. Verify **Run AI Analysis** is enabled
2. Check `rocpd` is installed: `which rocpd`
3. Verify output directory permissions
4. Review profiling output for `rocpd` errors

#### LLM API errors

**Solutions**:
1. Verify API key is correct
2. Check API quota/credits
3. Test API key independently
4. Try **Local (No LLM)** provider to test basic analysis

## Best practices

### Local profiling
- Test with simple applications first
- Use default tool arguments initially
- Verify ROCm tools are in PATH: `which rocprofv3`

### SSH profiling
- Set up SSH keys (avoid password authentication)
- Test connection manually before using dialog
- Use `~/.ssh/config` for complex SSH configurations

### SLURM profiling
- Start with short time limits (5-10 minutes)
- Test application on cluster before submitting profiling job
- Monitor jobs in Job Queue Monitor
- Clean up old jobs to free resources

### Container profiling
- Ensure ROCm is installed in container
- Test container execution manually first
- Use appropriate exec options for GPU access
- Bind mount input/output directories

## Command-line tool arguments

### Common rocprofv3 arguments

```bash
--sys-trace              # System-level tracing
--kernel-trace           # Kernel execution tracing
--memory-copy-trace      # Memory transfer tracing
--hip-trace              # HIP API tracing
--marker-trace           # Application markers
```

### rocpd analyze options

```bash
rocpd analyze -i <trace.db> --format json -o <output.json>
```

Options:
- `-i`: Input trace file
- `--format json`: Output format (JSON for ROCm Optiq)
- `-o`: Output file path

## Related topics

- [Timeline view](timeline.md) (if exists)
- [Advanced details panel](details-panel.md) (if exists)
- [System topology view](topology.md) (if exists)

---

*For issues or feature requests, visit [GitHub Issues](https://github.com/ROCm/roc-optiq/issues)*
