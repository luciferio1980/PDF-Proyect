@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -LiteralPath '%~dp0.' -Recurse -ErrorAction SilentlyContinue | Unblock-File -ErrorAction SilentlyContinue" >nul 2>&1
if not exist "%~dp0PDFForge.exe" (
    echo No se encontro PDFForge.exe.
    echo Extrae todo el zip a una carpeta e intenta otra vez.
    pause
    exit /b 1
)
if not exist "%~dp0vcruntime140.dll" (
    echo Falta vcruntime140.dll. Este zip esta incompleto.
    echo Descarga de nuevo PDFForge Alpha 0.2.8.
    pause
    exit /b 1
)
if not exist "%~dp0platforms\qwindows.dll" (
    echo Falta platforms\qwindows.dll. Extrae todo el zip, no solo el .exe.
    pause
    exit /b 1
)
start "" "%~dp0PDFForge.exe"
