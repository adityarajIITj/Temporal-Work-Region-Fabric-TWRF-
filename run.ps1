# ==============================================================================
# TWRF Virtual GPU Simulator: Automated Build, Test, Demo, DOOM & Plot Runner
# ==============================================================================

Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host "   TEMPORAL WORK REGION FABRIC (TWRF) - AUTOMATED PIPELINE RUNNER      " -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan

# 1. Ensure MSYS2 UCRT64 and CMake are on PATH
$env:PATH = "C:\msys64\ucrt64\bin;C:\Program Files\CMake\bin;" + $env:PATH

# 2. Build the project
Write-Host "`n[STEP 1/5] Building TWRF & DOOM Project with CMake & Ninja..." -ForegroundColor Yellow
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
Write-Host "`n[STEP 2/5] Running Full Acceptance Test Suite (35 Tests)..." -ForegroundColor Yellow
ctest --test-dir build --output-on-failure

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Acceptance tests failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "[SUCCESS] All 35 registered acceptance tests passed." -ForegroundColor Green

# 4. Run the comprehensive demonstration
Write-Host "`n[STEP 3/5] Running Comprehensive TWRF Virtual GPU Demo..." -ForegroundColor Yellow
.\build\twrf_demo.exe

# 5. Run TWRF-DOOM Benchmark
Write-Host "`n[STEP 4/5] Running TWRF-DOOM Benchmark (150-frame timedemo)..." -ForegroundColor Yellow
.\build\twrf_doom.exe --bench

# 6. Generate graphical curves and analysis
Write-Host "`n[STEP 5/5] Generating Scientific Plots with Python Analysis Suite..." -ForegroundColor Yellow
python python/analysis/plot_break_even.py
python python/analysis/plot_doom_results.py

Write-Host "`n========================================================================" -ForegroundColor Cyan
Write-Host "                     ALL TASKS COMPLETED SUCCESSFULLY                   " -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host "Generated Artifacts:" -ForegroundColor White
Write-Host "  * Core Sweeps Data:   results/phase3_sweeps.json"
Write-Host "  * Break-Even Plot:    results/break_even_curve.png"
Write-Host "  * DOOM Benchmark:     results/doom_twrf_benchmark.json"
Write-Host "  * DOOM Plot:          results/doom_twrf_benchmark.png"
Write-Host "  * DOOM Frame Captures:results/doom_frame*.png"
Write-Host ""
