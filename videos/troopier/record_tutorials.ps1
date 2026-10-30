<#
.SYNOPSIS
Records the ROCm Optiq tutorial chapters.

.DESCRIPTION
Runs roc-optiq-tests.exe in --record-tutorials mode. Each chapter is an ImGui
Test Engine test that drives the UI in cinematic mode and captures the Optiq
window (1920x1080, 60 fps, lossless, no audio) straight from its back buffer
into raw\, with timing files in raw\timing\.

With -Series systems, it records the ROCm Optiq chapters of the ROCm Systems
Profiler tutorials into raw-systems\, paced by voice-systems\. They open the
runs systems_capture.sh profiled, from copies of the output folders those runs
wrote (systems-capture\optiq\).

Each chapter is paced by its narration: a line starts once the previous one
has finished, using the lengths of the synthesized lines in voice\lines.tsv.
Synthesize the narration first so the pacing matches the voice exactly;
without it, line lengths are estimated.

The run uses a throwaway settings folder so the footage shows default settings
and your own recent files and preferences stay untouched. The sample traces
are copied to a neutral public folder so no personal paths appear on screen,
and the host, user and workload names stored in the copies are replaced with
neutral ones (anonymize_samples.py; needs Python).

Build the executable first, with the features of a release build (the recorder
refuses to run from a developer or experimental build):
  cmake --preset x64-tutorial
  cmake --build --preset "Windows Tutorial Build"

Each chapter reports PASS only when every scripted step ran; the log lines
printed at the end name any step that did not. Earlier recordings of the
chapters being recorded move to raw\previous, so a chapter that fails leaves no
stale footage behind. The recorder keeps the session awake and unlocked while it
runs. Do not click into the Optiq window while it records.

.EXAMPLE
.\record_tutorials.ps1
.\record_tutorials.ps1 -Chapters "03_"
.\record_tutorials.ps1 -Series systems -Chapters "sc_,sk_,sp_"
#>
param(
    [ValidateSet("optiq", "systems")]
    [string]$Series = "optiq",
    [string]$Chapters = "",
    [string]$OutputDir = "",
    [string]$Narration = "",
    [double]$UiScale = 1.25,
    [string]$Exe = "",
    [string]$Ffmpeg = ""
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$suffix = if ($Series -eq "systems") { "-systems" } else { "" }
if (-not $OutputDir) { $OutputDir = Join-Path $PSScriptRoot "raw$suffix" }
if (-not $Narration) { $Narration = Join-Path $PSScriptRoot "voice$suffix" }

if (-not $Exe) {
    $Exe = Join-Path $repo "build\x64-tutorial\Release\roc-optiq-tests.exe"
}
if (-not (Test-Path $Exe)) {
    throw "Test executable not found: $Exe (see the build steps in this script's help)"
}

if (-not $Ffmpeg) {
    $cmd = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($cmd) {
        $Ffmpeg = $cmd.Source
    } else {
        $found = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse -Filter ffmpeg.exe -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($found) { $Ffmpeg = $found.FullName }
    }
}
if (-not $Ffmpeg -or -not (Test-Path $Ffmpeg)) {
    throw "ffmpeg not found. Install it (winget install Gyan.FFmpeg) or pass -Ffmpeg."
}

$samplesName = if ($Series -eq "systems") { "Profiles" } else { "ROCm Optiq Samples" }
$samples = Join-Path ([Environment]::GetFolderPath("CommonDocuments")) $samplesName
if (Test-Path $samples) { Remove-Item $samples -Recurse -Force }
New-Item -ItemType Directory -Path $samples | Out-Null
if ($Series -eq "systems") {
    # Each run's output folder, as a user copies it next to the others.
    $runs = Join-Path $PSScriptRoot "systems-capture\optiq"
    if (-not (Test-Path $runs)) {
        throw "No captured runs in $runs (see systems_capture.sh)"
    }
    Get-ChildItem $runs -Directory | ForEach-Object {
        Get-ChildItem $_.FullName -Directory | ForEach-Object { Copy-Item $_.FullName $samples -Recurse }
    }
} else {
    foreach ($name in @("rocpd-transpose.db", "rocprof_compute_23ed6f36.db")) {
        Copy-Item (Join-Path $repo "sample\$name") $samples
    }
}
# The samples name the machines and people that captured them; the copies get
# neutral names so none appear on screen.
if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 (Join-Path $PSScriptRoot "anonymize_samples.py") (Get-ChildItem $samples -Filter *.db -Recurse).FullName
} else {
    Write-Warning "Python not found; the footage will show the host and user names stored in the samples."
}

$settings = Join-Path ([IO.Path]::GetTempPath()) "optiq-tutorial-settings"
if (Test-Path $settings) { Remove-Item $settings -Recurse -Force }
New-Item -ItemType Directory -Path $settings | Out-Null

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
# Chapters that fail before recording write nothing, so their earlier footage
# would otherwise pass for new.
$previous = Join-Path $OutputDir "previous"
$earlier = @()
foreach ($part in ($Chapters -split ",")) {
    $part = $part.Trim()
    $earlier += @(Get-ChildItem $OutputDir -Filter "*$part*.mkv" -File -ErrorAction SilentlyContinue)
    $earlier += @(Get-ChildItem (Join-Path $OutputDir "timing") -Filter "*$part*" -File -ErrorAction SilentlyContinue)
}
$earlier = @($earlier | Sort-Object FullName -Unique)
if ($earlier.Count -gt 0) {
    New-Item -ItemType Directory -Force -Path (Join-Path $previous "timing") | Out-Null
    foreach ($file in $earlier) {
        $into = if ($file.Directory.Name -eq "timing") { Join-Path $previous "timing" } else { $previous }
        Move-Item $file.FullName $into -Force
    }
    Write-Host "Moved $($earlier.Count) earlier recording files to $previous"
}
$arguments = @("--record-tutorials", "`"$OutputDir`"", "--ffmpeg", "`"$Ffmpeg`"",
               "--ui-scale", $UiScale.ToString([Globalization.CultureInfo]::InvariantCulture))
if ($Chapters) { $arguments += @("--chapters", "`"$Chapters`"") }
if (Test-Path (Join-Path $Narration "lines.tsv")) {
    $arguments += @("--narration", "`"$Narration`"")
} else {
    Write-Warning "No synthesized narration in $Narration; line lengths will be estimated."
}

$savedLocalAppData = $env:LOCALAPPDATA
try {
    $env:LOCALAPPDATA = $settings
    # In a console window of its own, the recorder has crashed at startup inside
    # Windows' CoreMessaging.dll; it runs fine attached to this console.
    $process = Start-Process -FilePath $Exe -ArgumentList $arguments -WorkingDirectory $samples -NoNewWindow -PassThru -Wait
} finally {
    $env:LOCALAPPDATA = $savedLocalAppData
}

$log = Join-Path $settings "AMD\ROCm-Optiq\roc-optiq.log"
if (Test-Path $log) {
    Get-Content $log | Select-String -Pattern "\[tutorial" | ForEach-Object { $_.Line }
}
Get-ChildItem $OutputDir -Filter *.mkv | Sort-Object Name | ForEach-Object {
    "{0,-45} {1,8:N1} MB" -f $_.Name, ($_.Length / 1MB)
}
if ($process.ExitCode -ne 0) {
    Write-Warning "Recorder exited with code $($process.ExitCode); check the log above."
}
