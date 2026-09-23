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
.un.ps1
```

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

[
p_o=rac{mutated objects}{objects},
qquad
p_r=rac{dirty TWRs}{TWRs},
qquad
p_e=rac{executed TWRs}{TWRs}.
]

Timing comparisons use (p_e), not the requested mutation parameter.

## Baselines

A — full recomputation.

B — temporal-cache abstraction.

C — executable software incremental scheduler.

TWRF — hardware-oriented TWR scheduler.

B3 and TWRF receive the same scene construction, mutation event, TWR graph, kernels, and persistent-state semantics. Their management mechanisms differ.

## Correctness gates

The standard campaign requires:

[
ObservedMutableDependencies(TWR)subseteq DeclaredDependencies(TWR)
]

and zero dependency-audit failures.

The raster oracle requires bitwise equality:

[
Output_{incremental}=Output_{full}.
]

The B3 parity gate requires:

[
E_{TWRF}=E_{B3}
]

and bitwise framebuffer equality for the paired raster result.

## Timing model

Cycle counts are calculated as:

[
C_{model}=sum_j n_jc_j
]

where (n_j) is an observed simulator operation count and (c_j) is a declared timing parameter.

These cycles are modeled architectural values, not physical-GPU measurements.

## Sensitivity campaign

The final runner generates 54 settings:

- tile sizes: 8, 16, 32;
- hardware control-plane multiplier: 0.25, 0.50, 0.75, 1.00, 1.50, 2.00;
- State Store latency multiplier: 0.50, 1.00, 2.00.

Each setting evaluates the full 14-case raster matrix.

Output:

`results/twrf_sensitivity.json`

## Artifact interpretation

`results/phase3_sweeps.json` and `results/twrf_sensitivity.json` are generated artifacts. They should be regenerated from the exact research branch used for a publication or thesis submission.

The repository must not treat a stale committed result file as proof that the current source has been executed. CI regenerates the files before validation.

## Hardware boundary

FPGA/RTL material is a feasibility study until RTL synthesis, place-and-route, timing closure, and board-level execution are performed.

No physical GPU performance, power, energy, or area claim is supported by the software simulator alone.
