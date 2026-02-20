:orphan:

<head>
  <meta charset="UTF-8">
  <meta name="description" content="ROCm Optiq SLURM job monitoring and queue management guide">
  <meta name="keywords" content="SLURM, job monitoring, HPC, queue, batch jobs, ROCm, AMD">
</head>

# Job monitoring and queue management

ROCm Optiq includes a comprehensive job queue monitoring system for tracking profiling jobs submitted to SLURM clusters. This feature provides real-time status updates, job management, and automated results retrieval.

## Overview

### Features

- **Real-time job monitoring**: Track multiple profiling jobs simultaneously
- **Auto-refresh**: Configurable automatic status updates (1-60 seconds)
- **Color-coded status**: Instant visual feedback on job state
- **Job actions**: Cancel, view output, load results
- **Detailed job information**: Resource allocation, runtime, node assignment
- **Persistent tracking**: Jobs tracked across application restarts
- **Automatic result loading**: Open traces and AI analysis when jobs complete

### Requirements

- SLURM workload manager installed and configured
- `squeue` command available in PATH
- `scancel` command for job cancellation
- SSH access to cluster (for remote job submission)
- Appropriate SLURM account and partition access

## Accessing the job queue monitor

### Opening the monitor window

**Method 1: Automatic (when submitting SLURM jobs)**
- Submit a profiling job via SLURM execution mode
- Job Queue Monitor opens automatically
- Job appears in queue with "Pending" or "Running" status

**Method 2: Menu bar**
1. Click **Tools** in the menu bar
2. Select **Job Queue Monitor**
3. Window opens showing all tracked jobs

**Method 3: Keyboard shortcut** (if configured)
- Default: Not assigned
- Configure in **Settings** → **Keyboard Shortcuts** → **Tools** → **Job Queue Monitor**

### Window layout

```
┌─────────────────────────────────────────────────────────────┐
│  Job Queue Monitor                                   [×]     │
├─────────────────────────────────────────────────────────────┤
│  [✓] Auto-refresh    Interval: [5] seconds   [Refresh Now] │
├─────────────────────────────────────────────────────────────┤
│  Job ID   │ Name          │ Partition │ Status    │ Actions │
│─────────────────────────────────────────────────────────────│
│  1234567  │ rocprof_job   │ gpu       │ Running   │ [···]   │
│  1234568  │ benchmark_gpu │ compute   │ Pending   │ [···]   │
│  1234569  │ trace_app     │ gpu       │ Completed │ [···]   │
├─────────────────────────────────────────────────────────────┤
│  Showing 3 jobs    Last updated: 10:42:35                   │
└─────────────────────────────────────────────────────────────┘
```

## Job status indicators

### Status colors

Jobs are color-coded for instant recognition:

| Status | Color | Meaning | Icon |
| ------ | ----- | ------- | ---- |
| **Pending** | Yellow | Queued, waiting for resources | 🟡 |
| **Running** | Green | Currently executing | 🟢 |
| **Completed** | Blue | Finished successfully | 🔵 |
| **Failed** | Red | Terminated with error | 🔴 |
| **Cancelled** | Orange | Manually cancelled by user | 🟠 |
| **Timeout** | Red | Exceeded time limit | ⏱️ |
| **Node Fail** | Red | Node hardware failure | ⚠️ |

### Status descriptions

#### Pending (PD)

**Job is queued but not yet running.**

**Common reasons:**
- Waiting for available resources (nodes, GPUs)
- Lower priority than other jobs
- Dependency on other jobs
- Partition limits reached
- Account resource limits

**Expected behavior:**
- Status updates every refresh interval
- May transition to Running when resources available
- Check **Elapsed** column for queue time

**Troubleshooting long pending times:**
1. Check partition availability: `sinfo -p <partition>`
2. View queue position: `squeue -u $USER -p <partition>`
3. Check account limits: `sacctmgr show assoc where user=$USER`
4. Reduce resource requests if too high
5. Consider different partition with more availability

#### Running (R)

**Job is actively executing on allocated nodes.**

**Information displayed:**
- **Nodes**: Number and names of allocated nodes
- **Elapsed**: Time since job started
- **Remaining**: Time until time limit expires

**Expected behavior:**
- Elapsed time increases with each refresh
- Can view live output file
- Can cancel if needed

**Monitoring execution:**
- Click **View Output** to see stdout/stderr
- Output file updates in real-time
- Look for profiling progress messages
- Check for error messages indicating issues

#### Completed (CD)

**Job finished successfully.**

**Post-completion:**
- Output files available for review
- Trace files ready for loading
- AI analysis JSON generated (if enabled)
- Results can be opened directly from monitor

**Actions available:**
- **View Output**: Review job stdout/stderr
- **View Results**: Load trace and AI analysis into ROCm Optiq
- **Remove from List**: Clear from monitor (files remain)

#### Failed (F)

**Job terminated with non-zero exit code.**

**Common failure reasons:**
- Application crashed
- Out of memory
- Invalid arguments
- Missing input files
- Profiling tool errors
- Permission issues

**Troubleshooting:**
1. Click **View Output** to see error messages
2. Check `.err` file for stderr output
3. Verify application path exists on cluster
4. Check ROCm module loaded correctly
5. Verify output directory permissions
6. Review resource allocation (memory, GPUs)
7. Test application manually before profiling

#### Cancelled (CA)

**Job was manually cancelled.**

**Reasons for cancellation:**
- User clicked **Cancel** in Job Queue Monitor
- User ran `scancel <job_id>` manually
- System administrator cancelled job
- Pre-empted by higher priority job

**Post-cancellation:**
- Partial output files may exist
- Trace may be incomplete or corrupted
- No automatic result loading

#### Timeout (TO)

**Job exceeded specified time limit.**

**Common causes:**
- Application runs longer than expected
- Profiling overhead underestimated
- Infinite loop or hang
- I/O bottlenecks slowing execution

**Solutions:**
1. Increase time limit in job submission:
   - Change `--time` parameter
   - Example: `01:00:00` → `02:00:00`
2. Optimize application for shorter runtime
3. Profile smaller workload first
4. Use checkpointing to resume

## Job information columns

### Job table columns

The job queue table displays the following information:

#### Job ID

**SLURM job identifier (numeric).**

- Unique identifier assigned by SLURM
- Use for manual `squeue` or `scancel` commands
- Click to copy to clipboard

**Example:**
```bash
# Check job status manually
squeue -j 1234567

# Cancel job manually
scancel 1234567

# View detailed info
scontrol show job 1234567
```

#### Name

**Job name specified during submission.**

- Set via `--job-name` in SLURM config
- Defaults to `rocprof_job` if not specified
- Helps identify jobs in queue

**Best practices:**
- Use descriptive names (e.g., `profile_matmul_v2`)
- Include version or iteration number
- Keep under 30 characters for table display

#### Partition

**SLURM partition (queue) where job runs.**

- Examples: `gpu`, `compute`, `debug`, `batch`
- Different partitions have different:
  - Resource limits (nodes, GPUs, time)
  - Priority settings
  - Access restrictions
  - Node hardware configurations

**Viewing partition info:**
```bash
# List all partitions
sinfo

# Show partition details
scontrol show partition <partition_name>
```

#### Status

**Current job state (see [Status indicators](#status-indicators)).**

- Updates automatically if auto-refresh enabled
- Click **Refresh Now** for manual update
- Color-coded for visual scanning

#### Nodes

**Number of nodes allocated or requested.**

- Format: `<allocated>/<requested>`
- Example: `2/2` (both nodes allocated)
- Pending jobs show `0/<requested>`

**Node details (hover tooltip):**
- Node names (e.g., `node01, node02`)
- Node features (GPU type, memory)
- Node state (idle, allocated, down)

#### Elapsed

**Time since job started or queued.**

- Format: `HH:MM:SS` or `D-HH:MM:SS` for days
- Pending jobs show queue time
- Running jobs show execution time

**Examples:**
- `00:05:42` = 5 minutes 42 seconds
- `1-04:30:00` = 1 day, 4 hours, 30 minutes

#### Time Limit

**Maximum allowed runtime.**

- Specified with `--time` parameter
- Format: `HH:MM:SS` or `D-HH:MM:SS`
- Job terminates if limit exceeded

**Planning time limits:**
- Add 20-30% buffer for profiling overhead
- Test with small workload first
- Use `--time-min` for flexible scheduling

#### Actions

**Available operations for the job (see [Job actions](#job-actions)).**

## Job actions

### Available actions

Each job has a context menu (click `[···]` button or right-click row) with available actions:

### Cancel job

**Stop a running or pending job.**

**Steps:**
1. Click `[···]` button or right-click job row
2. Select **Cancel**
3. Confirmation dialog appears: "Cancel job 1234567?"
4. Click **Yes** to confirm

**What happens:**
- SLURM sends termination signal to job
- Job status changes to "Cancelled" or "Cancelling"
- Allocated resources are freed
- Partial output files remain
- Job removed from queue within seconds

**Use cases:**
- Wrong configuration detected
- Application hanging or looping
- No longer need results
- Resource allocation error

**Note:** Cannot undo cancellation. Job must be resubmitted to re-run.

### View output

**Open job stdout and stderr files.**

**Steps:**
1. Click `[···]` → **View Output**
2. System default text editor opens output file

**Output files:**
- **stdout**: `<job_name>_<job_id>.out`
- **stderr**: `<job_name>_<job_id>.err` (if separate error file configured)

**What to look for:**
- Profiling progress messages
- Error messages or warnings
- Resource allocation information
- Trace file paths
- AI analysis completion status

**Live output viewing (running jobs):**
```bash
# Tail output file in real-time
tail -f <job_name>_<job_id>.out

# Or use srun
srun --jobid=<job_id> tail -f <output_file>
```

**Example output:**
```
ROCm Profiler v3.0.0
Profiling application: /home/user/myapp
Output directory: /scratch/user/rocprof_output
Starting profiling...
Trace file: trace_1234567.db
Recording kernel events...
Kernel 1: matmul_kernel (duration: 2.34 ms)
Kernel 2: reduce_kernel (duration: 0.87 ms)
...
Profiling complete.
Merging traces...
Merged trace: merged_trace.db
Running AI analysis...
AI Analysis complete: ai_analysis.json
```

### View results

**Load trace and AI analysis into ROCm Optiq.**

**Available when:** Job status is "Completed"

**Steps:**
1. Click `[···]` → **View Results**
2. ROCm Optiq retrieves result files from cluster:
   - Merged trace file (`merged_trace.db`)
   - AI analysis JSON (`ai_analysis.json`, if enabled)
3. Files are copied to local machine (if remote)
4. Trace loads in new tab
5. AI Analysis tab opens automatically (if JSON exists)

**What happens:**
- New timeline tab created
- Trace visualized with full features
- AI recommendations displayed
- Job marked as "viewed" in queue monitor
- Files remain on cluster (not deleted)

**File locations:**
- **Local jobs**: Files in specified output directory
- **Remote jobs**: Files copied to `~/.roc-optiq/remote_results/<job_id>/`

**Troubleshooting:**
- **"Trace file not found"**: Check output file for errors, ensure profiling completed
- **"Failed to copy files"**: Check SSH connection, permissions on remote system
- **"Corrupted trace"**: Application may have crashed, check stderr

### Remove from list

**Remove job from queue monitor (files preserved).**

**Steps:**
1. Click `[···]` → **Remove from List**
2. Job disappears from table immediately

**What happens:**
- Job removed from monitoring list
- Output files remain on disk
- SLURM job record unchanged
- Can re-add by resubmitting job or manually adding job ID

**Use cases:**
- Completed jobs no longer needed in monitor
- Declutter queue display
- Focus on active jobs

**Note:** Does not cancel the job. To stop execution, use **Cancel** action.

## Auto-refresh settings

### Enabling auto-refresh

**Automatic status updates at regular intervals.**

1. Check **Auto-refresh** checkbox in Job Queue Monitor
2. Status updates automatically every N seconds
3. Uncheck to disable (manual refresh only)

**Default:** Enabled with 5-second interval

**Benefits:**
- Real-time job status tracking
- Immediate notification of completion
- No manual intervention required
- Detect failures quickly

**Considerations:**
- Frequent `squeue` commands (may impact cluster)
- Network traffic for remote monitoring
- CPU usage for status parsing

### Setting refresh interval

**Control frequency of automatic updates.**

1. Use **Interval** slider or input field
2. Range: 1-60 seconds
3. Changes apply immediately

**Recommended intervals:**

| Scenario | Interval | Reason |
| -------- | -------- | ------ |
| **Quick debug jobs** | 1-2 seconds | Rapid feedback for short jobs |
| **Standard profiling** | 5-10 seconds | Balance responsiveness and load |
| **Long-running jobs** | 30-60 seconds | Reduce cluster load, conserve network |
| **Many jobs queued** | 10-15 seconds | Avoid overwhelming `squeue` |

**Best practices:**
- Use shorter intervals for critical jobs
- Increase interval when monitoring many jobs
- Disable auto-refresh when queue monitor not visible
- Respect cluster policies on `squeue` frequency

### Manual refresh

**Force immediate status update.**

1. Click **Refresh Now** button
2. All job statuses update immediately
3. Timestamp shows last update time

**When to use:**
- Auto-refresh disabled
- Want immediate update before auto-refresh triggers
- Verifying status change

## Job details panel

### Accessing detailed information

**View comprehensive job information.**

1. Click on a job row in the table
2. Details panel expands below table
3. Shows complete job configuration and status

### Information displayed

#### Resource allocation

```
Nodes:           2
CPUs per task:   8
GPUs:            2 (GPU type: MI250X)
Memory:          64 GB per node
```

#### Time information

```
Submitted:       2026-02-19 10:30:42
Started:         2026-02-19 10:32:15
Elapsed:         00:15:32
Time limit:      01:00:00
Remaining:       00:44:28
```

#### Node assignment

```
Allocated nodes: node01, node04
Node features:   GPU, high-memory, infiniband
```

#### Job configuration

```
Partition:       gpu
Account:         research_proj
QOS:             normal
Working dir:     /scratch/user/profiling/job_1234567
```

#### Output files

```
Stdout:          rocprof_job_1234567.out
Stderr:          rocprof_job_1234567.err
Trace file:      /scratch/user/profiling/job_1234567/merged_trace.db
AI analysis:     /scratch/user/profiling/job_1234567/ai_analysis.json
```

#### Exit information (completed jobs)

```
Exit code:       0
Exit status:     Success
Signal:          None
```

### Copying information

**Click any field to copy to clipboard:**
- Job ID → Copy for manual commands
- Node names → SSH to specific node
- File paths → Navigate directly

## Managing multiple jobs

### Bulk operations

**Perform actions on multiple jobs simultaneously.**

#### Selecting multiple jobs

1. Hold `Ctrl` (Windows/Linux) or `Cmd` (macOS)
2. Click job rows to select
3. Selected jobs highlighted

#### Bulk actions

- **Cancel Selected**: Cancel all selected jobs
- **Remove Selected**: Remove from monitor
- **Export Selected**: Save job information to CSV

### Filtering jobs

**Show only jobs matching criteria.**

1. Click **Filter** button above job table
2. Choose filter options:
   - **Status**: Show only specific statuses
   - **Partition**: Filter by partition
   - **Date range**: Jobs submitted within date range
   - **Name pattern**: Regex or wildcard match

**Example filters:**
- Show only running jobs
- Show only failed jobs in last 24 hours
- Show jobs in "gpu" partition

### Sorting

**Order jobs by any column.**

1. Click column header to sort
2. Click again to reverse order
3. Arrow indicator shows sort direction

**Common sorting:**
- **By elapsed time**: Find longest-running jobs
- **By job ID**: Chronological submission order
- **By status**: Group by state

## Persistent job tracking

### Automatic persistence

**Jobs tracked across application restarts.**

- Job list saved to disk automatically
- Reopening ROCm Optiq restores job monitor
- Historical jobs retained for 30 days (configurable)

### Storage location

**Job tracking database:**
- **Linux/macOS**: `~/.local/share/roc-optiq/jobs.db`
- **Windows**: `%LOCALAPPDATA%\roc-optiq\jobs.db`

### Retention settings

1. Go to **Settings** → **Job Monitoring**
2. Configure **Job History Retention**:
   - Keep completed jobs: 7, 14, 30, 90 days, or forever
   - Keep failed jobs: Separate retention period
3. Click **Clean Up Old Jobs** to manually purge

## Remote job monitoring

### SSH-based monitoring

**Monitor jobs on remote clusters.**

When profiling jobs submitted via SSH remote execution mode, monitoring works seamlessly:

1. Job submitted to remote SLURM cluster
2. ROCm Optiq polls remote `squeue` via SSH
3. Status updates appear in local Job Queue Monitor
4. Output files accessed via SSH
5. Results copied back automatically when complete

### SSH connection management

**Persistent SSH connection for monitoring:**

- Single SSH session reused for status queries
- Connection re-established if dropped
- Authentication via SSH keys (no password prompts)

**Configuration:**
- Same SSH settings as profiling dialog
- ProxyJump/bastion host supported
- Custom SSH options respected

### Bandwidth considerations

**Monitoring network usage:**

- Status query: ~1-2 KB per refresh
- Output file viewing: Varies by file size
- Result retrieval: Depends on trace size

**Optimization:**
- Increase refresh interval for slow connections
- Compress results before copying (automatic)
- Use SSH connection multiplexing

## Troubleshooting

### Jobs not appearing in monitor

**Symptoms:** Submitted job doesn't appear in queue

**Solutions:**
1. Check SLURM job actually submitted: `squeue -u $USER`
2. Verify job ID captured in profiling dialog output
3. Manually add job: **Add Job** button → Enter job ID
4. Check `squeue` command available in PATH
5. Verify SSH connection for remote jobs

### Status not updating

**Symptoms:** Job status stuck on old value

**Solutions:**
1. Click **Refresh Now** to force update
2. Check auto-refresh is enabled
3. Verify `squeue` command works manually
4. Check SSH connection for remote jobs
5. Review ROCm Optiq logs for errors
6. Restart Job Queue Monitor window

### "Permission denied" errors

**Symptoms:** Cannot query job status or cancel job

**Solutions:**
1. Verify SLURM account active: `sacctmgr show user $USER`
2. Check job belongs to your user: `squeue -j <job_id>`
3. Cannot cancel jobs owned by other users
4. Verify SSH key has correct permissions (remote jobs)
5. Check SLURM configuration allows user job queries

### Output files not found

**Symptoms:** "View Output" fails with file not found error

**Solutions:**
1. Check output file path in job details
2. Verify file exists on cluster: `ls <output_file>`
3. Check output directory permissions
4. Ensure job has started writing output
5. For remote jobs, verify SSH connection and file access

### Results fail to load

**Symptoms:** "View Results" fails or loads corrupted trace

**Solutions:**
1. Check job completed successfully (exit code 0)
2. Verify trace file exists: `ls <trace_file>`
3. Check trace file not empty: `ls -lh <trace_file>`
4. Ensure rocpd merge completed (check output file)
5. Verify AI analysis JSON valid (if enabled)
6. Manually open trace file to test: `File → Open`

## Best practices

### Job naming conventions

**Use systematic job names for easy identification:**

```
<project>_<app>_<config>_<date>
```

**Examples:**
- `ml_training_resnet_fp16_20260219`
- `benchmark_gemm_v2_large`
- `debug_kernel_opt3`

**Benefits:**
- Easy to identify in queue
- Sortable chronologically
- Filter by project or application
- Descriptive for team collaboration

### Monitoring workflow

**Efficient job monitoring routine:**

1. **Submit job**: Use descriptive name, appropriate resources
2. **Initial check**: Verify job appears in queue (1-2 minutes)
3. **Monitor pending**: Check queue position if stuck pending
4. **Watch running**: Set auto-refresh to 5-10 seconds
5. **Review output**: Periodically check output file for errors
6. **Load results**: Use "View Results" when completed
7. **Clean up**: Remove old jobs from monitor weekly

### Resource optimization

**Minimize cluster load from monitoring:**

- Use longer refresh intervals (10-15 seconds minimum)
- Monitor only active jobs (remove completed)
- Disable auto-refresh when not actively watching
- Batch multiple job status queries
- Use job arrays instead of many individual jobs

### Collaboration

**Team job monitoring:**

- Share job IDs with team members
- Use consistent naming conventions
- Document resource allocations
- Export job configurations as templates
- Review failed jobs as team for troubleshooting

## Advanced features

### Job dependencies

**Chain profiling jobs with dependencies.**

When submitting jobs via manual SLURM scripts:

```bash
# Job 1: Profile application
JOB1=$(sbatch --parsable job1.sh)

# Job 2: Analyze results (depends on Job 1)
JOB2=$(sbatch --dependency=afterok:$JOB1 analyze.sh)
```

Both jobs appear in ROCm Optiq Job Queue Monitor with dependency relationship shown.

### Job arrays

**Submit multiple profiling configurations as job array.**

```bash
#!/bin/bash
#SBATCH --job-name=rocprof_array
#SBATCH --array=1-10

# Profile with different inputs
rocprofv3 --sys-trace /path/to/app input_${SLURM_ARRAY_TASK_ID}.dat
```

Each array task appears as separate job in monitor with array notation (e.g., `1234567_1`, `1234567_2`).

### Custom job metrics

**Track additional metrics beyond SLURM status.**

Future enhancement (planned):
- GPU utilization during job
- Memory usage timeline
- I/O statistics
- Energy consumption
- Queue time analytics

## Integration with profiling workflow

### End-to-end workflow

```
Submit SLURM Job
    ↓
Job Queue Monitor opens automatically
    ↓
Job status: Pending → Running
    ↓
Monitor progress via "View Output"
    ↓
Job completes successfully
    ↓
Click "View Results"
    ↓
Trace + AI analysis load automatically
    ↓
Analyze results in timeline view
    ↓
Remove job from monitor (optional)
```

### Automation hooks

**Planned feature:** Trigger actions on job completion

- Send notification email
- Auto-archive results
- Launch comparison view
- Execute post-processing scripts

## Related topics

- [Profiling workflow](profiling-workflow.md)
- [SLURM profiling](profiling-workflow.md#slurm-cluster-profiling)
- [Settings](settings.md)

---

*For issues or feature requests, visit [GitHub Issues](https://github.com/ROCm/roc-optiq/issues)*
