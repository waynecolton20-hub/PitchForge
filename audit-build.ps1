$ErrorActionPreference = 'Stop'
Write-Host 'PITCHFORGE SOURCE AUDIT'
$required = @('CMakeLists.txt','src/PluginProcessor.h','src/PluginProcessor.cpp','src/PluginEditor.h','src/PluginEditor.cpp')
foreach ($f in $required) { if (-not (Test-Path $f)) { throw "Missing required file: $f" } }
$cm = Get-Content CMakeLists.txt -Raw
$ps = Get-Content src/PluginProcessor.cpp -Raw
$ed = Get-Content src/PluginEditor.cpp -Raw
$wf = Get-Content .github/workflows/build-vst3.yml -Raw -ErrorAction SilentlyContinue
$checks = @(
    @($ps -notmatch 'soundtouch::SETTING_', 'SoundTouch setting macros must not be namespace-qualified'),
    @($ed -notmatch 'textToValueFunction', 'JUCE Slider value parser must use valueFromTextFunction'),
    @($ed -match 'PathStrokeType\(4\.0f,\s*juce::PathStrokeType::curved,\s*juce::PathStrokeType::rounded\)', 'Needle stroke signature must be explicit'),
    @($ps -notmatch 'processOut\[\(size_t\)i \* 2\] = 0\.0f', 'No zero-fill fallback in wet output'),
    @($wf -match 'cmake --build build --config Release --parallel 2\r?\n', 'Windows build command must be canonical'),
    @($wf -notmatch 'cmake --build build --config Release --parallel 2 2', 'Malformed duplicate parallel argument absent'),
    @($cm -match 'VST3_CAN_REPLACE_VST2\s+FALSE', 'VST2 replacement guard present'),
    @($cm -match 'soundtouch-2\.3\.3', 'SoundTouch 2.3.3 pinned'),
    @($cm -match 'DOWNLOAD_EXTRACT_TIMESTAMP TRUE', 'Deterministic archive extraction enabled')
)
foreach ($c in $checks) { if (-not $c[0]) { throw "AUDIT FAIL: $($c[1])" } }
Write-Host 'STATIC AUDIT: PASS'
