# ==============================================================================
# TWRF Virtual GPU Simulator: Automated Build, Test, Demo & Plot Runner
# ==============================================================================

Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host "   TEMPORAL WORK REGION FABRIC (TWRF) - AUTOMATED PIPELINE RUNNER      " -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan

# 1. Ensure MSYS2 UCRT64 and CMake are on PATH
$env:PATH = "C:\msys64\ucrt64\bin;C:\Program Files\CMake\bin;" + $env:PATH

# 2. Build the project
Write-Host "`n[STEP 1/4] Building TWRF C++20 Project with CMake & Ninja..." -ForegroundColor Yellow
if (-not (Test-Path "build")) {
    cmake -B build -G Ninja
}
cmake --build build

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "[SUCCESS] Build completed successfully." -ForegroundColor Green

# 3. Run all 35 acceptance tests
Write-Host "`n[STEP 2/4] Running Full Acceptance Test Suite (35 Tests)..." -ForegroundColor Yellow
ctest --test-dir build --output-on-failure

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Acceptance tests failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "[SUCCESS] All 35 registered acceptance tests passed." -ForegroundColor Green

# 4. Run the comprehensive demonstration
Write-Host "`n[STEP 3/4] Running Comprehensive TWRF Virtual GPU Demo..." -ForegroundColor Yellow
.\build\twrf_demo.exe

# 5. Generate break-even graphical curves
Write-Host "`n[STEP 4/4] Generating Break-Even Curves with Python Analysis Suite..." -ForegroundColor Yellow
python python/analysis/plot_break_even.py

Write-Host "`n========================================================================" -ForegroundColor Cyan
Write-Host "                     ALL TASKS COMPLETED SUCCESSFULLY                   " -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host "Generated Artifacts:" -ForegroundColor White
Write-Host "  * Experimental Data:  results/phase3_sweeps.json"
Write-Host "  * Sensitivity Data:   results/twrf_sensitivity.json"
Write-Host "  * Architectural Plot: results/twrf_architectural_comparison.png"
Write-Host "  * Specifications:     docs/"
Write-Host ""
