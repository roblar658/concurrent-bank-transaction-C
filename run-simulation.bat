@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set ACCOUNTS_CSV=%1
if "%ACCOUNTS_CSV%"=="" set ACCOUNTS_CSV=data\accounts_100.csv

set TX_CSV=%2
if "%TX_CSV%"=="" set TX_CSV=data\transactions_adversarial_10k.csv

set WORKERS=%3
if "%WORKERS%"=="" set WORKERS=16

echo =================================================================
echo  Kjoerer C Bank Simulering med CSV-data (Windows CMD)
echo =================================================================
echo  Konto-fil        : %ACCOUNTS_CSV%
echo  Transaksjons-fil : %TX_CSV%
echo  Arbeidstraader   : %WORKERS%
echo.

if not exist bin mkdir bin

if not exist bin\bank_simulation.exe (
    echo Kompilerer simuleringsmotor...
    gcc -O2 -Iinclude src/main_simulation.c src/account.c src/transfer.c src/csv_loader.c src/simulation.c -o bin/bank_simulation.exe
    if %errorlevel% neq 0 (
        echo Kompileringsfeil!
        exit /b %errorlevel%
    )
)

bin\bank_simulation.exe "%ACCOUNTS_CSV%" "%TX_CSV%" %WORKERS%
