# TWRF FPGA / RTL Feasibility Memo

This document defines a proposed minimum hardware subset and an architectural mapping for a future FPGA/RTL prototype of TWRF. It is a feasibility study, not a synthesis report.

---

## 1. Prototype Scope & Verification Guardrail

In accordance with the project contract:
- Software simulator results **do not** constitute physical silicon or timing closure proof.
- This memo defines the smallest demonstrable hardware subset that preserves the central TWRF execution model without requiring commercial-scale GPU silicon area.
- The goal is an eventual RTL prototype on modern FPGA hardware (e.g., AMD Xilinx UltraScale+ or Intel Agilex) to validate hardware-managed temporal scheduling.

---

## 2. Minimum Demonstrable Synthesizable Subset

An initial prototype does not require full 4K rasterization or deep neural networks. A minimal demonstrable prototype consists of a **Single-Tile TWR Fabric Core**:

```
+-------------------------------------------------------------------------+
|                        TWRF PROTOTYPE FABRIC CORE                       |
|                                                                         |
|  +-----------------------+                    +----------------------+  |
|  |  TWR Metadata Store   |                    |  Logical State Store |  |
|  |  (Status, Versions,   |                    |  (Dual-Port URAM/    |  |
|  |   Dependencies, Depth)|                    |   BRAM Cache Bank)   |  |
|  +-----------+-----------+                    +-----------+----------+  |
|              |                                            |             |
|              v                                            v             |
|  +-----------------------+                    +----------------------+  |
|  | Change Detection &    |   Ready Item Bus   |  Tile Execution      |  |
|  | Ready Queue Logic     +------------------->|  Unit (TEU)          |  |
|  | (Comparator + Priority|                    |  (Raster/Ray/Neural  |  |
|  |  Scoreboard)          |                    |   Datapath ALUs)     |  |
|  +-----------------------+                    +-----------+----------+  |
|                                                           |             |
|  +--------------------------------------------------------+----------+  |
|  | Lightweight AXI4-Stream / Credit-Based Ring Interconnect          |  |
+--+-------------------------------------------------------------------+--+
```

### Key Hardware Modules

1. **TWR Metadata Table**:
   - Implemented in Distributed RAM / Block RAM.
   - Stores 64 entries (for 64 tiles/batches).
   - Each entry contains: Status (2 bits), Current Output Version (16 bits), Priority (8 bits), Topological Depth (8 bits), Upstream Dependency Count (4 bits).
2. **Change Tracker & Dependency Engine**:
   - Hardware scoreboard monitoring input resource version bumps.
   - When a resource version advances, parallel comparators mark affected TWR IDs dirty.
   - When all dirty upstream dependencies decrement to zero, the ID is enqueued into the hardware Ready Queue.
3. **Hardware Ready Queue**:
   - Dual-ported BRAM with prioritized insertion (ordering by priority, topological depth, and ID).
4. **Logical State Store Memory Bank**:
   - 1 MB of banked UltraRAM / BRAM configured as dual-port memory.
   - Port A: Write port for active TEU output payloads.
   - Port B: Read port for downstream consumers and display raster scanout.
5. **Tile Execution Unit (TEU)**:
   - Unified fixed-point SIMD ALU (16 lanes).
   - Supports:
     - 2D barycentric edge evaluation for raster tiles.
     - Analytical ray-sphere/AABB intersection for ray batches.
     - Matrix-multiply-accumulate (MMA) for MLP neural denoising.

---

## 3. Illustrative Resource Estimate (Not Synthesized)

The numerical values below are design estimates retained for feasibility discussion. No RTL synthesis, place-and-route, timing closure, or hardware execution has been performed in this repository.

Illustrative prototype parameters:
- Operating Frequency Target: **200 MHz**
- Screen Configuration: $128 \times 128$ resolution, $16 \times 16$ tile granularity (64 total tiles).
- State Store: 1 MB On-Chip Memory (512 KB color/depth + 256 KB ray buffers + 256 KB neural weights/states).

| Hardware Module | Estimated LUTs | Estimated Flip-Flops | DSP Slices | Block RAM (36Kb) | UltraRAM (288Kb) |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **TWR Metadata & Scoreboard** | 4,200 | 3,800 | 0 | 4 | 0 |
| **Change Tracker & Ready Queue** | 5,500 | 4,200 | 0 | 2 | 0 |
| **Tile Execution Unit (16-lane SIMD)**| 18,000 | 14,500 | 64 | 8 | 0 |
| **Logical State Store (1 MB)** | 2,100 | 1,200 | 0 | 16 | 24 |
| **Lightweight Interconnect / AXI Bridge**| 3,500 | 3,100 | 0 | 2 | 0 |
| **Total Estimated Fabric Footprint** | **33,300** | **26,800** | **64** | **32** | **24** |
| **ZCU102 Available Resources** | 274,080 | 548,160 | 2,520 | 912 | 0 (or BRAM equiv) |
| **Resource Utilization Fraction** | **~12.1%** | **~4.9%** | **~2.5%** | **~3.5%** | Minimal |

---

## 4. Hardware Verification Strategy & Shared Testbenches

To ensure absolute fidelity between software simulation and physical hardware:
1. **Shared Test Vector Generators**:
   - The C++ simulator exports golden input sequences, version updates, and expected State Store buffers to standard `.hex` / `.dat` memory files.
2. **SystemVerilog Testbench Replay**:
   - An RTL testbench instantiates the synthesizable modules, reads the exported vectors, drives clock cycles, and verifies that output versions and memory state match the C++ simulator bit-for-bit.
3. **Acceptance Test Migration**:
   - S1-01 (Lifecycle), S1-03 (Dependency Ordering), S4-01 (Ray Batch), and S4-04 (Cross-Workload Propagation) map directly to RTL assertions.
