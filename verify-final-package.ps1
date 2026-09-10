$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path $PSScriptRoot).Path
Set-Location $root
Write-Host 'PITCHFORGE v4.6.1 FINAL PACKAGE PREFLIGHT'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'verify-package.ps1')
if ($LASTEXITCODE -ne 0) { throw "verify-package.ps1 failed with exit code $LASTEXITCODE." }
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root 'audit-build.ps1')
if ($LASTEXITCODE -ne 0) { throw "audit-build.ps1 failed with exit code $LASTEXITCODE." }
Write-Host 'FINAL PACKAGE PREFLIGHT: PASS'
