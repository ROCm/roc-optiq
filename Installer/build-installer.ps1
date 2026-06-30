<#
.SYNOPSIS
    Build the roc-optiq InstallShield installer locally.

.DESCRIPTION
    Drives the InstallShield 2024 (ISWiAuto29) build pipeline without requiring
    an external build service. Runs version/copyright update scripts, optionally
    populates installer components from a staged release folder, then calls
    ISCmdBld.exe to produce the MSI.

.PARAMETER Version
    Product version string (e.g. "0.5.0.0"). Defaults to the version in
    CMakeLists.txt if not provided.

.PARAMETER ProductName
    Product name written into the installer. Defaults to "ROCm Optiq".

.PARAMETER ReleaseFolder
    Path to a staged release folder containing ReleaseInternal/, ReleaseNDA/,
    ReleaseCommon/, and/or ReleasePublic/ sub-directories. When provided,
    InstallerAutomation.vbs is run to add those files as installer components.
    Skip this parameter to build the installer with whatever components are
    already saved in the ISM.

.PARAMETER ReleaseType
    InstallShield release configuration to build (e.g. "Internal", "NDA",
    "Public"). Defaults to "Internal".

.PARAMETER InstallShieldPath
    Full path to the InstallShield installation directory containing
    ISCmdBld.exe. Auto-detected from the default Program Files location when
    not specified.

.PARAMETER OutputDir
    Directory where the built MSI is copied after a successful build. Defaults
    to "Installer\Output" relative to the repo root.

.EXAMPLE
    # Minimal local build (uses ISM components as-is, version from CMakeLists.txt)
    .\Installer\build-installer.ps1

.EXAMPLE
    # Full build with staged files and explicit version
    .\Installer\build-installer.ps1 -Version "0.5.1.0" -ReleaseFolder "C:\stage" -ReleaseType "Internal"
#>

[CmdletBinding()]
param(
    [string]$Version,
    [string]$ProductName = "ROCm Optiq",
    [string]$ReleaseFolder,
    [ValidateSet("Internal", "NDA", "Public")]
    [string]$ReleaseType = "Internal",
    [string]$InstallShieldPath,
    [string]$OutputDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Resolve paths relative to the repo root (script lives in Installer/)
# ---------------------------------------------------------------------------
$ScriptDir  = $PSScriptRoot
$RepoRoot   = (Resolve-Path (Join-Path $ScriptDir "..")).Path
$ScriptsDir = Join-Path $ScriptDir "Installation\Scripts"
$IsmFile    = Join-Path $ScriptsDir "setup.ism"

if (-not (Test-Path $IsmFile)) {
    Write-Error "ISM file not found at: $IsmFile"
    exit 1
}

if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot "Installer\Output"
}

# ---------------------------------------------------------------------------
# Read version from CMakeLists.txt when not explicitly provided
# ---------------------------------------------------------------------------
if (-not $Version) {
    $CmakeLists = Join-Path $RepoRoot "CMakeLists.txt"
    $major = (Select-String -Path $CmakeLists -Pattern 'set\(PROJECT_VERSION_MAJOR\s+(\d+)\)').Matches[0].Groups[1].Value
    $minor = (Select-String -Path $CmakeLists -Pattern 'set\(PROJECT_VERSION_MINOR\s+(\d+)\)').Matches[0].Groups[1].Value
    $patch = (Select-String -Path $CmakeLists -Pattern 'set\(PROJECT_VERSION_PATCH\s+(\d+)\)').Matches[0].Groups[1].Value
    $build = (Select-String -Path $CmakeLists -Pattern 'set\(PROJECT_VERSION_BUILD\s+(\d+)\)').Matches[0].Groups[1].Value
    $Version = "$major.$minor.$patch.$build"
    Write-Host "Version read from CMakeLists.txt: $Version"
}

# ---------------------------------------------------------------------------
# Locate ISCmdBld.exe
# ---------------------------------------------------------------------------
if (-not $InstallShieldPath) {
    $candidates = @(
        "${env:ProgramFiles(x86)}\InstallShield\2024\System",
        "${env:ProgramFiles}\InstallShield\2024\System",
        "${env:ProgramFiles(x86)}\InstallShield\2023\System",
        "${env:ProgramFiles}\InstallShield\2023\System"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path (Join-Path $candidate "ISCmdBld.exe")) {
            $InstallShieldPath = $candidate
            break
        }
    }
}

if (-not $InstallShieldPath) {
    Write-Error @"
ISCmdBld.exe not found. Install InstallShield 2024 (or 2023) and either:
  - Ensure it is installed to the default Program Files location, OR
  - Pass -InstallShieldPath to the full path of the System directory
    (e.g. "C:\Program Files (x86)\InstallShield\2024\System")
"@
    exit 1
}

$IsCmdBld = Join-Path $InstallShieldPath "ISCmdBld.exe"
Write-Host "Using ISCmdBld.exe: $IsCmdBld"

# ---------------------------------------------------------------------------
# Update copyright year
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Updating copyright year ==="
$CopyrightYear = (Get-Date).Year.ToString()
& cscript.exe //nologo (Join-Path $ScriptsDir "Update_Copyright.vbs") $CopyrightYear
if ($LASTEXITCODE -ne 0) { Write-Error "Update_Copyright.vbs failed (exit $LASTEXITCODE)"; exit 1 }

# ---------------------------------------------------------------------------
# Update product version, product code, and product name
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Updating version to $Version ==="
$env:PKG_VERSION   = $Version
$env:SCRIPT_NAME   = $IsmFile
$env:PROJECT_NAME  = $ProductName
& cscript.exe //nologo (Join-Path $ScriptsDir "Update_Version.vbs")
if ($LASTEXITCODE -ne 0) { Write-Error "Update_Version.vbs failed (exit $LASTEXITCODE)"; exit 1 }

# ---------------------------------------------------------------------------
# Populate installer components from staged release folder (optional)
# ---------------------------------------------------------------------------
if ($ReleaseFolder) {
    if (-not (Test-Path $ReleaseFolder)) {
        Write-Error "ReleaseFolder does not exist: $ReleaseFolder"
        exit 1
    }
    Write-Host ""
    Write-Host "=== Adding components from release folder: $ReleaseFolder ==="
    $env:ISM_FILE_PATH = $IsmFile
    $env:ROOTFOLDER    = $ReleaseFolder
    & cscript.exe //nologo (Join-Path $ScriptsDir "InstallerAutomation.vbs")
    if ($LASTEXITCODE -ne 0) { Write-Error "InstallerAutomation.vbs failed (exit $LASTEXITCODE)"; exit 1 }
}

# ---------------------------------------------------------------------------
# Build the installer with ISCmdBld.exe
#
# Flags used:
#   -p  <ism file>         Project file
#   -r  <release name>     Release configuration to build (Internal/NDA/Public)
#   -b  <build output>     Override build output root
#   -o  <log file>         Build log path
# ---------------------------------------------------------------------------
$BuildOutput = Join-Path $RepoRoot "Installer\ISBuild"
$BuildLog    = Join-Path $BuildOutput "ISCmdBld.log"
$null = New-Item -ItemType Directory -Force -Path $BuildOutput

Write-Host ""
Write-Host "=== Building installer (release: $ReleaseType) ==="
Write-Host "    Output dir : $BuildOutput"
Write-Host "    Log        : $BuildLog"

& $IsCmdBld -p $IsmFile -r $ReleaseType -b $BuildOutput -o $BuildLog
$buildExit = $LASTEXITCODE

# Always show the last 30 lines of the build log so failures are visible
if (Test-Path $BuildLog) {
    Write-Host ""
    Write-Host "--- ISCmdBld log (tail) ---"
    Get-Content $BuildLog -Tail 30 | Write-Host
}

if ($buildExit -ne 0) {
    Write-Error "ISCmdBld.exe exited with code $buildExit. See log: $BuildLog"
    exit 1
}

# ---------------------------------------------------------------------------
# Collect the produced MSI and copy to OutputDir
# ---------------------------------------------------------------------------
$MsiFiles = Get-ChildItem -Path $BuildOutput -Filter "*.msi" -Recurse
if (-not $MsiFiles) {
    Write-Error "Build reported success but no MSI was found under: $BuildOutput"
    exit 1
}

$null = New-Item -ItemType Directory -Force -Path $OutputDir
foreach ($msi in $MsiFiles) {
    $dest = Join-Path $OutputDir $msi.Name
    Copy-Item -Path $msi.FullName -Destination $dest -Force
    Write-Host "MSI copied to: $dest"
}

Write-Host ""
Write-Host "=== Build complete ==="
