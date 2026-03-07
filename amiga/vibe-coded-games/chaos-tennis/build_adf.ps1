Param(
    [string]$Compiler = "vc",
    [string]$AdfName = "chaos_tennis.adf",
    [string]$ExeName = "CHAOSTENNIS"
)

$ErrorActionPreference = "Stop"

Write-Host "=== CHAOS TENNIS ADF build ==="

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

if (-not (Get-Command $Compiler -ErrorAction SilentlyContinue)) {
    Write-Error "C compiler '$Compiler' not found. Install vbcc and ensure 'vc' is on your PATH."
}

if (-not (Get-Command "xdftool" -ErrorAction SilentlyContinue)) {
    Write-Error "xdftool not found. Install fs-uae-tools (or equivalent) and ensure 'xdftool' is on your PATH."
}

Write-Host "Compiling game source..."
$src = ".\src\chaos_tennis.c"
if (-not (Test-Path $src)) {
    Write-Error "Source file '$src' not found."
}

& $Compiler "+aos68k" "-c99" "-o" $ExeName $src
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed (exit code $LASTEXITCODE)."
}

if (Test-Path $AdfName) {
    Write-Host "Removing existing ADF '$AdfName'..."
    Remove-Item $AdfName
}

Write-Host "Creating AmigaDOS disk image '$AdfName'..."
& xdftool $AdfName "format" "CHAOS_TENNIS"
if ($LASTEXITCODE -ne 0) {
    Write-Error "xdftool format failed."
}

Write-Host "Creating 's' directory on ADF..."
& xdftool $AdfName "makedir" "s"
if ($LASTEXITCODE -ne 0) {
    Write-Error "xdftool makedir s failed."
}

Write-Host "Copying executable to ADF..."
& xdftool $AdfName "write" $ExeName $ExeName
if ($LASTEXITCODE -ne 0) {
    Write-Error "xdftool write executable failed."
}

Write-Host "Copying startup-sequence to ADF..."
& xdftool $AdfName "write" ".\s-startup-sequence" "s/startup-sequence"
if ($LASTEXITCODE -ne 0) {
    Write-Error "xdftool write startup-sequence failed."
}

Write-Host "=== Done. Generated ADF: $AdfName ==="
Write-Host "Boot this disk image as DF0: in your Amiga emulator."

