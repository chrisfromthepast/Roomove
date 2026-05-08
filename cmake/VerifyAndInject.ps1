param(
    [Parameter(Mandatory = $false)]
    [string]$BuildNativeDir = "build_native",
    [Parameter(Mandatory = $false)]
    [string]$BuildDspDir = "build_dsp",
    [Parameter(Mandatory = $false)]
    [string]$DspLibraryName = "libRoomove_DSP.a",
    [Parameter(Mandatory = $false)]
    [string]$PageTablePath = "Resources/Roomove_PageTable.xml",
    [Parameter(Mandatory = $false)]
    [string]$DspEnabled = $null
)

$ErrorActionPreference = "Stop"

try {
    $Bundle = Get-ChildItem -Path $BuildNativeDir -Filter "Roomove.aaxplugin" -Recurse -Directory | Select-Object -First 1
    if (-not $Bundle) {
        Write-Error "CRITICAL: Bundle folder 'Roomove.aaxplugin' not found in '$BuildNativeDir'."
        exit 1
    }

    $BundlePath = $Bundle.FullName
    $ResDir = Join-Path $BundlePath "Contents\Resources"
    $Win64Dir = Join-Path $BundlePath "Contents\Win64"
    $X64Dir = Join-Path $BundlePath "Contents\x64"

    $Binary = $null
    if (Test-Path $Win64Dir) {
        $Binary = Get-ChildItem -Path $Win64Dir -Filter "*.aaxplugin" -File -ErrorAction SilentlyContinue | Select-Object -First 1
    }
    if (-not $Binary -and (Test-Path $X64Dir)) {
        $Binary = Get-ChildItem -Path $X64Dir -Filter "*.aaxplugin" -File -ErrorAction SilentlyContinue | Select-Object -First 1
    }

    if (-not $Binary) {
        Write-Host "##[warning] No binary found in Contents\\Win64 or Contents\\x64. Checking build artifacts..."
        $Fallback = Get-ChildItem -Path $BuildNativeDir -Filter "*.aaxplugin" -Recurse -File |
            Where-Object { $_.FullName -notmatch "[\\/]Contents[\\/]" } |
            Select-Object -First 1
        if ($Fallback) {
            Write-Host "Found fallback binary at $($Fallback.FullName). Copying to Contents\\Win64..."
            if (-not (Test-Path $Win64Dir)) {
                New-Item -ItemType Directory -Force -Path $Win64Dir | Out-Null
            }
            Copy-Item -Path $Fallback.FullName -Destination $Win64Dir -Force
            $Binary = Get-ChildItem -Path $Win64Dir -Filter "*.aaxplugin" -File -ErrorAction SilentlyContinue | Select-Object -First 1
        }
    }

    if (-not $Binary) {
        Write-Error "CRITICAL FAILURE: No native AAX binary found in bundle or fallback artifacts."
        exit 1
    }

    Write-Host "Found Binary: $($Binary.FullName)"

    if (-not (Test-Path $ResDir)) {
        New-Item -ItemType Directory -Force -Path $ResDir | Out-Null
    }

    if ($null -eq $DspEnabled -or $DspEnabled -eq "") {
        $DspEnabled = $env:DSP_ENABLED
    }

    $DspLib = Join-Path $BuildDspDir $DspLibraryName
    if ($DspEnabled -eq "true" -and (Test-Path $DspLib)) {
        Copy-Item -Path $DspLib -Destination (Join-Path $ResDir "Roomove_DSP.a") -Force
        Write-Host "DSP Injected."
    } elseif ($DspEnabled -eq "true") {
        Write-Host "##[warning] DSP is enabled but '$DspLib' was not found. Skipping DSP injection."
    }

    if (Test-Path $PageTablePath) {
        Copy-Item -Path $PageTablePath -Destination $ResDir -Force
        Write-Host "Page Table Injected."
    } else {
        Write-Host "##[warning] Page table '$PageTablePath' not found. S6L page-table injection skipped."
    }

    Write-Host "Verification and injection completed for $BundlePath"
}
catch {
    Write-Error "VerifyAndInject failed: $($_.Exception.Message)"
    exit 1
}
