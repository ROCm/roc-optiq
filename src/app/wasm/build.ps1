# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT
#
# Convenience script to build the Emscripten / WebAssembly target.
#
# Usage:
#     pwsh -NoProfile -ExecutionPolicy Bypass -File src/app/wasm/build.ps1
#
# Output goes to:    <repo>/build-wasm/index.{html,js,wasm}

param(
    [string]$BuildDir = "$PSScriptRoot/../../../build-wasm",
    [string]$EmsdkDir = "$env:USERPROFILE/emsdk",
    [string]$Config   = "Release"
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $EmsdkDir)) {
    throw "Emscripten SDK not found at '$EmsdkDir'. Set -EmsdkDir or install via: git clone https://github.com/emscripten-core/emsdk $EmsdkDir; cd $EmsdkDir; ./emsdk install latest; ./emsdk activate latest"
}

$envScript = Join-Path $EmsdkDir 'emsdk_env.ps1'
if (-not (Test-Path $envScript)) {
    throw "emsdk_env.ps1 not found at '$envScript'."
}

Write-Host "Sourcing Emscripten SDK from $EmsdkDir"
& $envScript | Out-Null

# Make sure a `ninja` binary is on PATH. Prefer the existing one; otherwise
# fall back to the pip-installed `ninja` Python package, whose console
# script lands in the user/sys "Scripts" directory.
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    foreach ($scheme in @('nt_user', 'nt')) {
        try {
            $scriptsDir = & python -c "import sysconfig; print(sysconfig.get_path('scripts', '$scheme'))" 2>$null
        } catch {
            $scriptsDir = $null
        }
        if ($scriptsDir -and (Test-Path (Join-Path $scriptsDir 'ninja.exe'))) {
            $env:PATH = "$scriptsDir;$env:PATH"
            Write-Host "Added ninja to PATH from $scriptsDir"
            break
        }
    }
}
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw "ninja not found on PATH. Install with: python -m pip install --user ninja"
}

$BuildDir = (Resolve-Path -LiteralPath (New-Item -ItemType Directory -Force -Path $BuildDir)).Path
Write-Host "Build directory: $BuildDir"

Push-Location $BuildDir
try {
    $cmakeArgs = @('cmake', '-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Config", "$PSScriptRoot")
    Write-Host ("emcmake " + ($cmakeArgs -join ' '))
    & emcmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "emcmake cmake failed" }

    & cmake --build . --config $Config
    if ($LASTEXITCODE -ne 0) { throw "cmake --build failed" }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Build OK. Artifacts:"
Get-ChildItem -LiteralPath $BuildDir -Filter 'index.*' | ForEach-Object { Write-Host "  $($_.FullName)" }
