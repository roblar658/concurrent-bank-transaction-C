@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

echo =================================================================
echo  Kompilerer og kjoerer C Bank Transfer Test Suite (Windows CMD)
echo =================================================================

if not exist bin mkdir bin

echo.
echo 1. Kompilerer med GCC...
gcc -O2 -Iinclude tests/main_test_runner.c src/account.c src/transfer.c src/csv_loader.c src/simulation.c -o bin/bank_tests.exe
if %errorlevel% neq 0 (
    echo Kompileringsfeil!
    exit /b %errorlevel%
)

echo.
echo 2. Kjoerer testprogram...
bin\bank_tests.exe
if %errorlevel% neq 0 (
    echo Noen tester feilet!
    exit /b %errorlevel%
)

echo.
echo =================================================================
echo  ALLE C-TESTER BESTAATT SUKSESSFULLT!
echo =================================================================
