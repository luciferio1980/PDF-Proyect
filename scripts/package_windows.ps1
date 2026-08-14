# Builds a portable Windows folder: unzip and double-click PDFForge.exe
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"

$exeCandidates = @(
    (Join-Path $BuildDir $Config "PDFForge.exe"),
    (Join-Path $BuildDir "PDFForge.exe")
)
$exe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
    throw "PDFForge.exe not found under $BuildDir"
}

$pdfiumCandidates = @(
    (Join-Path (Split-Path $exe -Parent) "pdfium.dll"),
    (Join-Path $BuildDir $Config "pdfium.dll"),
    (Join-Path $BuildDir "pdfium.dll"),
    (Join-Path $BuildDir "third_party\pdfium\bin\pdfium.dll")
)
$pdfium = $pdfiumCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $pdfium) {
    throw "pdfium.dll not found"
}

if (Test-Path $OutDir) {
    Remove-Item -Recurse -Force $OutDir
}
New-Item -ItemType Directory -Path $OutDir | Out-Null

Copy-Item $exe (Join-Path $OutDir "PDFForge.exe")
Copy-Item $pdfium (Join-Path $OutDir "pdfium.dll")

$windeployqt = $null
if ($env:QT_ROOT_DIR) {
    $candidate = Join-Path $env:QT_ROOT_DIR "bin\windeployqt.exe"
    if (Test-Path $candidate) { $windeployqt = $candidate }
}
if (-not $windeployqt) {
    $windeployqt = Get-Command windeployqt -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}
if (-not $windeployqt) {
    throw "windeployqt.exe not found. Install Qt and add it to PATH."
}

& $windeployqt --release --compiler-runtime (Join-Path $OutDir "PDFForge.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

$samplesSrc = Join-Path $BuildDir "test_documents"
$samplesDst = Join-Path $OutDir "test_documents"
if (Test-Path $samplesSrc) {
    Copy-Item -Recurse $samplesSrc $samplesDst
}

Copy-Item (Join-Path $PSScriptRoot "..\resources\portable\LEEME.txt") (Join-Path $OutDir "LEEME.txt")
Copy-Item (Join-Path $PSScriptRoot "..\resources\portable\Abrir PDFForge.bat") (Join-Path $OutDir "Abrir PDFForge.bat")

Write-Host "Portable package ready: $OutDir"
