$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path $PSScriptRoot).Path
$required = @(
  'CMakeLists.txt',
  'src\PluginProcessor.h',
  'src\PluginProcessor.cpp',
  'src\PluginEditor.h',
  'src\PluginEditor.cpp',
  '.github\workflows\build-vst3.yml'
)
foreach ($r in $required) {
  if (-not (Test-Path (Join-Path $root $r))) { throw "Missing required file: $r" }
}
$cm = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($cm -match '`r`n|`n|`r') { throw 'CMakeLists.txt contains literal PowerShell escape sequences.' }
if ($cm -notmatch 'VST3_CAN_REPLACE_VST2\s+FALSE') { throw 'VST3_CAN_REPLACE_VST2 FALSE missing.' }
$cpp = Get-Content (Join-Path $root 'src\PluginProcessor.cpp') -Raw
if ($cpp -match 'process\([^\)]*\).*process\(') { throw 'Suspicious nested shifter process pattern detected.' }
Write-Host 'PACKAGE PREFLIGHT: PASS' -ForegroundColor Green
Write-Host "Source root: $root"
