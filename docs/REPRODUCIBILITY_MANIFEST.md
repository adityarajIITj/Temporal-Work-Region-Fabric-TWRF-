# TWRF Reproducibility Manifest

## Canonical software state

Branch: `research/p0-semantic-hardening`

The branch is the research-validation line. The default branch should not be treated as the frozen research configuration unless it is explicitly synchronized to the same commit.

## Build

Linux/macOS-style:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/twrf_demo
python3 python/analysis/plot_break_even.py
```

Windows PowerShell:

```powershell
.\run.ps1
```

### Compiler and Toolchain Prerequisites

| Component | Minimum Version | Verified Configuration | Role |
| :--- | :--- | :--- | :--- |
| **C++ Standard** | C++20 | ISO/IEC 14882:2020 | Language standard for concepts and spans |
| **Compiler** | GCC 12+ / Clang 15+ / MSVC 19.34+ | GCC 14.2 (MinGW-w64 / MSYS2) | Host simulation compilation |
| **CMake** | 3.24+ | CMake 3.30.3 | Build system generator |
| **Python** | 3.9+ | Python 3.12 (Matplotlib, NumPy) | Benchmark telemetry plotting |

## Standard raster campaign

Frame: 128 x 128

Raster region: 16 x 16 pixels

Persistent regions: 64

Benchmark objects: 16

Mutation settings:

```
p_o = {0.00, 0.05, 0.10, 0.25, 0.50, 0.75, 1.00}
```

Locality:

- Clustered
- Dispersed

The experiment reports three distinct fractions:

$$
p_o = \frac{mutated\ objects}{objects},
\qquad
p_r = \frac{dirty\ TWRs}{TWRs},
\qquad
p_e = \frac{executed\ TWRs}{TWRs}.
$$

Timing comparisons use (p_e), not the requested mutation parameter.

## Baselines

A — full recomputation.

B — temporal-cache abstraction.

C — executable software incremental scheduler.

TWRF — hardware-oriented TWR scheduler.

B3 and TWRF receive the same scene construction, mutation event, TWR graph, kernels, and persistent-state semantics. Their management mechanisms differ.

## Correctness gates

The standard campaign requires:

$
ObservedMutableDependencies(TWR)\subseteq DeclaredDependencies(TWR)
$

and zero dependency-audit failures.

The raster oracle requires bitwise equality:

$
Output_{incremental}=Output_{full}.
$

The B3 parity gate requires:

[
E_{TWRF}=E_{B3}
]

and bitwise framebuffer equality for the paired raster result.

## Timing model

Cycle counts are calculated as:

$
C_{model}=\sum_j n_j c_j
$

where $n_j$ is an observed simulator operation count and $c_j$ is a declared timing parameter.

These cycles are modeled architectural values, not physical-GPU measurements.

## Sensitivity campaign

The final runner generates 135 settings:

- tile sizes: 8, 16, 32;
- hardware control-plane multiplier: 0.25, 0.50, 0.75, 1.00, 1.50, 2.00, 4.00, 8.00, 16.00;
- State Store latency multiplier: 0.50, 1.00, 2.00, 4.00, 8.00.

Each setting evaluates the full 14-case raster matrix.

Output:

`results/twrf_sensitivity.json`

## Artifact interpretation

`results/phase3_sweeps.json` and `results/twrf_sensitivity.json` are generated artifacts. They should be regenerated from the exact research branch used for a publication or thesis submission.

The repository must not treat a stale committed result file as proof that the current source has been executed. CI regenerates the files before validation.

## Hardware boundary

FPGA/RTL material is a feasibility study until RTL synthesis, place-and-route, timing closure, and board-level execution are performed.

No physical GPU performance, power, energy, or area claim is supported by the software simulator alone.
