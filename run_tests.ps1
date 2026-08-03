# PowerShell script to automate build and regression testing for FatPanda Chess Engine

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host "         FatPanda Regression Test Suite Runner          " -ForegroundColor Cyan
Write-Host "=========================================================" -ForegroundColor Cyan

# Check if CMake is installed
if (-not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    Write-Error "CMake is required but not found in the environment path. Please install CMake."
    Exit 1
}

# Ensure build directory exists
if (-not (Test-Path "build")) {
    Write-Host "Creating build directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Write-Host "Configuring build using CMake..." -ForegroundColor Yellow
cmake -B build -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed."
    Exit $LASTEXITCODE
}

Write-Host "Building target FatPandaTests in Release mode..." -ForegroundColor Yellow
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
    Exit $LASTEXITCODE
}

Write-Host "Running tests..." -ForegroundColor Yellow
& ".\build\tests\Release\FatPandaTests.exe"
$testResult = $LASTEXITCODE

if ($testResult -eq 0) {
    Write-Host "`nAll tests PASSED successfully! No regressions detected." -ForegroundColor Green
} else {
    Write-Host "`nSome tests FAILED! Please inspect the test logs above." -ForegroundColor Red
}

Exit $testResult
