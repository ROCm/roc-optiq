<#
.SYNOPSIS
Records the ROCm Optiq tutorial chapters.

.DESCRIPTION
Runs roc-optiq-tests.exe in --record-tutorials mode. Each chapter is an ImGui
Test Engine test that drives the UI in cinematic mode and captures the Optiq
window (1920x1080, 60 fps, lossless, no audio) straight from its back buffer
into raw\, with timing files in raw\timing\.

Each chapter is paced by its narration: a line starts once the previous one
has finished, using the lengths of the synthesized lines in voice\lines.tsv.
Synthesize the narration first so the pacing matches the voice exactly;
without it, line lengths are estimated.

The run uses a throwaway settings folder so the footage shows default settings
and your own recent files and preferences stay untouched; it holds the app's
default Memory Chart layout as an override, because the gfx950 layout reads the
wrong metrics for the compute sample. The sample traces
are copied to a neutral public folder so no personal paths appear on screen,
and the host, user and workload names stored in the copies are replaced with
neutral ones (anonymize_samples.py; needs Python).

Build the executable first:
  cmake --preset x64-release -B build/tutorial -DROCPROFVIS_ENABLE_UI_TESTS=ON
  cmake --build build/tutorial --config Release --target roc-optiq-tests

Do not click into the Optiq window while it records.

.EXAMPLE
.\record_tutorials.ps1
.\record_tutorials.ps1 -Chapters "03_"
#>
param(
    [string]$Chapters = "",
    [string]$OutputDir = (Join-Path $PSScriptRoot "raw"),
    [string]$Narration = (Join-Path $PSScriptRoot "voice"),
    [double]$UiScale = 1.25,
    [string]$Exe = "",
    [string]$Ffmpeg = ""
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

if (-not $Exe) {
    $Exe = Join-Path $repo "build\tutorial\Release\roc-optiq-tests.exe"
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

$samples = Join-Path ([Environment]::GetFolderPath("CommonDocuments")) "ROCm Optiq Samples"
if (Test-Path $samples) { Remove-Item $samples -Recurse -Force }
New-Item -ItemType Directory -Path $samples | Out-Null
foreach ($name in @("rocpd-transpose.db", "rocprof_compute_23ed6f36.db")) {
    Copy-Item (Join-Path $repo "sample\$name") $samples
}
# The samples name the machines and people that captured them; the copies get
# neutral names so none appear on screen.
if (Get-Command py -ErrorAction SilentlyContinue) {
    & py -3 (Join-Path $PSScriptRoot "anonymize_samples.py") (Get-ChildItem $samples -Filter *.db).FullName
} else {
    Write-Warning "Python not found; the footage will show the host and user names stored in the samples."
}

$settings = Join-Path ([IO.Path]::GetTempPath()) "optiq-tutorial-settings"
if (Test-Path $settings) { Remove-Item $settings -Recurse -Force }
New-Item -ItemType Directory -Path $settings | Out-Null

# Optiq picks the Memory Chart layout by GPU architecture, and its gfx950 layout
# reads the wrong metrics for the compute sample, which numbers them the way the
# default layout does. The app's default layout goes in as the developer
# override (memory_chart.json in the config folder), which the app loads first.
$layouts = Get-Content (Join-Path $repo "src\view\src\compute\rocprofvis_memory_chart_layouts_generated.h") -Raw
$default = [regex]::Match($layouts, 'kMemChartLayout_default = R"MCJSON\((.*?)\)MCJSON"', 'Singleline')
if ($default.Success) {
    $config = Join-Path $settings "AMD\ROCm-Optiq"
    New-Item -ItemType Directory -Force -Path $config | Out-Null
    [IO.File]::WriteAllText((Join-Path $config "memory_chart.json"), $default.Groups[1].Value,
                            (New-Object Text.UTF8Encoding $false))
} else {
    Write-Warning "Default Memory Chart layout not found; the compute chapters will show wrong values."
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
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
