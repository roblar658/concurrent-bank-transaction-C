# Powershell script for å bygge og kjøre CSV-simuleringer i C
param (
    [string]$AccountsCsv = "data/accounts_100.csv",
    [string]$TransactionsCsv = "data/transactions_adversarial_10k.csv",
    [int]$Workers = 16
)

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot

Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host " Bygger og kjører C Bank Simulering med CSV-data (Windows Native)" -ForegroundColor Cyan
Write-Host "=================================================================" -ForegroundColor Cyan
Write-Host "  Konto-fil        : $AccountsCsv"
Write-Host "  Transaksjons-fil : $TransactionsCsv"
Write-Host "  Arbeidstråder    : $Workers"

$BinDir = Join-Path $ProjectRoot "bin"
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null
$SimExe = Join-Path $BinDir "bank_simulation.exe"

$Sources = @(
    "src/main_simulation.c",
    "src/account.c",
    "src/transfer.c",
    "src/csv_loader.c",
    "src/simulation.c"
)

# Kompiler simuleringsmotor
Write-Host "`nKompilerer simuleringsmotor med GCC..." -ForegroundColor Yellow
gcc -O2 -Iinclude $Sources -o $SimExe

Write-Host "`nKjører simulering..." -ForegroundColor Green
& $SimExe $AccountsCsv $TransactionsCsv $Workers
