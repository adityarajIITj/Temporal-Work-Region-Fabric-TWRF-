# TWRF Break-Even Cost Model

**Status:** Final analytical reference.

## 1. Single-region model

Let:

- (C_r>0): recomputation cost of a region;
- (C_t): management cost required to decide whether that region must execute;
- (p_ein[0,1]): fraction/probability of regions that execute after invalidation.

The simplified expected costs are:

[
C_{full}=C_r
]

and:

[
C_{TWRF}=C_t+p_eC_r.
]

TWRF is beneficial in this simplified model when:

[
C_t+p_eC_r<C_r
]

which gives:

[
\boxed{
p_e<1-\frac{C_t}{C_r}
}
]

The right-hand side is an analytical threshold, not a measured universal property of GPUs.

## 2. Why the old inverted expression is rejected

The expression

[
p>\frac{C_t}{C_r}
]

does not describe the TWRF break-even condition.

As (p_eightarrow1):

[
C_{TWRF}ightarrow C_t+C_r
]

while:

[
C_{full}=C_r.
]

For positive (C_t), the incremental scheme therefore carries an overhead tax in the fully dynamic regime.

The formal test suite keeps this algebraic correction as an explicit invariant.

## 3. The implemented model is richer

The simulator does not reduce the complete experiment to one (C_t) scalar.

It models:

[
C_{TWRF}=
C_{compute}
+C_{detect}
+C_{schedule}
+C_{dependency}
+C_{state}
+C_{scene}
+C_{interconnect}.
]

The corresponding software incremental baseline uses separately parameterized software control-plane costs.

The final cycle value is:

[
C_{model}=sum_j n_jc_j
]

where (n_j) is measured from the simulator and (c_j) is an explicit timing parameter.

## 4. Which volatility variable matters

The workload generator starts with an object mutation parameter:

[
p_o=
\frac{mutated objects}{objects}.
]

The fabric then produces:

[
p_r=
\frac{dirty regions}{regions}
]

and:

[
p_e=
\frac{executed regions}{regions}.
]

The practical break-even analysis should therefore use (p_e), because (p_e) is the quantity that determines how much recomputation is actually avoided.

A small (p_o) can still produce a large (p_e) when a dependency or spatial invalidation has broad fan-out.

## 5. Architectural interpretation

The useful design question is not:

> Does temporal reuse work?

It is:

[
\boxed{
	ext{When does the cost of managing persistent work become lower than the cost of repeating valid computation?}
}
]

For TWRF versus a software incremental implementation, the stronger comparison is:

[
C_{TWRF}<C_{B3}.
]

This isolates whether hardware-oriented management has enough advantage to justify architectural specialization.

## 6. Sensitivity requirements

A final evaluation should sweep:

- resource/version-check latency;
- spatial-check latency;
- scheduler operation latency;
- dependency-notification latency;
- State Store read/write latency;
- region granularity;
- State Store capacity;
- dependency depth;
- workload locality.

The resulting break-even surface is more informative than a single claimed crossover percentage.

## 7. Worst-case regime

At:

[
p_e=1
]

the simplified model becomes:

[
C_{TWRF}=C_t+C_r>C_r.
]

This regime is not hidden from the evaluation. The test suite explicitly checks that the modeled TWRF cost exceeds the zero-management full-recompute baseline when all regions execute.

The precise penalty is parameter-dependent.

## 8. Reproducibility

Every machine-readable sweep record should contain enough information to reconstruct the comparison:

- requested mutation parameter;
- locality;
- (p_o,p_r,p_e);
- executed/skipped counts;
- execution-set parity;
- output parity;
- dependency-audit failures;
- modeled cycle components;
- total modeled cycles.

This is the schema used by the final experiment runner.
