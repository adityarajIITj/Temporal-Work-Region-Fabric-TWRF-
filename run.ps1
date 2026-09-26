# ==============================================================================
# TWRF Virtual GPU Simulator: Automated Build, Test, GTA3 3D Demo & Benchmark
# ==============================================================================

Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host "   TEMPORAL WORK REGION FABRIC (TWRF) - 3D GTA3 VIRTUAL GPU RUNNER      " -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan

# 1. Ensure MSYS2 UCRT64 and CMake are on PATH
$env:PATH = "C:\msys64\ucrt64\bin;C:\Program Files\CMake\bin;" + $env:PATH

# 2. Build the project
Write-Host "`n[STEP 1/5] Building TWRF C++20 Project with CMake & Ninja..." -ForegroundColor Yellow
if (-not (Test-Path "build")) {
    cmake -B build -G Ninja
}
cmake --build build

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "[SUCCESS] Build completed successfully." -ForegroundColor Green

# 3. Run all acceptance tests (including 3D spatial binning)
Write-Host "`n[STEP 2/5] Running Full Acceptance Test Suite (36 Tests)..." -ForegroundColor Yellow
ctest --test-dir build --output-on-failure

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Acceptance tests failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "[SUCCESS] All 36 registered acceptance tests passed." -ForegroundColor Green

# 4. Run TWRF GTA3 3D Virtual GPU Benchmark
Write-Host "`n[STEP 3/5] Running TWRF-GTA3 3D Virtual GPU Benchmark (0% Host GPU)..." -ForegroundColor Yellow
.\build\twrf_city_3d.exe --bench 180

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Benchmark execution failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "[SUCCESS] Benchmark completed." -ForegroundColor Green

# 5. Generate plots and visualization
Write-Host "`n[STEP 4/5] Generating Performance and Speedup Telemetry Plots..." -ForegroundColor Yellow
python python/analysis/plot_gta3_results.py

# 6. Convert BMP preview to PNG
Write-Host "`n[STEP 5/5] Finalizing 3D Render Preview Artifacts..." -ForegroundColor Yellow
python -c "from PIL import Image; Image.open('results/gta3_city_preview.bmp').save('results/gta3_city_preview.png')"

Write-Host "`n========================================================================" -ForegroundColor Cyan
Write-Host "                     ALL TASKS COMPLETED SUCCESSFULLY                   " -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host "Generated GTA3 Artifacts:" -ForegroundColor White
Write-Host "  * Benchmark Telemetry : results/gta3_twrf_benchmark.json"
Write-Host "  * Speedup Plot        : results/gta3_twrf_speedup.png"
Write-Host "  * 3D City Preview     : results/gta3_city_preview.png"
Write-Host ""
Write-Host "Interactive Mode:" -ForegroundColor Yellow
Write-Host "  To drive through Liberty City in real-time:" -ForegroundColor White
Write-Host "    .\build\twrf_city_3d.exe" -ForegroundColor Green
Write-Host "  Controls: WASD/Arrows to drive, C to switch camera, T to toggle tile debug, ESC to quit."
Write-Host ""
