$ErrorActionPreference = 'Stop'
Write-Host 'PITCHFORGE SOURCE AUDIT v4.1.7'

$required = @(
    'CMakeLists.txt',
    'src/PluginProcessor.h',
    'src/PluginProcessor.cpp',
    'src/PluginEditor.h',
    'src/PluginEditor.cpp',
    '.github/workflows/build-vst3.yml'
)
foreach ($f in $required) {
    if (-not (Test-Path $f -PathType Leaf)) { throw "Missing required file: $f" }
}

$cm = Get-Content CMakeLists.txt -Raw
$ps = Get-Content src/PluginProcessor.cpp -Raw
$ed = Get-Content src/PluginEditor.cpp -Raw
$wf = Get-Content .github/workflows/build-vst3.yml -Raw

# Only inspect the body of processBlock for realtime-allocation checks.
$m = [regex]::Match(
    $ps,
    '(?s)void\s+PitchForgeAudioProcessor::processBlock\s*\([^)]*\)\s*\{(?<body>.*?)\n\}\s*\n\s*void\s+PitchForgeAudioProcessor::getStateInformation'
)
if (-not $m.Success) { throw 'Could not isolate processBlock for realtime-safety audit.' }
$processBody = $m.Groups['body'].Value

# Normalize whitespace so CRLF/LF differences cannot create a false failure.
$wfNormalized = [regex]::Replace($wf, '\s+', ' ').Trim()

$checks = @(
    [pscustomobject]@{ Pass = ($ps -notmatch 'soundtouch::SETTING_'); Name = 'SoundTouch setting macros are not namespace-qualified' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_USE_AA_FILTER,\s*1\)'); Name = 'SoundTouch AA filter setting is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_AA_FILTER_LENGTH,\s*64\)'); Name = 'SoundTouch AA filter length is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_USE_QUICKSEEK,\s*0\)'); Name = 'SoundTouch quickseek is explicitly disabled' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_SEQUENCE_MS,\s*sequenceMs\)'); Name = 'SoundTouch sequence setting is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_SEEKWINDOW_MS,\s*seekMs\)'); Name = 'SoundTouch seek-window setting is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_OVERLAP_MS,\s*overlapMs\)'); Name = 'SoundTouch overlap setting is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.getSetting\(SETTING_INITIAL_LATENCY\)'); Name = 'SoundTouch initial latency query is present' },
    [pscustomobject]@{ Pass = ($ed -notmatch 'textToValueFunction'); Name = 'JUCE Slider parser uses valueFromTextFunction' },
    [pscustomobject]@{ Pass = ($ed -match 'PathStrokeType\(4\.0f,\s*juce::PathStrokeType::curved,\s*juce::PathStrokeType::rounded\)'); Name = 'Needle stroke constructor uses valid JUCE signature' },
    [pscustomobject]@{ Pass = ($ps -notmatch 'processOut\[\(size_t\)i \* 2\] = 0\.0f'); Name = 'No hard-zero wet-output fallback remains' },
    [pscustomobject]@{ Pass = ($ps -match 'processOut\[\(size_t\)i \* 2\] = processIn\[\(size_t\)i \* 2\]'); Name = 'Wet underflow uses continuity fallback' },
    [pscustomobject]@{ Pass = ($ps -notmatch 'SmoothPitchShifter'); Name = 'Legacy granular shifter is absent' },
    [pscustomobject]@{ Pass = ($processBody -notmatch '(?i)(new\s+\w+|delete\s+|std::malloc|std::free|\.resize\s*\(|\.assign\s*\(|std::vector\s*<[^>]+>\s+\w+\s*[;=])'); Name = 'No obvious heap-allocation operations are introduced in processBlock' },
    [pscustomobject]@{ Pass = ($ps -match 'const float coefficient = 1\.0f - static_cast<float>\(std::exp\('); Name = 'Speed coefficient resolves exp result to float before jlimit' },
    [pscustomobject]@{ Pass = ($ps -match 'juce::jlimit\(-range, range, rawSemi\)'); Name = 'Correction range jlimit is type-consistent' },
    [pscustomobject]@{ Pass = ($wfNormalized -match 'cmake --build build --config Release --parallel 2(?: |$)'); Name = 'Windows build command is canonical' },
    [pscustomobject]@{ Pass = ($wfNormalized -notmatch 'cmake --build build --config Release --parallel 2 2'); Name = 'Malformed duplicate parallel argument is absent' },
    [pscustomobject]@{ Pass = ($cm -match 'VST3_CAN_REPLACE_VST2\s+FALSE'); Name = 'VST2 replacement guard is present' },
    [pscustomobject]@{ Pass = ($cm -match 'soundtouch-2\.3\.3'); Name = 'SoundTouch 2.3.3 is pinned' },
    [pscustomobject]@{ Pass = ($cm -match 'DOWNLOAD_EXTRACT_TIMESTAMP\s+TRUE'); Name = 'Deterministic archive extraction is requested' },
    [pscustomobject]@{ Pass = ($cm -match 'target_compile_definitions\(PitchForge_VST3 PRIVATE[^\n]*SOUNDTOUCH_FLOAT_SAMPLES=1'); Name = 'SoundTouch float sample mode is enabled for VST3 target' }
)

$failed = @($checks | Where-Object { -not $_.Pass })
foreach ($c in $checks) {
    if ($c.Pass) { Write-Host "PASS: $($c.Name)" }
    else { Write-Host "FAIL: $($c.Name)" }
}
if ($failed.Count -gt 0) {
    throw ("SOURCE AUDIT FAILED: {0} check(s) failed." -f $failed.Count)
}
Write-Host 'STATIC AUDIT: PASS'
