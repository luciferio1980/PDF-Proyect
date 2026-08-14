# Builds a portable Windows folder: unzip and double-click PDFForge.exe
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string]$Config = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"

function Find-Vc143Crt {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $hits = & $vswhere -latest -products * -prerelease -find "VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT\vcruntime140.dll"
        if ($hits) {
            return Split-Path -Parent ($hits | Select-Object -Last 1)
        }
    }

    $crtRoots = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC"
    )
    foreach ($root in $crtRoots) {
        if (-not (Test-Path $root)) { continue }
        $crtDir = Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
            ForEach-Object { Join-Path $_.FullName "x64\Microsoft.VC143.CRT" } |
            Where-Object { Test-Path (Join-Path $_ "vcruntime140.dll") } |
            Select-Object -Last 1
        if ($crtDir) { return $crtDir }
    }
    return $null
}

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

& $windeployqt --release --compiler-runtime --verbose 1 (Join-Path $OutDir "PDFForge.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

# windeployqt --compiler-runtime often skips CRT when Qt was built with a
# different MSVC than the runner. Copy the VS 2022 redistributable DLLs so the
# zip runs on machines that never installed the Visual C++ runtime.
$crtDir = Find-Vc143Crt
if ($crtDir) {
    Copy-Item (Join-Path $crtDir "*.dll") $OutDir -Force
    Write-Host "Copied VC++ runtime from $crtDir"
} else {
    Write-Warning "VS redist folder not found; copying CRT DLLs from System32"
    $sys32 = Join-Path $env:SystemRoot "System32"
    foreach ($name in @(
            "vcruntime140.dll",
            "vcruntime140_1.dll",
            "msvcp140.dll",
            "msvcp140_1.dll",
            "msvcp140_2.dll",
            "msvcp140_atomic_wait.dll",
            "concrt140.dll",
            "vccorlib140.dll"
        )) {
        $src = Join-Path $sys32 $name
        if (Test-Path $src) {
            Copy-Item $src $OutDir -Force
        }
    }
}

if ($env:QT_ROOT_DIR) {
    $sw = Join-Path $env:QT_ROOT_DIR "bin\opengl32sw.dll"
    if (Test-Path $sw) {
        Copy-Item $sw $OutDir -Force
    }
}

$samplesSrc = Join-Path $BuildDir "test_documents"
$samplesDst = Join-Path $OutDir "test_documents"
if (Test-Path $samplesSrc) {
    Copy-Item -Recurse $samplesSrc $samplesDst
}

$portable = Join-Path $PSScriptRoot "..\resources\portable"
Copy-Item (Join-Path $portable "LEEME.txt") (Join-Path $OutDir "LEEME.txt")
Copy-Item (Join-Path $portable "Abrir PDFForge.bat") (Join-Path $OutDir "Abrir PDFForge.bat")
Copy-Item (Join-Path $portable "qt.conf") (Join-Path $OutDir "qt.conf")

$required = @(
    (Join-Path $OutDir "PDFForge.exe"),
    (Join-Path $OutDir "pdfium.dll"),
    (Join-Path $OutDir "Qt6Core.dll"),
    (Join-Path $OutDir "Qt6Gui.dll"),
    (Join-Path $OutDir "Qt6Widgets.dll"),
    (Join-Path $OutDir "platforms\qwindows.dll"),
    (Join-Path $OutDir "vcruntime140.dll"),
    (Join-Path $OutDir "vcruntime140_1.dll"),
    (Join-Path $OutDir "msvcp140.dll"),
    (Join-Path $OutDir "qt.conf")
)
foreach ($path in $required) {
    if (-not (Test-Path $path)) {
        throw "Packaging incomplete: missing $path"
    }
}

Write-Host "Portable package ready: $OutDir"
Get-ChildItem $OutDir -File | Select-Object Name, Length | Format-Table
Get-ChildItem (Join-Path $OutDir "platforms") -File | Select-Object Name, Length | Format-Table
