# Publication Figures and Tables

## Figures

1. **TWRF architecture:** Input versions → validity engine → dependency manager → ready scheduler → TWR dispatcher → Raster/Ray/Neural execution → State Store.
2. **TWR lifecycle:** Create → Configure → Execute → Validate → Commit → Persist, with Dirty/Ready and Failed/Rollback paths.
3. **Temporal execution across frames:** unchanged regions reused while affected regions execute.
4. **Mutation versus execution:** p_o, p_r, and p_e for clustered/dispersed workloads.
5. **Modeled cost comparison:** TWRF, full recomputation, temporal cache, and B3 across the 14 cases.
6. **Locality effect:** clustered versus dispersed TWRF modeled cost.
7. **Sensitivity range:** B3/TWRF ratio over the 135-setting grid.
8. **Heterogeneous graph:** Raster → Ray → Neural with direct Raster → Neural and Ray → Neural edges.

## Tables

1. Research questions and evidence.
2. Experimental configuration.
3. Baseline definitions.
4. Correctness gates.
5. Clustered timing matrix.
6. Dispersed timing matrix.
7. Sensitivity configuration: 3 x 9 x 5 = 135 settings and 1,890 scenario evaluations.
8. Evidence classification.
9. Limitations and threats to validity.

## Quantitative figure rule

All quantitative figures must be generated from machine-readable experiment output. Do not manually transcribe plotted datasets. Every cycle-based caption must state that values are derived timing-model results, not physical GPU measurements.

## Assembly rule

Use consistent typography, axis conventions, units, decimal precision, and legend placement. The main paper should contain only figures required to follow the argument; detailed matrices belong in tables or appendices.
