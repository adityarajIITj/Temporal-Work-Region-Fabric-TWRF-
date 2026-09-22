# Break-Even Cost Model & Formal Correction

**Scope:** Analytical cost foundations for Temporal Work Region Fabric (TWRF)  
**Status:** Mandatory Reference Standard for all simulator sweeps and tests

---

## 1. The Break-Even Model

Let:
- $C_r \in \mathbb{R}^+$: The computational cost (cycles/energy) to fully recompute a work region from scratch.
- $C_t \in \mathbb{R}^+$: The tracking, scheduling, and validation overhead required by TWRF to verify whether a region is clean or dirty.
- $p \in [0, 1]$: The probability that an individual region's inputs or dependencies have mutated during a given frame step (the **rate of change** or scene volatility).

### 1.1 Expected Cost Formulations
* **Full Recompute Baseline ($E[\text{Full}]$)**:
  Every frame executes the work region unconditionally:
  $$E[\text{Full}] = C_r$$

* **TWRF Execution Model ($E[\text{TWRF}]$)**:
  TWRF always incurs the tracking overhead $C_t$. If the region has changed (with probability $p$), it must also pay the full recomputation cost $C_r$. If the region is clean (with probability $1 - p$), it pays $0$ recomputation cost:
  $$E[\text{TWRF}] = C_t + p \cdot C_r + (1 - p) \cdot 0 = C_t + p \cdot C_r$$

### 1.2 Derivation of the Benefit Threshold
TWRF achieves net performance benefit over full recomputation if and only if:
$$E[\text{TWRF}] < E[\text{Full}]$$
$$C_t + p \cdot C_r < C_r$$
$$C_t < C_r - p \cdot C_r$$
$$C_t < (1 - p) \cdot C_r$$
Dividing both sides by $C_r$ (since $C_r > 0$):
$$\frac{C_t}{C_r} < 1 - p \implies p < 1 - \frac{C_t}{C_r}$$

---

## 2. Rejection of the Inverted Formula

In earlier exploratory notes (such as Part 14.1 in early research memos), an algebraic error produced:
$$C_t < p \cdot C_r \implies p > \frac{C_t}{C_r} \quad \text{[REJECTED / INVALID]}$$

### Why the Old Formula Is Physically Absurd
* If $p > C_t / C_r$, as scene dynamics increase ($p \to 1$), TWRF would supposedly become *more* advantageous.
* In physical reality, when $p = 1$ (100% of regions change), TWRF performs the exact same recomputation work as baseline ($C_r$) **plus** the tracking overhead ($C_t$), yielding a total cost of $C_r + C_t > C_r$. TWRF strictly loses.
* Under the correct formula $p < 1 - C_t / C_r$:
  - When $C_t \ll C_r$ (e.g. $C_t / C_r = 0.01$), TWRF wins whenever change is below $99\%$ ($p < 0.99$).
  - When $C_t / C_r = 0.20$, TWRF wins only when change is below $80\%$ ($p < 0.80$).
  - When $C_t \ge C_r$, $1 - C_t / C_r \le 0$, meaning TWRF can **never** win for any non-negative change rate $p$.

**Test Invariant S1-08 enforces this mathematical property across all test suites.**
