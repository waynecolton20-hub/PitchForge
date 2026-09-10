$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Write-Host 'PITCHFORGE SOURCE AUDIT v4.3.1'

$required = @(
    'CMakeLists.txt',
    'src/PluginProcessor.h',
    'src/PluginProcessor.cpp',
    'src/PluginEditor.h',
    'src/PluginEditor.cpp',
    '.github/workflows/build-vst3.yml'
)
foreach ($f in $required) {
    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { throw "Missing required file: $f" }
}

$cm = Get-Content CMakeLists.txt -Raw
$ps = Get-Content src/PluginProcessor.cpp -Raw
$ph = Get-Content src/PluginProcessor.h -Raw
$ed = Get-Content src/PluginEditor.cpp -Raw
$wf = Get-Content .github/workflows/build-vst3.yml -Raw

$m = [regex]::Match(
    $ps,
    '(?s)void\s+PitchForgeAudioProcessor::processBlock\s*\([^)]*\)\s*\{(?<body>.*?)\n\}\s*\n\s*void\s+PitchForgeAudioProcessor::getStateInformation'
)
if (-not $m.Success) { throw 'Could not isolate processBlock for realtime-safety audit.' }
$processBody = $m.Groups['body'].Value
$wfNormalized = [regex]::Replace($wf, '\s+', ' ').Trim()

$checks = @(
    [pscustomobject]@{ Pass = ($cm -match 'cmake_minimum_required\(VERSION 3\.22\)'); Name = 'CMake minimum version is supported' },
    [pscustomobject]@{ Pass = ($cm -match 'GIT_REPOSITORY\s+https://github\.com/juce-framework/JUCE\.git'); Name = 'JUCE source is fetched from GitHub' },
    [pscustomobject]@{ Pass = ($cm -match 'GIT_TAG 8\.0\.8'); Name = 'JUCE 8.0.8 is pinned' },
    [pscustomobject]@{ Pass = ($cm -match 'GIT_REPOSITORY\s+https://github\.com/stenzek/soundtouch\.git'); Name = 'SoundTouch uses reachable GitHub source' },
    [pscustomobject]@{ Pass = ($cm -match 'GIT_TAG e83424d5928ab8513d2d082779c275765dee31b9'); Name = 'SoundTouch 2.3.3 commit is immutable-pinned' },
    [pscustomobject]@{ Pass = ($cm -match 'GIT_SHALLOW FALSE'); Name = 'SoundTouch immutable commit fetch is non-shallow' },
    [pscustomobject]@{ Pass = ($cm -notmatch 'www\.surina\.net'); Name = 'Unreliable SoundTouch tarball host is absent' },
    [pscustomobject]@{ Pass = ($cm -match 'VST3_CAN_REPLACE_VST2\s+FALSE'); Name = 'VST2 replacement guard is present' },
    [pscustomobject]@{ Pass = ($cm -match 'target_compile_definitions\(PitchForge_VST3 PRIVATE[^\n]*SOUNDTOUCH_FLOAT_SAMPLES=1'); Name = 'SoundTouch float sample mode is enabled for VST3' },
    [pscustomobject]@{ Pass = ($cm -match 'target_compile_features\(PitchForge PRIVATE cxx_std_17\)'); Name = 'C++17 is explicitly required' },
    [pscustomobject]@{ Pass = ($cm -match 'target_sources\(PitchForge PRIVATE src/PluginProcessor\.cpp src/PluginEditor\.cpp\)'); Name = 'Plugin source targets are wired' },
    [pscustomobject]@{ Pass = ($cm -match 'target_link_libraries\(PitchForge PRIVATE .*SoundTouch\)'); Name = 'SoundTouch is linked to plugin target' },
    [pscustomobject]@{ Pass = ($ps -notmatch 'soundtouch::SETTING_'); Name = 'SoundTouch setting macros are not namespace-qualified' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_USE_AA_FILTER,\s*1\)'); Name = 'SoundTouch AA filter setting is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_AA_FILTER_LENGTH,\s*64\)'); Name = 'SoundTouch AA filter length is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_USE_QUICKSEEK,\s*0\)'); Name = 'SoundTouch quickseek is explicitly disabled' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_SEQUENCE_MS,\s*sequenceMs\)'); Name = 'SoundTouch sequence setting is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_SEEKWINDOW_MS,\s*seekMs\)'); Name = 'SoundTouch seek-window setting is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.setSetting\(SETTING_OVERLAP_MS,\s*overlapMs\)'); Name = 'SoundTouch overlap setting is present' },
    [pscustomobject]@{ Pass = ($ps -match 'engine\.getSetting\(SETTING_INITIAL_LATENCY\)'); Name = 'SoundTouch initial latency query is present' },
    [pscustomobject]@{ Pass = ($ps -notmatch '(?s)AudioParameterFloat\([^;]*?,\s*[-+]?\d+(?:\.\d+)?f\s*,\s*[-+]?\d+(?:\.\d+)?f'); Name = 'All AudioParameterFloat ranges use JUCE 8 NormalisableRange' },
    [pscustomobject]@{ Pass = ($ed -notmatch 'textToValueFunction'); Name = 'JUCE Slider parser uses valueFromTextFunction' },
    [pscustomobject]@{ Pass = ($ed -match 'PathStrokeType\(4\.0f,\s*juce::PathStrokeType::curved,\s*juce::PathStrokeType::rounded\)'); Name = 'Needle stroke constructor uses valid JUCE signature' },
    [pscustomobject]@{ Pass = ($ps -notmatch 'SmoothPitchShifter'); Name = 'Legacy granular shifter is absent' },
    [pscustomobject]@{ Pass = ($ps -notmatch 'processOut\[\(size_t\)i \* 2\] = 0\.0f'); Name = 'No hard-zero wet-output fallback remains' },
    [pscustomobject]@{ Pass = ($processBody -match '(?s)if\s*\(i\s*>=\s*got\s*\).*?wetL\s*=\s*dryL.*?wetR\s*=\s*dryR'); Name = 'Wet underflow falls back to latency-aligned dry' },
    [pscustomobject]@{ Pass = ($ps -notmatch 'processOut\[\(size_t\)i \* 2\] = processIn\[\(size_t\)i \* 2\]'); Name = 'Wet underflow never copies undelayed input into wet path' },
    [pscustomobject]@{ Pass = ($processBody -notmatch '(?i)(new\s+\w+|delete\s+|std::malloc|std::free|\.resize\s*\(|\.assign\s*\(|std::vector\s*<[^>]+>\s+\w+\s*[;=]|std::string\s+\w+\s*[;=]|juce::String\s+\w+\s*[;=]|make_unique|make_shared|lock_guard|ScopedLock|CriticalSection|MessageManagerLock|Logger::|juce::File|std::cout|std::cerr)'); Name = 'processBlock has no obvious realtime-allocation/locking/I-O operations' },
    [pscustomobject]@{ Pass = ($processBody -match 'shifter\.putStereo\(processIn\.data\(\), frames\)'); Name = 'processBlock feeds the primary shifter' },
    [pscustomobject]@{ Pass = ($processBody -match 'shifter\.receiveStereo\(processOut\.data\(\), frames\)'); Name = 'processBlock drains the primary shifter' },
    [pscustomobject]@{ Pass = ($processBody -match 'const int got = shifter\.receiveStereo'); Name = 'Primary output availability is tracked' },
    [pscustomobject]@{ Pass = ($wf -match 'runs-on:\s*windows-2022'); Name = 'Windows 2022 runner is selected' },
    [pscustomobject]@{ Pass = ($wf -match 'uses:\s*actions/checkout@v6'); Name = 'Checkout action is present' },
    [pscustomobject]@{ Pass = ($wf -match 'uses:\s*actions/upload-artifact@v4'); Name = 'Artifact upload action is present' },
    [pscustomobject]@{ Pass = ($wf -match 'cmake -S \. -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release'); Name = 'CMake configure command is canonical' },
    [pscustomobject]@{ Pass = (([regex]::Matches($wfNormalized, 'cmake --build build --config Release --parallel 2')).Count -eq 1); Name = 'Exactly one canonical Windows build command is present' },
    [pscustomobject]@{ Pass = ($wfNormalized -notmatch 'cmake --build build --config Release --parallel 2 2'); Name = 'Malformed duplicate parallel argument is absent' },
    [pscustomobject]@{ Pass = ($wf -match 'Compress-Archive\s+-Path artifact/PitchForge\.vst3'); Name = 'VST3 artifact packaging step is present' },
    [pscustomobject]@{ Pass = ($wf -match 'if-no-files-found:\s*error'); Name = 'Artifact upload fails on missing package' },
    [pscustomobject]@{ Pass = ($wf -match 'PitchForge-v4\.3\.1-Windows-VST3\.zip'); Name = 'Final v4.3.1 artifact name is consistent' },
    [pscustomobject]@{ Pass = ($ph -match 'std::vector<float> processIn'); Name = 'Process buffers are preallocated members' },
    [pscustomobject]@{ Pass = ($ph -match 'std::vector<float> fifo'); Name = 'Shifter FIFO is a persistent member' },
    [pscustomobject]@{ Pass = ($wf -notmatch '\x60(?:r\x60n|n|r)'); Name = 'Workflow has no literal PowerShell newline escape text' }
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
