# Powershell script for å bygge og kjøre alle tester i C
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host " Kompilerer og kjører C Bank Transfer Test Suite (Windows Native)" -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan

$BinDir = Join-Path $ProjectRoot "bin"
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null

$TestExe = Join-Path $BinDir "bank_tests.exe"
$Sources = @(
    "tests/main_test_runner.c",
    "src/account.c",
    "src/transfer.c",
    "src/csv_loader.c",
    "src/simulation.c"
)

Write-Host "`n1. Kompilerer C-kode med GCC (MinGW-w64 / Windows Native)..." -ForegroundColor Yellow
gcc -O2 -Iinclude $Sources -o $TestExe

Write-Host "   -> Kompilering OK ($TestExe)." -ForegroundColor Green

Write-Host "`n2. Kjører testprogram..." -ForegroundColor Yellow
& $TestExe

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nTester feilet med kode: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "`n=================================================================" -ForegroundColor Green
Write-Host " ALLE C-TESTER BESTÅTT SUKSESSFULLT!" -ForegroundColor Green
Write-Host "=================================================================" -ForegroundColor Green
